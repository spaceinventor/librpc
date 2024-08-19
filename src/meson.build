lib_src = [
    'dsu_rpc.c',
]

csp_dep = subproject('csp').get_variable('csp_dep').partial_dependency(links: false, includes: true)
csp_dep_static = subproject('csp').get_variable('csp_dep')

lib_deps = [csp_dep]

dsu_rpc = library('dsu_rpc', lib_src, include_directories: [include, include_directories('.')], dependencies: lib_deps)
dsu_rpc_dep = declare_dependency(include_directories : include, link_with : dsu_rpc, dependencies: lib_deps)
