"""
CSP RPC client implementation in Python.

This module provides a client implementation for the CSP RPC protocol, allowing users to interact with RPC servers 
using Python. It defines classes for handling RPC headers, calls, replies, procedures, and programs, as well as a 
main RpcClient class for managing connections and fetching procedures from the server.
"""

from typing import Any, Iterable, Tuple
import struct
from typing import NamedTuple
import libcsp_py3 as csp


# CSP RPC packing format to Python struct packing format:
# - f: float -> f
# - d: double -> d
# - b: signed byte -> b
# - B: unsigned byte -> B
# - h: int16_t -> h
# - H: uint16_t -> H
# - l: int32_t -> l
# - L: uint32_t -> L
# - q: int64_t -> q
# - Q: uint64_t -> Q
# - s: char[] -> s (terminating 0-byte is present)
# - p: length as uint16_t prefix followed by data bytes -> custom


def unpack_cstring(data: bytes) -> Tuple[str, int]:
    if data:
        len = data.index(0)
        fmt_str = f"{len}s"
        res = struct.unpack_from(fmt_str, data)[0]
        return res.decode(), len + 1
    return ("", 0)


class RpcHeader(NamedTuple):
    type: int
    version: int
    xid: int

    def to_network(self) -> bytes:
        return struct.pack(">bbI", self.type, self.version, self.xid)

    @staticmethod
    def from_nw(data: bytes) -> "RpcHeader":
        type, version, xid = struct.unpack_from(">bbI", data)
        return RpcHeader(type, version, xid)


class RpcCall(NamedTuple):
    program: int
    procedure: int
    data: bytes

    def to_network(self) -> bytes:
        return struct.pack(">IIH", self.program, self.procedure, len(self.data))


class RpcReply(NamedTuple):
    amount: int
    idx: int
    data: bytes
    header: RpcHeader

    @staticmethod
    def from_nw(data: bytes) -> "RpcReply":
        header = RpcHeader.from_nw(data)
        amount, idx = struct.unpack_from(">II", data, 6)
        res = RpcReply(amount=amount, idx=idx, data=data[16:], header=header)
        return res


class RpcProcedure(NamedTuple):
    result: int
    name: str
    program_id: int
    proc_id: int
    proc_name: str
    arg_fmt: str
    res_fmt: str
    client: "RpcClient"

    @staticmethod
    def from_nw(client: "RpcClient", data: bytes) -> "RpcProcedure":
        result, program_id = struct.unpack_from(">iI", data)
        name, name_len = unpack_cstring(data[8:])
        idx = 8 + name_len
        proc_id = struct.unpack_from(">I", data, idx)[0]
        idx += 4
        proc_name, name_len = unpack_cstring(data[idx:])
        idx += name_len
        arg_fmt, name_len = unpack_cstring(data[idx:])
        idx += name_len
        res_fmt, name_len = unpack_cstring(data[idx:])
        res = RpcProcedure(
            result=result,
            program_id=program_id,
            name=name,
            proc_id=proc_id,
            proc_name=proc_name,
            arg_fmt=arg_fmt,
            res_fmt=res_fmt,
            client=client,
        )
        return res

    def prepare_call(self, *args) -> bytes:
        packed_args = b""
        for idx, a in enumerate(args):
            # Easy cases first:
            if self.arg_fmt[idx] not in ["s", "p"]:
                packed_args += struct.pack(f">{self.arg_fmt[idx]}", a)
            elif self.arg_fmt[idx] == "s":
                pack_fmt = f"{len(a)}s"
                packed_args += struct.pack(f">{pack_fmt}", a.encode())
                packed_args += b"\x00"
            elif self.arg_fmt[idx] == "p":
                packed_args += struct.pack(">H", len(a))
                packed_args += a
        return packed_args

    def to_nw(self, *args) -> bytes:
        if len(args) != len(self.arg_fmt):
            raise TypeError(
                f"{self.proc_name} takes {len(self.arg_fmt)} arguments, {len(args)} given!"
            )
        call_data = self.prepare_call(*args)
        data = RpcHeader(
            type=RpcClient.RPC_MSG_CALL, version=RpcClient.RPC_VERSION, xid=0x1234
        ).to_network()
        data += RpcCall(
            data=call_data, program=self.program_id, procedure=self.proc_id
        ).to_network()
        data += call_data
        return data

    def __call__(self, *args) -> Any:
        if len(args) != len(self.arg_fmt):
            raise TypeError(
                f"{self.proc_name} takes {len(self.arg_fmt)} arguments, {len(args)} given!"
            )
        call_data = self.prepare_call(*args)
        connection = csp.connect(
            csp.CSP_PRIO_NORM, self.client.server, 9, self.client.settings.get("timeout", 1000), csp.CSP_O_RDP
        )
        data = RpcHeader(
            type=RpcClient.RPC_MSG_CALL, version=RpcClient.RPC_VERSION, xid=0x1234
        ).to_network()
        data += RpcCall(
            data=call_data, program=self.program_id, procedure=self.proc_id
        ).to_network()
        data += call_data
        request = csp.buffer_get(0)
        csp.packet_set_data(request, data)
        csp.send(connection, request)
        result = []
        while True:
            reply = csp.read(connection, self.client.settings.get("timeout", 1000))
            if reply:
                reply = csp.packet_get_data(reply)
                header = RpcHeader.from_nw(reply)
                reply = RpcReply.from_nw(reply)
                if header.xid == reply.header.xid:
                    result += self.unpack_result(reply)
                    if reply.idx == reply.amount - 1:
                        break
                else:
                    break
            else:
                break
        return result

    def unpack_result(self, reply: RpcReply) -> Iterable:
        result = []
        offset = 0
        for a in self.res_fmt:
            # Easy cases first:
            if a not in ["s", "p"]:
                fmt = f">{a}"
                size = struct.calcsize(fmt)
                result.append(struct.unpack_from(f">{a}", reply.data[offset:])[0])
            elif a == "s":
                string, size = unpack_cstring(reply.data[offset:])
                result.append(string)
            else:
                size = struct.unpack_from(">H", reply.data[offset:])[0]
                result.append(reply.data[offset + 2 : size])
                size += 2
            offset += size
        return result


class RpcProgram:
    def __init__(self, attr: RpcProcedure, client: "RpcClient") -> None:
        self.rpc_client = client
        self.name = attr.name
        self.id = attr.program_id
        self.procs = {}

    def add_proc(self, proc: RpcProcedure) -> None:
        self.procs[proc.proc_name] = proc
        setattr(self, proc.proc_name, proc)


class RpcClient:    
    """ Example use:
    >>> client = RpcClient(70)
    >>> client.settings["timeout"] = 500
    >>> client.fetch_procedures()
    >>> client.programs.dsu_server.list_blocks()
    ['noname', 0, 100000, 0, 0, 20000, 'noname', 1, 100000, 0, 0, 96700]
    """
    RPC_MSG_CALL = 0
    RPC_VERSION = 1

    def __init__(self, server: int, settings: dict = {}):
        self.server = server
        self.programs = type("Programs", (object,), {})()
        self.settings = settings
        self.transaction_id = 0

    def fetch_procedures(self) -> bool:
        """
        Fetch RPC meta-data from the server, including available programs and their procedures. This method establishes a connection to the RPC server, sends a request to retrieve the list of available procedures, 
        and processes the response to populate the client's program and procedure information.
        If successful, the `self` object will be updated with the available programs and their procedures, which can then be called directly from the client as attributes. 

        Returns:
            `True` if the procedures were successfully fetched and processed, or `False` if there was an error during the process (e.g., connection issues, invalid responses).
        """
        res = True
        rpc_port = self.settings.get("port", 9)
        timeout = self.settings.get("timeout", 1000)
        csp_priority = self.settings.get("csp_priority", csp.CSP_PRIO_NORM)
        csp_socket_options = self.settings.get("socket_options", csp.CSP_O_NONE)
        connection = csp.connect(
            csp_priority, self.server, rpc_port, timeout, csp_socket_options
        )
        data = RpcHeader(
            type=self.RPC_MSG_CALL, version=self.RPC_VERSION, xid=self.transaction_id
        ).to_network()
        self.transaction_id += 1
        data += RpcCall(data=bytes(), program=0xFFFFFFFF, procedure=2).to_network()
        reply = csp.buffer_get(0)
        csp.packet_set_data(reply, data)
        csp.send(connection, reply)
        while True:
            reply = csp.read(connection, timeout)
            if reply:
                reply_data = csp.packet_get_data(reply)
                reply = RpcReply.from_nw(reply_data)
                if not reply:
                    res = False
                    break
                if reply.idx == reply.amount:
                    # We're done here
                    break
                fetch_r = RpcProcedure.from_nw(self, reply.data)
                prg = RpcProgram(fetch_r, self)
                if not hasattr(self.programs, fetch_r.name):
                    setattr(self.programs, fetch_r.name, prg)
                else:
                    prg = getattr(self.programs, fetch_r.name)
                prg.add_proc(fetch_r)
            else:
                res = False
                break
        csp.close(connection)
        return res
