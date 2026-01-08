lib_src = [
    'rpc.c',
]

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)

lib_deps = [csp_dep]

rpc = library('rpc', lib_src, include_directories: [include, include_directories('.')], dependencies: lib_deps, install: true, soversion: meson.project_version())
rpc_dep = declare_dependency(include_directories : include, link_with : rpc, dependencies: lib_deps)

pkg = import('pkgconfig')
pkg.generate(
	rpc,
	subdirs: ['.'],
    requires: ['csp']
)
