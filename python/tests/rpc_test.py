import pycsh
from spaceinventor.rpc import RpcClient

def test_scheduler(zmqproxy, csh_session):
    with zmqproxy:        
        csh_session.prompt()
        csh_session.sendline("csp add zmq 1 localhost")
        csh_session.sendline("apm load")
        csh_session.sendline("dsu start")
        pycsh.csp_init()
        pycsh.csp_add_zmq(2, "localhost")
        client = RpcClient(1)
        client.fetch_procedures()
        assert len(dir(client.programs))
