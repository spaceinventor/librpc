#!/usr/bin/env python3
"""
RPC Code Generator: Generates C code from JSON interface definitions.

Converts JSON interface specifications into C header and implementation files
with request/response structs, client functions, and server handlers.
Supports structs, arrays of structs, array of primitives, and bitwise math in enum values.
"""

try:
    import json5 as json
    json5_available = True
except ImportError:
    json5_available = False
    import json

import os
import sys
import traceback
from pathlib import Path
from typing import Dict, List, Any, Optional
import shutil

# Type mapping from JSON5 to C primitives
TYPE_MAPPING = {
    'u8': 'uint8_t', 'u16': 'uint16_t', 'u32': 'uint32_t', 'u64': 'uint64_t',
    'i8': 'int8_t',  'i16': 'int16_t',  'i32': 'int32_t',  'i64': 'int64_t',
    'x8': 'uint8_t', 'x16': 'uint16_t', 'x32': 'uint32_t', 'x64': 'uint64_t',
    'f32': 'float',  'f64': 'double',   'string': 'char *',
}

DEFAULT_STRING_MAX = 256


def get_rpc_version(spec: Dict[str, Any]) -> int:
    if 'rpc_version' not in spec:
        raise ValueError("Missing required rpc_version (must be 1 or 2)")
    rpc_version = spec['rpc_version']
    if type(rpc_version) is not int or rpc_version not in (1, 2):
        raise ValueError(f"Unknown rpc_version: {rpc_version!r} (must be 1 or 2)")
    return rpc_version


def validate_rpc_features(spec: Dict[str, Any]) -> None:
    rpc_version = get_rpc_version(spec)

    def validate_field(field: Dict[str, Any]) -> None:
        name = field['name']
        field_type = field['type']
        max_elements = field.get('max_elements', 1)

        if type(max_elements) is not int or max_elements < 1:
            raise ValueError(f"max_elements must be a positive integer for field '{name}'")
        if rpc_version == 1 and max_elements > 1:
            raise ValueError(
                f"Arrays defined by max_elements are only supported with "
                f"rpc_version 2 (field '{name}')"
            )
        if field_type == 'struct' and rpc_version == 1:
            raise ValueError(
                f"Struct fields are only supported with rpc_version 2 "
                f"(field '{name}')"
            )

        for subfield in field.get('contents', []):
            validate_field(subfield)

    for proc in spec.get('procedures', []):
        if rpc_version == 2 and 'maxresponses' in proc:
            raise ValueError(
                f"maxresponses is only supported with rpc_version 1 "
                f"(procedure '{proc['name']}')"
            )

        if 'maxresponses' in proc:
            maxresponses = proc['maxresponses']
            if type(maxresponses) is not int or maxresponses < 1:
                raise ValueError(
                    f"maxresponses must be a positive integer "
                    f"(procedure '{proc['name']}')"
                )

        for field in proc.get('request', []) + proc.get('response', []):
            validate_field(field)


def get_c_type(json_type: str) -> str:
    if json_type not in TYPE_MAPPING:
        raise ValueError(f"Unknown type: {json_type}")
    return TYPE_MAPPING[json_type]


def get_field_type(spec: Dict[str, Any], field: Dict[str, Any]) -> str:
    type_str = field['type']
    if type_str == 'struct':
        return 'struct'

    if type_str in TYPE_MAPPING:
        return get_c_type(type_str)

    enums = spec.get('enums', [])
    enum_item = next((item for item in enums if item["name"] == type_str), None)
    if enum_item:
        program = spec['program']
        return f"rpc_{program}_{type_str}_t"

    raise ValueError(f"Unknown field type: {type_str}")


def get_field_declaration(spec: Dict[str, Any], field: Dict[str, Any], indent="    ") -> str:
    json_type = field['type']
    name = field['name']
    max_elements = field.get('max_elements', 1)

    if max_elements == 0:
        raise ValueError(f"max_elements cannot be 0 for field '{name}'")

    is_array = max_elements > 1
    is_struct = json_type == 'struct'

    if is_struct:
        lines = []
        if is_array:
            lines.append(f"uint32_t {name}_count;")
            lines.append(f"{indent}struct {{")
        else:
            lines.append(f"struct {{")

        for subfield in field.get('contents', []):
            if 'description' in subfield:
                lines.append(f"{indent}    /* {subfield['description']} */")

            sub_type = subfield['type']
            sub_max = subfield.get('max_elements', 1)

            if sub_max == 0:
                raise ValueError(f"max_elements cannot be 0 for subfield '{subfield['name']}'")
            if sub_type == 'struct' or sub_max > 1:
                raise ValueError(f"Nested structs/arrays are not supported in field '{name}'.")

            if sub_type == 'string':
                max_length = subfield.get('max_length', subfield.get('max_len', DEFAULT_STRING_MAX))
                lines.append(f"{indent}    char {subfield['name']}[{max_length}];")
            else:
                c_type = get_field_type(spec, subfield)
                lines.append(f"{indent}    {c_type} {subfield['name']};")

        if is_array:
            lines.append(f"{indent}}} {name}[{max_elements}]")
        else:
            lines.append(f"{indent}}} {name}")

        return "\n".join(lines)

    elif json_type == 'string':
        max_length = field.get('max_length', field.get('max_len', DEFAULT_STRING_MAX))
        if is_array:
            return f"uint32_t {name}_count;\n{indent}char {name}[{max_elements}][{max_length}]"
        else:
            return f"char {name}[{max_length}]"

    else:
        c_type = get_field_type(spec, field)
        if is_array:
            return f"uint32_t {name}_count;\n{indent}{c_type} {name}[{max_elements}]"
        else:
            return f"{c_type} {name}"


def get_type_size(spec: Dict[str, Any], field: Dict[str, Any]) -> str:
    field_type = field['type']

    if field_type == 'struct':
        return 'struct'

    if field_type in TYPE_MAPPING:
        return field_type

    enums = spec.get('enums', [])
    enum_item = next((item for item in enums if item["name"] == field_type), None)

    if enum_item:
        min_val, max_val, current_val = 0, 0, -1
        for item in enum_item.get('items', []):
            if 'value' in item:
                val_str = str(item['value'])
                # Use eval to support bitwise operations like (1 << 0)
                current_val = eval(val_str)
            else:
                current_val += 1
            if current_val < min_val: min_val = current_val
            if current_val > max_val: max_val = current_val

        if min_val >= 0:
            if max_val <= 255: return 'u8'
            elif max_val <= 65535: return 'u16'
            elif max_val <= 4294967295: return 'u32'
            else: return 'u64'
        else:
            if min_val >= -128 and max_val <= 127: return 'i8'
            elif min_val >= -32768 and max_val <= 32767: return 'i16'
            elif min_val >= -2147483648 and max_val <= 2147483647: return 'i32'
            else: return 'i64'

    raise ValueError(f"Unknown field type: {field_type}")


def render_c_default(field: Dict[str, Any]) -> str:
    default = field['default']
    if field['type'] == 'string':
        return json.dumps(default)
    return default


def to_macro_name(name: str) -> str:
    result = []
    for i, char in enumerate(name):
        if char.isupper() and i > 0:
            result.append('_')
        result.append(char.upper())
    return ''.join(result)


def to_function_name(program: str, procedure: str, suffix: str = '') -> str:
    base = f"rpc_{program}_{procedure}"
    return f"{base}_{suffix}" if suffix else base


def get_debug_printf_format(json_type: str) -> str:
    mapping = {
        'u8': '%"PRIu8"', 'u16': '%"PRIu16"', 'u32': '%"PRIu32"', 'u64': '%"PRIu64"',
        'i8': '%"PRId8"', 'i16': '%"PRId16"', 'i32': '%"PRId32"', 'i64': '%"PRId64"',
        'x8': '0x%"PRIx8"', 'x16': '0x%"PRIx16"', 'x32': '0x%"PRIx32"', 'x64': '0x%"PRIx64"',
        'f32': '%f', 'f64': '%f', 'string': '%s',
    }
    return mapping.get(json_type, '%"PRIu32"')


# ---- Flat Generation Helpers for Push / Pop / Debug ----

def get_push_lines(spec: Dict[str, Any], field: Dict[str, Any], prefix: str, msg_name: str, is_result: bool, indent: str = "    ") -> List[str]:
    lines = []
    push_pfx = "rpc_result_push" if is_result else "rpc_request_push"
    name = field['name']

    max_elements = field.get('max_elements', 1)
    if max_elements == 0:
        raise ValueError(f"max_elements cannot be 0 for field '{name}'")

    is_array = max_elements > 1
    is_struct = field['type'] == 'struct'

    if is_array:
        lines.append(f"{indent}{push_pfx}_uint32({prefix}{name}_count, {msg_name});")
        lines.append(f"{indent}for (uint32_t i_{name} = 0; i_{name} < {prefix}{name}_count; i_{name}++) {{")

        if is_struct:
            for subfield in field.get('contents', []):
                if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                    raise ValueError(f"Nested structs/arrays are not supported in '{name}'.")
                elif subfield['type'] == 'string':
                    lines.append(f"{indent}    {push_pfx}_string({prefix}{name}[i_{name}].{subfield['name']}, {msg_name});")
                else:
                    c_type = get_c_type(get_type_size(spec, subfield))
                    suffix = c_type.replace('_t', '')
                    lines.append(f"{indent}    {push_pfx}_{suffix}({prefix}{name}[i_{name}].{subfield['name']}, {msg_name});")
        elif field['type'] == 'string':
            lines.append(f"{indent}    {push_pfx}_string({prefix}{name}[i_{name}], {msg_name});")
        else:
            type_size = get_type_size(spec, field)
            c_type = get_c_type(type_size)
            suffix = c_type.replace('_t', '')
            lines.append(f"{indent}    {push_pfx}_{suffix}({prefix}{name}[i_{name}], {msg_name});")

        lines.append(f"{indent}}}")

    elif is_struct:
        for subfield in field.get('contents', []):
            if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                raise ValueError(f"Nested structs/arrays are not supported in '{name}'.")
            elif subfield['type'] == 'string':
                lines.append(f"{indent}{push_pfx}_string({prefix}{name}.{subfield['name']}, {msg_name});")
            else:
                c_type = get_c_type(get_type_size(spec, subfield))
                suffix = c_type.replace('_t', '')
                lines.append(f"{indent}{push_pfx}_{suffix}({prefix}{name}.{subfield['name']}, {msg_name});")

    elif field['type'] == 'string':
        lines.append(f"{indent}{push_pfx}_string({prefix}{name}, {msg_name});")

    else:
        type_size = get_type_size(spec, field)
        c_type = get_c_type(type_size)
        suffix = c_type.replace('_t', '')
        lines.append(f"{indent}{push_pfx}_{suffix}({prefix}{name}, {msg_name});")

    return lines


def get_pop_lines(spec: Dict[str, Any], field: Dict[str, Any], prefix: str, msg_name: str, offset_name: str, is_result: bool, indent: str = "    ") -> List[str]:
    lines = []
    pop_pfx = "rpc_result_pop" if is_result else "rpc_request_pop"
    name = field['name']

    max_elements = field.get('max_elements', 1)
    if max_elements == 0:
        raise ValueError(f"max_elements cannot be 0 for field '{name}'")

    is_array = max_elements > 1
    is_struct = field['type'] == 'struct'

    if is_array:
        lines.append(f"{indent}{prefix}{name}_count = {pop_pfx}_uint32({msg_name}, {offset_name});")
        lines.append(f"{indent}if ({prefix}{name}_count > {max_elements}) {prefix}{name}_count = {max_elements};")
        lines.append(f"{indent}for (uint32_t i_{name} = 0; i_{name} < {prefix}{name}_count; i_{name}++) {{")

        if is_struct:
            for subfield in field.get('contents', []):
                if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                    raise ValueError(f"Nested structs/arrays are not supported in '{name}'.")
                elif subfield['type'] == 'string':
                    lines.append(f"{indent}    {pop_pfx}_string({prefix}{name}[i_{name}].{subfield['name']}, {msg_name}, {offset_name});")
                else:
                    c_type = get_c_type(get_type_size(spec, subfield))
                    suffix = c_type.replace('_t', '')
                    lines.append(f"{indent}    {prefix}{name}[i_{name}].{subfield['name']} = {pop_pfx}_{suffix}({msg_name}, {offset_name});")
        elif field['type'] == 'string':
            lines.append(f"{indent}    {pop_pfx}_string({prefix}{name}[i_{name}], {msg_name}, {offset_name});")
        else:
            type_size = get_type_size(spec, field)
            c_type = get_c_type(type_size)
            suffix = c_type.replace('_t', '')
            lines.append(f"{indent}    {prefix}{name}[i_{name}] = {pop_pfx}_{suffix}({msg_name}, {offset_name});")

        lines.append(f"{indent}}}")

    elif is_struct:
        for subfield in field.get('contents', []):
            if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                raise ValueError(f"Nested structs/arrays are not supported in '{name}'.")
            elif subfield['type'] == 'string':
                lines.append(f"{indent}{pop_pfx}_string({prefix}{name}.{subfield['name']}, {msg_name}, {offset_name});")
            else:
                c_type = get_c_type(get_type_size(spec, subfield))
                suffix = c_type.replace('_t', '')
                lines.append(f"{indent}{prefix}{name}.{subfield['name']} = {pop_pfx}_{suffix}({msg_name}, {offset_name});")

    elif field['type'] == 'string':
        lines.append(f"{indent}{pop_pfx}_string({prefix}{name}, {msg_name}, {offset_name});")

    else:
        type_size = get_type_size(spec, field)
        c_type = get_c_type(type_size)
        suffix = c_type.replace('_t', '')
        lines.append(f"{indent}{prefix}{name} = {pop_pfx}_{suffix}({msg_name}, {offset_name});")

    return lines


def mpack_type_suffix(spec: Dict[str, Any], field: Dict[str, Any]) -> str:
    field_type = get_type_size(spec, field)
    if field_type.startswith('x'):
        field_type = 'u' + field_type[1:]
    elif field_type == 'f32':
        field_type = 'float'
    elif field_type == 'f64':
        field_type = 'double'
    return field_type


def get_mpack_write_value_lines(spec: Dict[str, Any], field: Dict[str, Any], value: str, writer: str, indent: str) -> List[str]:
    if field['type'] == 'struct':
        contents = field.get('contents', [])
        lines = [f"{indent}mpack_start_array({writer}, {len(contents)});"]
        for subfield in contents:
            lines.extend(get_mpack_write_value_lines(spec, subfield, f"{value}.{subfield['name']}", writer, indent))
        lines.append(f"{indent}mpack_finish_array({writer});")
        return lines

    if field['type'] == 'string':
        return [f"{indent}mpack_write_cstr({writer}, {value});"]

    suffix = mpack_type_suffix(spec, field)
    return [f"{indent}mpack_write_{suffix}({writer}, {value});"]


def get_mpack_write_lines(spec: Dict[str, Any], field: Dict[str, Any], value: str, writer: str, indent: str = "    ") -> List[str]:
    max_elements = field.get('max_elements', 1)
    if max_elements <= 1:
        return get_mpack_write_value_lines(spec, field, value, writer, indent)

    index = f"i_{field['name']}"
    lines = [
        f"{indent}mpack_start_array({writer}, {value}_count);",
        f"{indent}for (uint32_t {index} = 0; {index} < {value}_count; {index}++) {{",
    ]
    lines.extend(get_mpack_write_value_lines(spec, field, f"{value}[{index}]", writer, indent + "    "))
    lines.extend([
        f"{indent}}}",
        f"{indent}mpack_finish_array({writer});",
    ])
    return lines


def get_mpack_read_value_lines(spec: Dict[str, Any], field: Dict[str, Any], target: str, reader: str, indent: str) -> List[str]:
    if field['type'] == 'struct':
        contents = field.get('contents', [])
        lines = [f"{indent}mpack_expect_array_match({reader}, {len(contents)});"]
        for subfield in contents:
            lines.extend(get_mpack_read_value_lines(spec, subfield, f"{target}.{subfield['name']}", reader, indent))
        lines.append(f"{indent}if (mpack_reader_error({reader}) == mpack_ok) mpack_done_array({reader});")
        return lines

    if field['type'] == 'string':
        return [
            f"{indent}mpack_expect_str_buf({reader}, {target}, sizeof({target}));",
            f"{indent}{target}[sizeof({target}) - 1] = '\\0';",
        ]

    suffix = mpack_type_suffix(spec, field)
    return [f"{indent}{target} = mpack_expect_{suffix}({reader});"]


def get_mpack_read_lines(spec: Dict[str, Any], field: Dict[str, Any], target: str, reader: str, indent: str = "    ") -> List[str]:
    max_elements = field.get('max_elements', 1)
    if max_elements <= 1:
        return get_mpack_read_value_lines(spec, field, target, reader, indent)

    index = f"i_{field['name']}"
    lines = [
        f"{indent}{target}_count = mpack_expect_array_max({reader}, {max_elements});",
        f"{indent}for (uint32_t {index} = 0; {index} < {target}_count; {index}++) {{",
    ]
    lines.extend(get_mpack_read_value_lines(spec, field, f"{target}[{index}]", reader, indent + "    "))
    lines.append(f"{indent}}}")
    lines.append(f"{indent}if (mpack_reader_error({reader}) == mpack_ok) mpack_done_array({reader});")
    return lines


def get_mpack_decode_lines(spec: Dict[str, Any], fields: List[Dict[str, Any]], prefix: str, msg_name: str, indent: str = "    ") -> List[str]:
    reader = 'reader'
    lines = [
        f"{indent}mpack_reader_t {reader};",
        f"{indent}mpack_reader_init_data(&{reader}, (const char *){msg_name}->reply.data, {msg_name}->reply.data_len);",
        f"{indent}mpack_expect_array_match(&{reader}, {len(fields)});",
    ]
    for field in fields:
        lines.extend(get_mpack_read_lines(spec, field, f"{prefix}{field['name']}", f"&{reader}", indent))
    lines.extend([
        f"{indent}if (mpack_reader_error(&{reader}) == mpack_ok) mpack_done_array(&{reader});",
        f"{indent}status = mpack_reader_destroy(&{reader}) == mpack_ok ? RPC_STATUS_OK : RPC_STATUS_ERR_INVALID;",
        f"{indent}if (status != RPC_STATUS_OK) {{",
        f"{indent}    rpc_buffer_free({msg_name});",
        f"{indent}    rpc_disconnect(conn);",
        f"{indent}    return status;",
        f"{indent}}}",
    ])
    return lines


def get_mpack_server_decode_lines(spec: Dict[str, Any], fields: List[Dict[str, Any]], indent: str = "            ") -> List[str]:
    reader = 'request_reader'
    lines = [
        f"{indent}mpack_reader_t {reader};",
        f"{indent}mpack_reader_init_data(&{reader}, (const char *)call->call.data, call->call.data_len);",
        f"{indent}mpack_expect_array_match(&{reader}, {len(fields)});",
    ]
    for field in fields:
        lines.extend(get_mpack_read_lines(spec, field, f"request->{field['name']}", f"&{reader}", indent))
    lines.extend([
        f"{indent}if (mpack_reader_error(&{reader}) == mpack_ok) mpack_done_array(&{reader});",
        f"{indent}if (mpack_reader_destroy(&{reader}) != mpack_ok) return RPC_STATUS_ERR_INVALID;",
    ])
    return lines


def get_mpack_encode_lines(spec: Dict[str, Any], fields: List[Dict[str, Any]], prefix: str, msg_name: str, is_result: bool, indent: str = "    ") -> List[str]:
    payload = f"{msg_name}->reply" if is_result else f"{msg_name}->call"
    payload_data = f"{payload}.data"
    data_offset = "reply" if is_result else "call"
    writer = 'writer'
    lines = [
        f"{indent}mpack_writer_t {writer};",
        f"{indent}mpack_writer_init(&{writer}, (char *){payload_data}, CSP_BUFFER_SIZE - offsetof(rpc_msg_t, {data_offset}.data));",
        f"{indent}mpack_start_array(&{writer}, {len(fields)});",
    ]
    for field in fields:
        value = f"{prefix}{field['name']}"
        if field.get('max_elements', 1) > 1:
            lines.extend([
                f"{indent}if ({value}_count > {field['max_elements']}) {{",
                f"{indent}    mpack_writer_destroy(&{writer});",
                f"{indent}    rpc_buffer_free({msg_name});",
            ])
            if not is_result:
                lines.extend([
                    f"{indent}    rpc_disconnect(conn);",
                ])
            lines.extend([
                f"{indent}    return RPC_STATUS_ERR_INVALID;",
                f"{indent}}}",
            ])
        lines.extend(get_mpack_write_lines(spec, field, value, f"&{writer}", indent))
    lines.extend([
        f"{indent}mpack_finish_array(&{writer});",
        f"{indent}size_t packed_size = mpack_writer_buffer_used(&{writer});",
        f"{indent}mpack_error_t pack_error = mpack_writer_destroy(&{writer});",
    ])
    if is_result:
        lines.extend([
            f"{indent}if (pack_error != mpack_ok || packed_size > UINT16_MAX) {{",
            f"{indent}    rpc_buffer_free({msg_name});",
            f"{indent}    return RPC_STATUS_ERR_INVALID;",
            f"{indent}}}",
            f"{indent}{payload}.data_len = (uint16_t)packed_size;",
        ])
    else:
        lines.extend([
            f"{indent}if (pack_error != mpack_ok || packed_size > UINT16_MAX) {{",
            f"{indent}    rpc_buffer_free({msg_name});",
            f"{indent}    rpc_disconnect(conn);",
            f"{indent}    return RPC_STATUS_ERR_INVALID;",
            f"{indent}}}",
            f"{indent}{payload}.data_len = (uint16_t)packed_size;",
        ])
    return lines


def get_debug_print_lines(spec: Dict[str, Any], field: Dict[str, Any], path_prefix: str, val_prefix: str, indent: str = "    ") -> List[str]:
    lines = []
    name = field['name']

    max_elements = field.get('max_elements', 1)
    if max_elements == 0:
        raise ValueError(f"max_elements cannot be 0 for field '{name}'")

    is_array = max_elements > 1
    is_struct = field['type'] == 'struct'

    if is_array:
        lines.append(f'{indent}printf("RPC: {path_prefix}{name}_count=%"PRIu32"\\n", {val_prefix}{name}_count);')
        lines.append(f"{indent}for (uint32_t i_{name} = 0; i_{name} < {val_prefix}{name}_count; i_{name}++) {{")

        if is_struct:
            for subfield in field.get('contents', []):
                if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                    raise ValueError("Nested structs/arrays are not supported.")
                elif subfield['type'] == 'string':
                    lines.append(f'{indent}    printf("RPC: {path_prefix}{name}[%"PRIu32"].{subfield["name"]}=%s\\n", i_{name}, {val_prefix}{name}[i_{name}].{subfield["name"]});')
                else:
                    fmt = get_debug_printf_format(get_type_size(spec, subfield))
                    lines.append(f'{indent}    printf("RPC: {path_prefix}{name}[%"PRIu32"].{subfield["name"]}={fmt}\\n", i_{name}, {val_prefix}{name}[i_{name}].{subfield["name"]});')
        elif field['type'] == 'string':
            lines.append(f'{indent}    printf("RPC: {path_prefix}{name}[%"PRIu32"]=%s\\n", i_{name}, {val_prefix}{name}[i_{name}]);')
        else:
            fmt = get_debug_printf_format(get_type_size(spec, field))
            lines.append(f'{indent}    printf("RPC: {path_prefix}{name}[%"PRIu32"]={fmt}\\n", i_{name}, {val_prefix}{name}[i_{name}]);')

        lines.append(f"{indent}}}")

    elif is_struct:
        for subfield in field.get('contents', []):
            if subfield['type'] == 'struct' or subfield.get('max_elements', 1) > 1:
                raise ValueError("Nested structs/arrays are not supported.")
            elif subfield['type'] == 'string':
                lines.append(f'{indent}printf("RPC: {path_prefix}{name}.{subfield["name"]}=%s\\n", {val_prefix}{name}.{subfield["name"]});')
            else:
                fmt = get_debug_printf_format(get_type_size(spec, subfield))
                lines.append(f'{indent}printf("RPC: {path_prefix}{name}.{subfield["name"]}={fmt}\\n", {val_prefix}{name}.{subfield["name"]});')

    elif field['type'] == 'string':
        lines.append(f'{indent}printf("RPC: {path_prefix}{name}=%s\\n", {val_prefix}{name});')

    else:
        fmt = get_debug_printf_format(get_type_size(spec, field))
        lines.append(f'{indent}printf("RPC: {path_prefix}{name}={fmt}\\n", {val_prefix}{name});')

    return lines


def generate_common_header_enums(spec: Dict[str, Any], lines: List, program: str):
    if 'enums' not in spec.keys():
        return
    program_upper = to_macro_name(program)
    enums = spec['enums']
    for enum in enums:
        lines.append("")
        if 'description' in enum:
            lines.append(f"/* {enum['description']} */")
        lines.append("typedef enum {")
        for item in enum['items']:
            if 'description' in item.keys():
                lines.append(f"    /* {item['description']} */")
            val_name_upper = to_macro_name(item['name'])
            if 'value' in item.keys():
                val = item['value']
                lines.append(f"    RPC_{program_upper}_{val_name_upper} = {val},")
            else:
                lines.append(f"    RPC_{program_upper}_{val_name_upper},")
        enum_name = enum['name']
        lines.append(f"}} rpc_{program}_{enum_name}_t;")


def generate_common_header(spec: Dict[str, Any]) -> str:
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
        if 'description' in proc:
            lines.append(f"/* {proc['description']} */")
        proc_upper = to_macro_name(proc['name'])
        proc_index = proc['index']
        lines.append(f"#define RPC_{program_upper}_{proc_upper} {proc_index}")

    generate_common_header_enums(spec, lines, program)

    lines.append("")
    for proc in procedures:
        proc_name = proc['name']

        if 'description' in proc:
            lines.append(f"/* Procedure: {proc_name} - {proc['description']} */")

        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        lines.append(f"typedef struct {{")
        for field in proc['request']:
            if 'description' in field.keys():
                lines.append(f"    /* {field['description']} */")
            lines.append(f"    {get_field_declaration(spec, field, '    ')};")
        lines.append(f"}} {request_typedef};")
        lines.append("")

        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        lines.append(f"typedef struct {{")
        for field in proc['response']:
            if 'description' in field.keys():
                lines.append(f"    /* {field['description']} */")
            lines.append(f"    {get_field_declaration(spec, field, '    ')};")
        lines.append(f"}} {response_typedef};")
        lines.append("")

    lines.append(f"#endif /* {guard} */")
    return "\n".join(lines)


def generate_client_header(spec: Dict[str, Any]) -> str:
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
        non_default_fields = [f for f in proc['request'] if 'default' not in f and f.get('max_elements', 1) <= 1 and f['type'] != 'struct']

        if non_default_fields:
            init_params = ", ".join([f"{get_field_type(spec, f)} {f['name']}" for f in non_default_fields])
            lines.append(f"int {to_function_name(program, proc_name, 'init')}({request_typedef} *request, {init_params});")
        else:
            lines.append(f"int {to_function_name(program, proc_name, 'init')}({request_typedef} *request);")

        if maxresponses > 1:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response, uint32_t *numresponses);")
        else:
            lines.append(f"int {to_function_name(program, proc_name)}(uint16_t node, uint32_t timeout, const {request_typedef} *request, {response_typedef} *response);")
        lines.append("")
    lines.append(f"#endif /* {guard} */")
    return "\n".join(lines)


def generate_server_header(spec: Dict[str, Any]) -> str:
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
    program = spec['program']
    procedures = spec['procedures']
    use_mpack = get_rpc_version(spec) == 2

    lines = []
    lines.append(f'#include "rpc_{program}.h"')
    lines.append("#include <rpc_client.h>")
    lines.append("#include <string.h>")
    lines.append("#include <assert.h>")
    if use_mpack:
        lines.append("#include <stddef.h>")
        lines.append("#include <mpack/mpack.h>")
    if debug:
        lines.append("#include <stdio.h>")
        lines.append("#include <inttypes.h>")
    lines.append("")

    # Add size validation assert block
    lines.append("/* Size Checks against CSP_BUFFER_SIZE */")
    lines.append("static union {")
    for proc in procedures:
        proc_name = proc['name']
        lines.append(f"    struct {{ rpc_{program}_{proc_name}_request_t t; }} __attribute__((packed)) {proc_name}_req;")
        lines.append(f"    struct {{ rpc_{program}_{proc_name}_response_t t; }} __attribute__((packed)) {proc_name}_res;")
    lines.append(f"}} rpc_{program}_packed_tmp;")
    lines.append("")
    lines.append(f'static_assert(sizeof(rpc_msg_t) + sizeof(rpc_{program}_packed_tmp) <= CSP_BUFFER_SIZE,')
    lines.append(f'              "rpc_{program} messages are too large compared to CSP_BUFFER_SIZE");')
    lines.append("")

    # Init functions
    for proc in procedures:
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"

        non_default_fields = [f for f in proc['request'] if 'default' not in f and f.get('max_elements', 1) <= 1 and f['type'] != 'struct']

        if non_default_fields:
            init_params = ", ".join([f"{get_field_type(spec, f)} {f['name']}" for f in non_default_fields])
            lines.append(f"int {to_function_name(program, proc_name, 'init')}({request_typedef} *request, {init_params}) {{")
        else:
            lines.append(f"int {to_function_name(program, proc_name, 'init')}({request_typedef} *request) {{")

        if len(proc['request']) == 0:
            lines.append("    (void)request;")

        for field in proc['request']:
            if field.get('max_elements', 1) > 1:
                lines.append(f"    request->{field['name']}_count = 0;")

        for field in non_default_fields:
            if field['type'] == 'string':
                lines.append(f"    strncpy(request->{field['name']}, {field['name']}, sizeof(request->{field['name']}));")
                lines.append(f"    request->{field['name']}[sizeof(request->{field['name']}) - 1] = 0;")
            else:
                lines.append(f"    request->{field['name']} = {field['name']};")

        for field in proc['request']:
            if 'default' in field and field.get('max_elements', 1) <= 1 and field['type'] != 'struct':
                default_value = render_c_default(field)
                if field['type'] == 'string':
                    lines.append(f"    strncpy(request->{field['name']}, {default_value}, sizeof(request->{field['name']}));")
                    lines.append(f"    request->{field['name']}[sizeof(request->{field['name']}) - 1] = 0;")
                else:
                    lines.append(f"    request->{field['name']} = {default_value};")
        lines.append("    return 0;")
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

        if len(proc['request']) == 0:
            lines.append("    (void)request;")
        if len(proc['response']) == 0:
            lines.append("    (void)response;")
        lines.append("")

        lines.append(f"    int status = rpc_build_request(node, RPC_PROGRAM_{program_upper}, RPC_{program_upper}_{proc_upper}, &conn, &request_msg);")
        lines.append("    if (status != RPC_STATUS_OK) {")
        lines.append("        return status;")
        lines.append("    }")
        lines.append("")
        lines.append("    /* Add request fields */")

        if debug:
            lines.append(f'    printf("RPC: Sending {proc_upper} call\\n");')
            for field in proc['request']:
                lines.extend(get_debug_print_lines(spec, field, "", "request->", "    "))
            lines.append("")

        if use_mpack:
            lines.extend(get_mpack_encode_lines(spec, proc['request'], "request->", "request_msg", False))
        else:
            for field in proc['request']:
                lines.extend(get_push_lines(spec, field, "request->", "request_msg", False, "    "))
        lines.append("")

        lines.append("    rpc_send(conn, request_msg);")
        lines.append("")

        if maxresponses > 1:
            lines.append("    uint32_t max_req_responses = *numresponses;")
            lines.append("    uint32_t idx = 0;")
            lines.append("    while (idx < max_req_responses) {")

            if len(proc['response']) > 0 and not use_mpack:
                lines.append("        uint16_t offset = 0;")
                lines.append("")

            lines.append(f"        status = rpc_get_reply(conn, &reply_msg, {maxresponses}, idx, timeout);")
            lines.append("        if (status != RPC_STATUS_OK) {")
            lines.append("            rpc_disconnect(conn);")
            lines.append("            return status;")
            lines.append("        }")
            lines.append("")
            lines.append("        /* Extract response fields */")

            if use_mpack:
                lines.extend(get_mpack_decode_lines(spec, proc['response'], "response[idx].", "reply_msg", "        "))
            else:
                for field in proc['response']:
                    lines.extend(get_pop_lines(spec, field, "response[idx].", "reply_msg", "&offset", True, "        "))
            lines.append("")

            if debug:
                lines.append(f'        printf("RPC: Receiving {proc_upper} response idx=%"PRIu32"\\n", idx);')
                for field in proc['response']:
                    lines.extend(get_debug_print_lines(spec, field, "response[idx].", "response[idx].", "        "))
                lines.append("")

            lines.append("        uint32_t amount = reply_msg->reply.amount;")
            lines.append("        rpc_buffer_free(reply_msg);")
            lines.append("        idx++;")
            lines.append("")
            lines.append("        if (idx >= amount) {")
            lines.append("            break;")
            lines.append("        }")
            lines.append("    }")
            lines.append("")
            lines.append("    *numresponses = idx;")
        else:
            if len(proc['response']) > 0 and not use_mpack:
                lines.append("    uint16_t offset = 0;")
                lines.append("")

            lines.append("    status = rpc_get_reply(conn, &reply_msg, 1, 0, timeout);")
            lines.append("    if (status != RPC_STATUS_OK) {")
            lines.append("        rpc_disconnect(conn);")
            lines.append("        return status;")
            lines.append("    }")
            lines.append("")
            lines.append("    /* Extract response fields */")

            if use_mpack:
                lines.extend(get_mpack_decode_lines(spec, proc['response'], "response->", "reply_msg"))
            else:
                for field in proc['response']:
                    lines.extend(get_pop_lines(spec, field, "response->", "reply_msg", "&offset", True, "    "))
            lines.append("")

            if debug:
                lines.append(f'    printf("RPC: Receiving {proc_upper} response\\n");')
                for field in proc['response']:
                    lines.extend(get_debug_print_lines(spec, field, "response->", "response->", "    "))
                lines.append("")

            lines.append("    rpc_buffer_free(reply_msg);")

        lines.append("")
        lines.append("    return rpc_disconnect(conn);")
        lines.append("}")
        lines.append("")

    return "\n".join(lines)


def generate_server_implementation(spec: Dict[str, Any], debug: bool = False) -> str:
    program = spec['program']
    procedures = spec['procedures']
    use_mpack = get_rpc_version(spec) == 2

    lines = []
    lines.append(f'#include "rpc_{program}_server.h"')
    lines.append('#include <endian.h>')
    lines.append('#include <inttypes.h>')
    lines.append("#include <string.h>")
    lines.append("#include <assert.h>")
    if use_mpack:
        lines.append("#include <stddef.h>")
        lines.append("#include <mpack/mpack.h>")
    if debug:
        lines.append('#include <stdio.h>')
    lines.append("")

    # Add size validation assert block
    lines.append("/* Size Checks against CSP_BUFFER_SIZE */")
    lines.append("static union {")
    for proc in procedures:
        proc_name = proc['name']
        lines.append(f"    struct {{ rpc_{program}_{proc_name}_request_t t; }} __attribute__((packed)) {proc_name}_req;")
        lines.append(f"    struct {{ rpc_{program}_{proc_name}_response_t t; }} __attribute__((packed)) {proc_name}_res;")
    lines.append(f"}} rpc_{program}_packed_tmp;")
    lines.append("")
    lines.append(f'static_assert(sizeof(rpc_msg_t) + sizeof(rpc_{program}_packed_tmp) <= CSP_BUFFER_SIZE,')
    lines.append(f'              "rpc_{program} messages are too large compared to CSP_BUFFER_SIZE");')
    lines.append("")

    lines.append("/* Server Handler Dispatcher */")
    program_upper = to_macro_name(program)
    lines.append(f"int rpc_handle_calls_{program}(uint32_t procedure, csp_conn_t *conn, rpc_msg_t *call) {{")
    lines.append("")

    # Create unified buffers to minimize stack and overall BSS usage
    lines.append("    static union {")
    for proc in procedures:
        proc_name = proc['name']
        request_typedef = f"rpc_{program}_{proc_name}_request_t"
        lines.append(f"        {request_typedef} {proc_name};")
    lines.append("    } req_buf;")
    lines.append("")

    lines.append("    static union {")
    for proc in procedures:
        proc_name = proc['name']
        response_typedef = f"rpc_{program}_{proc_name}_response_t"
        maxresponses = proc.get('maxresponses', 1)
        lines.append(f"        {response_typedef} {proc_name}[{maxresponses}];")
    lines.append("    } res_buf;")
    lines.append("")

    lines.append("    memset(&req_buf, 0, sizeof(req_buf));")
    lines.append("    memset(&res_buf, 0, sizeof(res_buf));")
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
        lines.append(f"            {request_typedef} *request = &req_buf.{proc_name};")
        lines.append(f"            {response_typedef} *response = res_buf.{proc_name};")

        if maxresponses > 1:
            lines.append(f"            uint32_t numresponses = {maxresponses};")

        if len(proc['request']) > 0 and not use_mpack:
            lines.append("            uint16_t offset = 0;")
        lines.append("")

        if use_mpack:
            lines.extend(get_mpack_server_decode_lines(spec, proc['request']))
        else:
            for field in proc['request']:
                lines.extend(get_pop_lines(spec, field, "request->", "call", "&offset", False, "            "))
        lines.append("")

        if debug:
            lines.append(f'            printf("RPC: Received {proc_upper} call\\n");')
            for field in proc['request']:
                lines.extend(get_debug_print_lines(spec, field, "", "request->", "            "))
            lines.append("")

        if maxresponses > 1:
            lines.append(f"            {to_function_name(program, proc_name, 'host')}(request, response, &numresponses);")
            lines.append("")
            lines.append(f"            if (numresponses == 0) return sizeof({response_typedef});")
            lines.append(f"            if (numresponses > {maxresponses}) return RPC_STATUS_ERR_INVALID;")
        else:
            lines.append(f"            {to_function_name(program, proc_name, 'host')}(request, response);")
        lines.append("")

        if maxresponses > 1:
            lines.append("            for (uint32_t i_resp = 0; i_resp < numresponses; i_resp++) {")
            if debug:
                lines.append(f'                printf("RPC: Sending {proc_upper} response idx=%"PRIu32"\\n", i_resp);')
                for field in proc['response']:
                    lines.extend(get_debug_print_lines(spec, field, "response[i_resp].", "response[i_resp].", "                "))

            lines.append("                rpc_msg_t *reply = rpc_result_prepare(call, numresponses, i_resp);")
            if use_mpack:
                lines.extend(get_mpack_encode_lines(spec, proc['response'], "response[i_resp].", "reply", True, "                "))
            else:
                for field in proc['response']:
                    lines.extend(get_push_lines(spec, field, "response[i_resp].", "reply", True, "                "))

            lines.append(f"                rpc_send_reply(conn, reply);")
            lines.append("            }")
        else:
            if debug:
                lines.append(f'            printf("RPC: Sending {proc_upper} response\\n");')
                for field in proc['response']:
                    lines.extend(get_debug_print_lines(spec, field, "response->", "response->", "            "))

            lines.append("            rpc_msg_t *reply = rpc_result_prepare(call, 1, 0);")
            if use_mpack:
                lines.extend(get_mpack_encode_lines(spec, proc['response'], "response->", "reply", True, "            "))
            else:
                for field in proc['response']:
                    lines.extend(get_push_lines(spec, field, "response->", "reply", True, "            "))

            lines.append(f"            rpc_send_reply(conn, reply);")

        lines.append("            break;")
        lines.append("        }")

    lines.append("        default:")
    lines.append("            break;")
    lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_files(json_file: str, output_dir: Optional[str] = None, mode: str = 'both', debug: bool = False, copy_headers: bool = True):
    spec = None
    try:
        with open(json_file, 'r') as f:
            spec = json.load(f)
    except Exception:
        if not json5_available:
            print("Warning: json5 module not available, falling back to standard json using '{}' as input.".format(json_file.replace('.json5', '.json')))
            with open(json_file.replace('.json5', '.json'), 'r') as f:
                spec = json.load(f)
        else:
            raise

    validate_rpc_features(spec)
    program = spec['program']

    if mode not in ('client', 'server', 'both'):
        raise ValueError("mode must be 'client', 'server', or 'both'")

    if output_dir is None:
        output_dir = os.path.dirname(json_file) or '.'

    os.makedirs(output_dir, exist_ok=True)

    common_header = os.path.join(output_dir, f"rpc_{program}.h")
    with open(common_header, 'w') as f:
        f.write(generate_common_header(spec))

    if mode in ('client', 'both'):
        client_header = os.path.join(output_dir, f"rpc_{program}_client.h")
        with open(client_header, 'w') as f:
            f.write(generate_client_header(spec))
        client_file = os.path.join(output_dir, f"rpc_{program}_client.c")
        with open(client_file, 'w') as f:
            f.write(generate_client_implementation(spec, debug))

    if mode in ('server', 'both'):
        server_header = os.path.join(output_dir, f"rpc_{program}_server.h")
        with open(server_header, 'w') as f:
            f.write(generate_server_header(spec))
        server_file = os.path.join(output_dir, f"rpc_{program}_server.c")
        with open(server_file, 'w') as f:
            f.write(generate_server_implementation(spec, debug))

    if copy_headers:
        parent_dir = os.path.join(output_dir, '..')
        shutil.copy(common_header, parent_dir)
        if mode in ('client', 'both'):
            shutil.copy(client_header, parent_dir)
        if mode in ('server', 'both'):
            shutil.copy(server_header, parent_dir)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 generate_rpc.py <json_file> [output_dir] [mode] [debug] [no-copy]")
        print("mode = client | server | both (default)")
        print("debug = debug | (omitted for false)")
        print("no-copy = disables copying header files to the parent directory (../)")
        sys.exit(1)

    json_file = sys.argv[1]
    output_dir = None
    mode = 'both'
    debug = False
    copy_headers = True

    for i in range(2, len(sys.argv)):
        arg = sys.argv[i].lower()
        if arg in ('client', 'server', 'both'):
            mode = arg
        elif arg == 'debug':
            debug = True
        elif arg == 'no-copy':
            copy_headers = False
        else:
            if output_dir is not None:
                print(f"Error: Unrecognized argument: {sys.argv[i]}")
                sys.exit(1)
            output_dir = sys.argv[i]

    if not os.path.exists(json_file):
        print(f"Error: File not found: {json_file}")
        sys.exit(1)

    try:
        generate_files(json_file, output_dir, mode, debug, copy_headers)
    except Exception as e:
        print("FATAL ERROR GENERATING CODE:")
        traceback.print_exc()
        sys.exit(1)
