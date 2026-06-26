#!/usr/bin/env python3
"""
RPC Code Generator: Generates C code from JSON5 interface definitions.

Converts JSON5 interface specifications into C header and implementation files
with request/response structs, client functions, and server handlers.
"""

import json5
import os
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional
import shutil

import json

# Type mapping from JSON5 to C primitives
TYPE_MAPPING = {
    'u8': 'uint8_t',
    'u16': 'uint16_t',
    'u32': 'uint32_t',
    'u64': 'uint64_t',
    'i8': 'int8_t',
    'i16': 'int16_t',
    'i32': 'int32_t',
    'i64': 'int64_t',
    'x8': 'uint8_t',
    'x16': 'uint16_t',
    'x32': 'uint32_t',
    'x64': 'uint64_t',
    'f32': 'float',
    'f64': 'double',
    'string': 'char *',
}

DEFAULT_STRING_MAX = 256


def get_c_type(json_type: str) -> str:
    """Convert JSON5 type to C type."""
    if json_type not in TYPE_MAPPING:
        raise ValueError(f"Unknown type: {json_type}")
    return TYPE_MAPPING[json_type]


def get_field_declaration(field: Dict[str, Any], context: str) -> str:
    """Return a C field declaration for a given JSON5 field."""
    json_type = field['type']
    name = field['name']
    if json_type == 'string':
        max_length = field.get('max_length', field.get('max_len', DEFAULT_STRING_MAX))
        return f"char {name}[{max_length}]"
    return f"{get_c_type(json_type)} {name}"


def render_c_default(field: Dict[str, Any]) -> str:
    """Render a C initializer or default value for a field."""
    default = field['default']
    if field['type'] == 'string':
        return json.dumps(default)
    return default


def to_macro_name(name: str) -> str:
    """Convert camelCase/snake_case to UPPER_CASE macro name."""
    result = []
    for i, char in enumerate(name):
        if char.isupper() and i > 0:
            result.append('_')
        result.append(char.upper())
    return ''.join(result)


def to_function_name(program: str, procedure: str, suffix: str = '') -> str:
    """Generate function name from program and procedure."""
    base = f"rpc_{program}_{procedure}"
    return f"{base}_{suffix}" if suffix else base


def get_debug_printf_format(json_type: str) -> str:
    """Return a printf format fragment for debug output of a JSON5 type."""
    mapping = {
        'u8': '%"PRIu8"',
        'u16': '%"PRIu16"',
        'u32': '%"PRIu32"',
        'u64': '%"PRIu64"',
        'i8': '%"PRId8"',
        'i16': '%"PRId16"',
        'i32': '%"PRId32"',
        'i64': '%"PRId64"',
        'x8': '0x%"PRIx8"',
        'x16': '0x%"PRIx16"',
        'x32': '0x%"PRIx32"',
        'x64': '0x%"PRIx64"',
        'f32': '%f',
        'f64': '%f',
        'string': '%s',
    }
    return mapping.get(json_type, '%"PRIu32')


def generate_common_header(spec: Dict[str, Any]) -> str:
    """Generate the shared common header file content."""
    program = spec['program']
    program_upper = to_macro_name(program)
    address = spec['address']
    procedures = spec['procedures']
    
    lines = []
    guard = f"RPC_{program_upper}_H"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <rpc.h>")
    lines.append("")
    lines.append(f"/* Program: {program} */")
    lines.append(f"#define RPC_PROGRAM_{program_upper} {hex(address)}")
    lines.append("")
    for proc in procedures:
        proc_upper = to_macro_name(proc['name'])
        proc_index = proc['index']
        lines.append(f"#define RPC_{program_upper}_{proc_upper} {proc_index}")
    lines.append("")
    lines.append("/* Request and Response Structures */")
    for proc in procedures:
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        lines.append(f"typedef struct {{")
        for field in proc['request']:
            lines.append(f"    {get_field_declaration(field, 'request')};")
        lines.append(f"}} {request_typedef};")
        lines.append("")
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        lines.append(f"typedef struct {{")
        for field in proc['response']:
            lines.append(f"    {get_field_declaration(field, 'response')};")
        lines.append(f"}} {response_typedef};")
        lines.append("")
    lines.append(f"#endif /* {guard} */")
    return "\n".join(lines)


def generate_client_header(spec: Dict[str, Any]) -> str:
    """Generate the client-only header file content."""
    program = spec['program']
    program_upper = to_macro_name(program)
    procedures = spec['procedures']
    lines = []
    guard = f"RPC_{program_upper}_CLIENT_H"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append(f"#include \"rpc_{program}.h\"")
    lines.append(f"#include <rpc_client.h>")
    lines.append("")
    lines.append("/* Client-side Functions */")
    for proc in procedures:
        maxresponses = proc.get('maxresponses', 1)
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        non_default_fields = [f for f in proc['request'] if 'default' not in f]
        if non_default_fields:
            init_params = ", ".join([f"{get_c_type(f['type'])} {f['name']}" for f in non_default_fields])
            lines.append(f"{request_typedef} {to_function_name(program, proc_name, 'init')}({init_params});")
        else:
            lines.append(f"{request_typedef} {to_function_name(program, proc_name, 'init')}(void);")
        if maxresponses > 1:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response, uint32_t *numresponses);")
        else:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response);") 
        lines.append("")
    lines.append(f"#endif /* {guard} */")
    return "\n".join(lines)


def generate_server_header(spec: Dict[str, Any]) -> str:
    """Generate the server-only header file content."""
    program = spec['program']
    program_upper = to_macro_name(program)
    procedures = spec['procedures']
    lines = []
    guard = f"RPC_{program_upper}_SERVER_H"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append(f"#include \"rpc_{program}.h\"")
    lines.append(f"#include <rpc_server.h>")
    lines.append("")
    lines.append("/* Server-side Handler */")
    lines.append("/* User must implement this handler */")
    for proc in procedures:
        maxresponses = proc.get('maxresponses', 1)
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        if maxresponses > 1:
            lines.append(f"void {to_function_name(program, proc_name, 'host')}(const {request_typedef} *request, {response_typedef} *response, uint32_t *numresponses);")
        else:
            lines.append(f"void {to_function_name(program, proc_name, 'host')}(const {request_typedef} *request, {response_typedef} *response);")
        lines.append("")
    lines.append(f"int rpc_handle_calls_{program}(uint32_t procedure, csp_conn_t *conn, rpc_msg_t *call_msg);")
    lines.append("")
    lines.append(f"#endif /* {guard} */")
    return "\n".join(lines)


def generate_client_implementation(spec: Dict[str, Any], debug: bool = False) -> str:
    """Generate the client-side C implementation file content."""
    program = spec['program']
    procedures = spec['procedures']
    
    lines = []
    lines.append(f'#include "rpc_{program}.h"')
    lines.append("#include <rpc_client.h>")
    lines.append("#include <string.h>")
    lines.append("")
    if debug:
        lines.append("#define RPC_DEBUG")
        lines.append("")
    # Init functions
    for proc in procedures:
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        non_default_fields = [f for f in proc['request'] if 'default' not in f]
        if non_default_fields:
            init_params = ", ".join([f"{get_c_type(f['type'])} {f['name']}" for f in non_default_fields])
        else:
            init_params = "void"
        lines.append(f"{request_typedef} {to_function_name(program, proc_name, 'init')}({init_params}) {{")
        lines.append(f"    {request_typedef} request;")
        for field in non_default_fields:
                if field['type'] == 'string':
                    lines.append(f"    strncpy(request.{field['name']}, {field['name']}, sizeof(request.{field['name']}));")
                    lines.append(f"    request.{field['name']}[sizeof(request.{field['name']}) - 1] = 0;")
                else:
                    lines.append(f"    request.{field['name']} = {field['name']};")
        for field in proc['request']:
            if 'default' in field:
                default_value = render_c_default(field)
                if field['type'] == 'string':
                    lines.append(f"    strncpy(request.{field['name']}, {default_value}, sizeof(request.{field['name']}));")
                    lines.append(f"    request.{field['name']}[sizeof(request.{field['name']}) - 1] = 0;")
                else:
                    lines.append(f"    request.{field['name']} = {default_value};")
        lines.append("    return request;")
        lines.append("}")
        lines.append("")
    
    # Client methods
    lines.append("/* Client Call Functions */")
    for proc in procedures:
        maxresponses = proc.get('maxresponses', 1)
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        program_upper = to_macro_name(program)
        proc_upper = to_macro_name(proc_name)
        if maxresponses > 1:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response, uint32_t *numresponses) {{")
        else:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response) {{")
        lines.append("")
        lines.append("    csp_conn_t *conn = NULL;")
        lines.append("    rpc_msg_t *request_msg = NULL;")
        lines.append("    rpc_msg_t *reply_msg = NULL;")
        lines.append("")
        lines.append(f"    int status = rpc_build_request(node, RPC_PROGRAM_{program_upper}, RPC_{program_upper}_{proc_upper}, &conn, &request_msg);")
        lines.append("    if (status != RPC_STATUS_OK) {")
        lines.append("        return status;")
        lines.append("    }")
        lines.append("")
        lines.append("    /* Add request fields */")
        for field in proc['request']:
            field_type = field['type']
            if field_type == 'string':
                lines.append(f"    rpc_request_push_string(request->{field['name']}, request_msg);")
            else:
                c_type = get_c_type(field_type)
                if c_type == 'int32_t':
                    push_func = 'rpc_request_push_int32'
                elif c_type == 'int16_t':
                    push_func = 'rpc_request_push_int16'
                elif c_type == 'int8_t':
                    push_func = 'rpc_request_push_int8'
                elif c_type == 'uint32_t':
                    push_func = 'rpc_request_push_uint32'
                elif c_type == 'uint16_t':
                    push_func = 'rpc_request_push_uint16'
                elif c_type == 'uint8_t':
                    push_func = 'rpc_request_push_uint8'
                elif c_type == 'int64_t':
                    push_func = 'rpc_request_push_int64'
                elif c_type == 'uint64_t':
                    push_func = 'rpc_request_push_uint64'
                elif c_type == 'float':
                    push_func = 'rpc_request_push_float'
                elif c_type == 'double':
                    push_func = 'rpc_request_push_double'
                else:
                    push_func = 'rpc_request_push_uint32'
                lines.append(f"    {push_func}(request->{field['name']}, request_msg);")
        lines.append("")
        lines.append("    rpc_send(conn, request_msg);")
        lines.append("")
        lines.append("    uint32_t idx = 0;")
        lines.append("    do {")
        lines.append(f"        status = rpc_get_reply(conn, &reply_msg, {maxresponses if maxresponses > 1 else 1}, idx, timeout);")
        lines.append("        if (status != RPC_STATUS_OK) {")
        lines.append("            rpc_disconnect(conn);")
        lines.append("            return status;")
        lines.append("        }")
        lines.append("")
        lines.append("        /* Extract response fields */")
        for field in proc['response']:
            field_type = field['type']
            if field_type == 'string':
                lines.append(f"        rpc_result_pop_string(response[idx].{field['name']}, reply_msg);")
            else:
                c_type = get_c_type(field_type)
                if c_type == 'int32_t':
                    pop_func = 'rpc_result_pop_int32'
                elif c_type == 'int16_t':
                    pop_func = 'rpc_result_pop_int16'
                elif c_type == 'int8_t':
                    pop_func = 'rpc_result_pop_int8'
                elif c_type == 'uint32_t':
                    pop_func = 'rpc_result_pop_uint32'
                elif c_type == 'uint16_t':
                    pop_func = 'rpc_result_pop_uint16'
                elif c_type == 'uint8_t':
                    pop_func = 'rpc_result_pop_uint8'
                elif c_type == 'int64_t':
                    pop_func = 'rpc_result_pop_int64'
                elif c_type == 'uint64_t':
                    pop_func = 'rpc_result_pop_uint64'
                elif c_type == 'float':
                    pop_func = 'rpc_result_pop_float'
                elif c_type == 'double':
                    pop_func = 'rpc_result_pop_double'
                else:
                    pop_func = 'rpc_result_pop_uint32'
                lines.append(f"        response[idx].{field['name']} = {pop_func}(reply_msg);")
        lines.append("")
        lines.append("        rpc_buffer_free(reply_msg);")
        lines.append("        idx++;")
        lines.append("")
        lines.append("    } while (idx < reply_msg->reply.amount);")
        lines.append("")
        if maxresponses > 1:
            lines.append("    *numresponses = idx;")
        lines.append("")
        lines.append("    return rpc_disconnect(conn);")
        lines.append("}")
        lines.append("")
    
    return "\n".join(lines)



def generate_server_implementation(spec: Dict[str, Any], debug: bool = False) -> str:
    """Generate the server-side C implementation file content."""
    program = spec['program']
    procedures = spec['procedures']
    
    lines = []
    lines.append(f'#include "rpc_{program}_server.h"')
    lines.append('#include <endian.h>')
    lines.append('#include <inttypes.h>')
    lines.append("#include <string.h>")
    if debug:
        lines.append('#include <stdio.h>')
    lines.append("")
    lines.append("/* Server Handler Dispatcher */")
    program_upper = to_macro_name(program)
    lines.append(f"int rpc_handle_calls_{program}(uint32_t procedure, csp_conn_t *conn, rpc_msg_t *call) {{")
    lines.append("")
    lines.append("    switch (procedure) {")
    for proc in procedures:
        proc_name = proc['name']
        proc_upper = to_macro_name(proc_name)
        proc_index = proc['index']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        maxresponses = proc.get('maxresponses', 1)
        lines.append(f"        case RPC_{program_upper}_{proc_upper}: /* {proc_index} */")
        lines.append("        {")
        lines.append(f"            {request_typedef} request;")
        lines.append(f"            {response_typedef} response[{maxresponses}];")
        lines.append(f"            uint32_t numresponses = {maxresponses};")
        lines.append("")
        for field in proc['request']:
            field_type = field['type']
            if field_type == 'string':
                lines.append(f"            char *{field['name']}_ptr = request.{field['name']};")
                lines.append(f"            rpc_request_pop_string({field['name']}_ptr, call);")
            else:
                c_type = get_c_type(field_type)
                if c_type == 'int32_t':
                    pop_func = 'rpc_request_pop_int32'
                elif c_type == 'int16_t':
                    pop_func = 'rpc_request_pop_int16'
                elif c_type == 'int8_t':
                    pop_func = 'rpc_request_pop_int8'
                elif c_type == 'uint32_t':
                    pop_func = 'rpc_request_pop_uint32'
                elif c_type == 'uint16_t':
                    pop_func = 'rpc_request_pop_uint16'
                elif c_type == 'uint8_t':
                    pop_func = 'rpc_request_pop_uint8'
                elif c_type == 'int64_t':
                    pop_func = 'rpc_request_pop_int64'
                elif c_type == 'uint64_t':
                    pop_func = 'rpc_request_pop_uint64'
                elif c_type == 'float':
                    pop_func = 'rpc_request_pop_float'
                elif c_type == 'double':
                    pop_func = 'rpc_request_pop_double'
                else:
                    pop_func = 'rpc_request_pop_uint32'
                lines.append(f"            request.{field['name']} = {pop_func}(call);")
        lines.append("")
        if debug:
            lines.append(f'            printf("RPC: Received {proc_upper} call\\n");')
            for field in proc['request']:
                fmt = get_debug_printf_format(field['type'])
                name = field['name']
                lines.append(f'            printf("RPC: {name}={fmt}\\n", request.{name});')
            lines.append("")
        if maxresponses > 1:
            lines.append(f"            {to_function_name(program, proc_name, 'host')}(&request, response, &numresponses);")
            lines.append("")
            lines.append(f"            if (numresponses == 0) return sizeof({response_typedef});")
        else:
            lines.append(f"            {to_function_name(program, proc_name, 'host')}(&request, response);")
        lines.append("")
        lines.append("            for (uint32_t i = 0; i < numresponses; i++) {")
        if debug:
            lines.append(f'                printf("RPC: Sending {proc_upper} response idx=%"PRIu32"\\n", i);')
            for field in proc['response']:
                fmt = get_debug_printf_format(field['type'])
                name = field['name']
                lines.append(f'                printf("RPC: response[%"PRIu32"].{name}={fmt}\\n", i, response[i].{name});')
        lines.append("                rpc_msg_t *reply = rpc_result_prepare(call, numresponses, i);")
        for field in proc['response']:
            field_type = field['type']
            if field_type == 'string':
                lines.append(f"                rpc_result_push_string(response[i].{field['name']}, reply);")
            else:
                c_type = get_c_type(field_type)
                if c_type in ['int32_t', 'int16_t', 'int8_t']:
                    push_func = 'rpc_result_push_int32'
                elif c_type in ['uint32_t', 'uint16_t', 'uint8_t']:
                    push_func = 'rpc_result_push_uint32'
                elif c_type == 'int64_t':
                    push_func = 'rpc_result_push_int64'
                elif c_type == 'uint64_t':
                    push_func = 'rpc_result_push_uint64'
                elif c_type == 'float':
                    push_func = 'rpc_result_push_float'
                elif c_type == 'double':
                    push_func = 'rpc_result_push_double'
                else:
                    push_func = 'rpc_result_push_int32'
                lines.append(f"                {push_func}(response[i].{field['name']}, reply);")
        lines.append(f"                rpc_send_reply(conn, reply);")
        lines.append("            }")
        lines.append("            break;")
        lines.append("        }")
    lines.append("        default:")
    lines.append("            break;")
    lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_files(json_file: str, output_dir: Optional[str] = None, mode: str = 'both', debug: bool = False):
    """Generate C files from JSON5 specification."""
    # Parse JSON5 file
    with open(json_file, 'r') as f:
        spec = json5.load(f)
    
    program = spec['program']
    
    if mode not in ('client', 'server', 'both'):
        raise ValueError("mode must be 'client', 'server', or 'both'")
    
    # Determine output directory
    if output_dir is None:
        output_dir = os.path.dirname(json_file) or '.'
    
    os.makedirs(output_dir, exist_ok=True)
    output_dir_header = os.path.join(output_dir, "../")
    
    common_header = os.path.join(output_dir, f"rpc_{program}.h")
    with open(common_header, 'w') as f:
        f.write(generate_common_header(spec))
    shutil.copy(common_header, output_dir_header)

    if mode in ('client', 'both'):
        client_header = os.path.join(output_dir, f"rpc_{program}_client.h")
        with open(client_header, 'w') as f:
            f.write(generate_client_header(spec))
        shutil.copy(client_header, output_dir_header)
        client_file = os.path.join(output_dir, f"rpc_{program}_client.c")
        with open(client_file, 'w') as f:
            f.write(generate_client_implementation(spec, debug))
    
    if mode in ('server', 'both'):
        server_header = os.path.join(output_dir, f"rpc_{program}_server.h")
        with open(server_header, 'w') as f:
            f.write(generate_server_header(spec))
        shutil.copy(server_header, output_dir_header)
        server_file = os.path.join(output_dir, f"rpc_{program}_server.c")
        with open(server_file, 'w') as f:
            f.write(generate_server_implementation(spec, debug))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 generate_rpc.py <json5_file> [output_dir] [mode] [debug]")
        print("mode = client | server | both (default)")
        print("debug = true | false (default)")
        sys.exit(1)
    
    json_file = sys.argv[1]
    output_dir = None
    mode = 'both'
    debug = False
    for i in range(2, len(sys.argv)):
        arg = sys.argv[i].lower()
        if arg in ('client', 'server', 'both'):
            mode = arg
        elif arg == 'debug':
            debug = True
        else:
            if output_dir is not None:
                print(f"Error: Unrecognized argument: {sys.argv[i]}")
                sys.exit(1)
            output_dir = sys.argv[i]

    if not os.path.exists(json_file):
        print(f"Error: File not found: {json_file}")
        sys.exit(1)

    try:
        generate_files(json_file, output_dir, mode, debug)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
