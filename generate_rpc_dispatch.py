#!/usr/bin/env python3
"""Generate direct RPC program dispatch from one or more JSON/JSON5 specs."""

import json
import sys
from pathlib import Path

try:
    import json5
except ImportError:
    json5 = None


def load_spec(path: Path):
    try:
        with path.open() as spec_file:
            return (json5 or json).load(spec_file)
    except Exception:
        fallback_path = path.with_suffix(".json")
        if json5 is None and fallback_path.exists():
            with fallback_path.open() as spec_file:
                return json.load(spec_file)
        raise


def to_macro_name(name: str) -> str:
    return "".join(
        f"_{char}" if char.isupper() and index > 0 else char
        for index, char in enumerate(name)
    ).upper()


def escape_depfile_path(path: Path) -> str:
    return str(path).replace("\\", "\\\\").replace(" ", "\\ ").replace("#", "\\#").replace("$", "$$")


def generate_dispatch(output: Path, depfile: Path, specs):
    programs = []
    program_ids = set()
    program_names = set()
    for spec_path in specs:
        spec = load_spec(spec_path)
        program = spec["program"]
        program_id = spec["address"]
        if program in program_names or program_id in program_ids:
            raise ValueError(f"Duplicate RPC program name or ID: {program} ({program_id})")
        programs.append((program, program_id))
        program_names.add(program)
        program_ids.add(program_id)

    lines = [
        '#include "rpc_server.h"',
        *[f'#include "rpc_{program}_server.h"' for program, _ in programs],
        "",
        "int rpc_dispatch_program(uint32_t program, uint32_t procedure, csp_packet_t *request, rpc_msg_t *call) {",
        "    switch (program) {",
    ]
    for program, program_id in programs:
        lines.extend([
            f"        case RPC_PROGRAM_{to_macro_name(program)}:",
            f"            return rpc_handle_calls_{program}(procedure, request, call);",
        ])
    lines.extend([
        "        default:",
        "            return RPC_STATUS_ERR_NOT_FOUND;",
        "    }",
        "}",
        "",
    ])
    output.write_text("\n".join(lines))
    absolute_output = output.resolve()
    dependencies = " ".join(escape_depfile_path(spec.resolve()) for spec in specs)
    depfile.write_text(f"{escape_depfile_path(absolute_output)}: {dependencies}\n")


def main():
    if len(sys.argv) < 4:
        print("Usage: generate_rpc_dispatch.py <output.c> <depfile> <spec.json|spec.json5> [...]")
        return 2
    generate_dispatch(Path(sys.argv[1]), Path(sys.argv[2]), [Path(path) for path in sys.argv[3:]])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"Failed to generate RPC dispatcher: {error}", file=sys.stderr)
        sys.exit(1)
