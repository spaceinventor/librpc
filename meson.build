project('rpc', 'c', version: '1.1.0')

include = include_directories('include')

subdir('src')

gen_doc = find_program('gen-sidoc', required: false)

gen_args = [files('generate_rpc.py').get(0).full_path(), '@INPUT@', '@BUILD_DIR@']

if get_option('debugprint') == true
    gen_args += ['debug']
endif

gen_rpc_server = generator(find_program('python3'), arguments: gen_args + ['server'], output: ['rpc_@BASENAME@_server.c', 'rpc_@BASENAME@_server.h', 'rpc_@BASENAME@.h'])
gen_rpc_client = generator(find_program('python3'), arguments: gen_args + ['client'], output: ['rpc_@BASENAME@_client.c', 'rpc_@BASENAME@_client.h', 'rpc_@BASENAME@.h'])
gen_rpc_both = generator(find_program('python3'), arguments: gen_args + ['both'], output: ['rpc_@BASENAME@_server.c', 'rpc_@BASENAME@_server.h', 'rpc_@BASENAME@_client.c', 'rpc_@BASENAME@_client.h', 'rpc_@BASENAME@.h'])

if gen_doc.found()
  doc_out = meson.current_build_dir() / '@OUTPUT@'
  doc = custom_target('doc',
    output : 'rpc.pdf',
    input : ['doc/rpc.rst', 'README.rst'],
    command : ['gen-sidoc', '-t', 'MAN', 'RPC', '@INPUT0@', '-o', doc_out],
    build_by_default: false
  )
endif

doxygen = find_program('doxygen', required : false)
if doxygen.found()
  cdata = configuration_data()
  cdata.set('VERSION', meson.project_version())

  if find_program('dot', required : false).found()
    cdata.set('HAVE_DOT', 'YES')
  else
    cdata.set('HAVE_DOT', 'NO')
  endif

  cdata.set('TOP_SRCDIR', meson.project_source_root())
  cdata.set('TOP_BLDDIR', meson.project_build_root())

  doxyfile = configure_file(input: 'Doxyfile.in',
                            output: 'Doxyfile',
                            configuration: cdata,
                            install: false)

  html_target = custom_target('api-docs',
                              input: doxyfile,
                              output: 'html',
                              command: [doxygen, doxyfile],
                              build_by_default: false)
endif
