========================================
RPC (Remote Procedure Call) Framework
========================================

Overview
========

The RPC library implements a lightweight, efficient Remote Procedure Call mechanism for distributed embedded systems. Built on top of CSP (Cubesat Space Protocol), it enables seamless communication between nodes in satellite systems, supporting both ARM-based satellite modules and x86-based ground stations.

**Features:**

- Simple client-server architecture
- Binary protocol for minimal overhead
- Support for multiple programs and procedures per node
- Automatic code generation from JSON5 interface definitions
- Flexible serialization of primitives, strings, and complex types
- Built-in program discovery mechanisms
- Cross-platform compatibility (ARM/x86)

Architecture
============

Message Protocol
----------------

The RPC framework operates over CSP (Cubesat Space Protocol) and uses a binary message format with two main message types:

**RPC_MSG_CALL** - Client Request
  - Program ID (32-bit)
  - Procedure ID (32-bit)
  - Serialized argument data (variable length)

**RPC_MSG_REPLY** - Server Response
  - Amount (32-bit counter)
  - Index (32-bit sequence)
  - Serialized result data (variable length)

All multi-byte integers are serialized in big-endian (network byte order).

Program and Procedure Registration
-----------------------------------

Programs are registered statically using the ``RPC_DECLARE_PROGRAM`` macro, which places them in a special ``.rpc_programs`` section. Each program contains:

- **Program ID**: Unique 32-bit identifier
- **Program Name**: Text description
- **Handler**: Callback function to dispatch procedures
- **Procedures**: Array of available procedures
- **Context Data**: Optional user-defined data passed to handler

Supported Data Types
====================

The framework supports the following primitive types:

- **Integers**: int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
- **Floating Point**: float, double
- **Strings**: Null-terminated character arrays
- **Buffers**: Binary data with length prefix
- **Enums**: emun types

Framework API
=============

Client Side
-----------

Server Side
-----------

**Initialization**

.. code-block:: c

    int rpc_start_server(rpc_server_t *me);

Start the RPC server main loop.

Error Handling
==============

Status codes are returned from initialization and RPC call functions:

- ``RPC_STATUS_OK`` (0): Operation successful
- ``RPC_STATUS_ERR_COULD_NOT_CONNECT`` (-1): Connection failed
- ``RPC_STATUS_ERR_TIMEOUT`` (-2): Operation timeout
- ``RPC_STATUS_ERR_NO_MEMORY`` (-3): Memory allocation failure
- ``RPC_STATUS_ERR_INVALID`` (-4): Invalid parameters
- ``RPC_STATUS_ERR_NOT_FOUND`` (-5): Program/procedure not found
- ``RPC_STATUS_ERR_NOT_SUPPORTED`` (-6): Feature not supported
- ``RPC_STATUS_ERR_INUSE`` (-7): Resource in use

RPC Code Generator
===================

Automated C Code Generation
----------------------------

The RPC code generator (``generate_rpc.py``) automatically creates client and server code from JSON5 interface specifications, eliminating manual serialization/deserialization boilerplate.

**Usage**

.. code-block:: bash

    python3 generate_rpc.py <json5_file> [output_dir] [mode]

**Arguments**

- ``<json5_file>``: Path to JSON5 interface definition
- ``[output_dir]``: Output directory (defaults to input file directory)
- ``[mode]``: ``client``, ``server``, or ``both`` (default: ``both``)

**Generated Output Files**

- ``rpc_<program>.h``: Common header with structure definitions and program constants
- ``rpc_<program>_client.h``: Client API declarations
- ``rpc_<program>_client.c``: Client implementation with init and invoke functions
- ``rpc_<program>_server.h``: Server handler declarations
- ``rpc_<program>_server.c``: Server dispatcher and response serialization

JSON5 Interface Definition
--------------------------

Interface specifications define programs and their procedures:

.. code-block:: json5

    {
        program: 'program_name',           // Program identifier
        address: 0x10000000,               // Unique 32-bit program ID
        enums: [
            {
                name: 'some_enum_t',       // Name of the enum type
                size: 'u8',                // Size of the enum
                items: [
                    {
                        name: 'field_name',
                        value: '0',        // Optional value
                        description: 'Optional field description',
                    }
                ]
            }
        ],
        procedures: [
            {
                name: 'procedure_name',    // Procedure identifier
                index: 0,                  // Procedure ID within program
                request: [                 // Request parameter list
                    {
                        name: 'field_name',
                        type: 'u32',
                        description: 'Optional field description',
                        default: '0'       // Optional default value
                    }
                ],
                response: [                // Response value list
                    {
                        name: 'result',
                        type: 'i32',
                        description: 'Return value'
                    }
                ]
            }
        ]
    }

Supported Type Mapping
~~~~~~~~~~~~~~~~~~~~~~

===========  ===========  ===================================
JSON5 Type   C Type       Description
===========  ===========  ===================================
u8, x8       uint8_t      Unsigned 8-bit integer
u16, x16     uint16_t     Unsigned 16-bit integer
u32, x32     uint32_t     Unsigned 32-bit integer
u64, x64     uint64_t     Unsigned 64-bit integer
i8           int8_t       Signed 8-bit integer
i16          int16_t      Signed 16-bit integer
i32          int32_t      Signed 32-bit integer
i64          int64_t      Signed 64-bit integer
f32          float        32-bit floating point
f64          double       64-bit floating point
string       char *       Null-terminated string
enum         enum         Enum type
===========  ===========  ===================================

String Field Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~

Strings are handled specially:

- **Request strings**: Add ``max_length`` to allocate local buffer

  .. code-block:: json5

      { name: 'text', type: 'string', max_length: 256 }

- **Response strings**: Generated as ``const char *`` pointers (caller must ensure lifetime)

Enum Field Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~

Enums are handled specially:

- **enums**: Add ``enum_name`` to point to the enum name

  .. code-block:: json5

      { name: 'field_name', type: 'enum', enum_name: 'some_enum_t' }

Generated Functions
~~~~~~~~~~~~~~~~~~~

**Client-side Functions**

- ``rpc_<program>_<procedure>_init()`` - Initialize request struct with non-default parameters
- ``rpc_<program>_<procedure>()`` - Invoke RPC call with initialized request, return results

**Server-side Functions**

- ``rpc_<program>_<procedure>_host()`` - **User must implement** - Core procedure logic
- ``rpc_handle_calls_<program>()`` - **Auto-generated** - Dispatcher routing incoming calls

Example
-------

**Input: dsu.json5**

.. code-block:: json5

    {
        program: 'dsu',
        address: 0x10000000,
        procedures: [{
            name: 'peek_element',
            index: 0,
            request: [
                { name: 'block_id', type: 'u32' },
                { name: 'num_elements', type: 'u32', default: '1' }
            ],
            response: [
                { name: 'vaddr', type: 'x64' },
                { name: 'size_actual', type: 'i32' }
            ]
        }]
    }

**Generate Code**

.. code-block:: bash

    python3 generate_rpc.py dsu.json5

**Output Files**

- ``rpc_dsu.h`` - Common structures and constants
- ``rpc_dsu_client.h`` - Client API
- ``rpc_dsu_client.c`` - Client implementation
- ``rpc_dsu_server.h`` - Server callback declarations
- ``rpc_dsu_server.c`` - Server dispatcher

**Server Implementation**

The generated ``rpc_dsu_server.h`` declares:

.. code-block:: c

    void rpc_dsu_peek_element_host(uint16_t node, uint32_t timeout,
                                   const rpc_dsu_peek_element_request_t *request,
                                   rpc_dsu_peek_element_response_t *response);

You implement this function in your server code to handle the RPC call.

**Client Usage**

.. code-block:: c

    rpc_dsu_peek_element_request_t request = rpc_dsu_peek_element_init(42, 10);
    rpc_dsu_peek_element_response_t response;
    int result = rpc_dsu_peek_element(remote_node, &request, &response);
    if (result == RPC_STATUS_OK) {
        printf("vaddr: 0x%llx\n", response.vaddr);
    }

Meson Build Integration
-----------------------

The build system includes an automatic code generation step via Meson ``generator`` target. When you run the build, the generator will process your JSON5 files and produce the necessary C source and header files before compilation.:

.. code-block:: bash

    meson setup build-0
    meson compile -C build-0

Generated files are placed in the build tree:

.. code-block:: text

    build-0/lib/cpstorage/rpc_dsu.h
    build-0/lib/cpstorage/rpc_dsu_client.h
    build-0/lib/cpstorage/rpc_dsu_client.c
    build-0/lib/cpstorage/rpc_dsu_server.h
    build-0/lib/cpstorage/rpc_dsu_server.c

Integration Steps
-----------------

1. Define your RPC interface in JSON5
2. Run the generator to produce headers and sources
3. Implement server callback functions (``*_host()`` functions)
4. Link generated code into your application
5. Call client functions from remote nodes or server initialization from local nodes

