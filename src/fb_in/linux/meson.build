src = ['x11_extensions_in.c']

x11_extensions_in = static_library('x11-extensions-in-lib',
	src,
	include_directories : include_directories('../../include'),
	dependencies: [dependency('x11'), dependency('xproto'), dependency('xext')]
  )

x11_extensions_in_dep = declare_dependency(
	include_directories : include_directories('../../include'),
	link_with : x11_extensions_in,
	link_args : ['-lrt']
	)

fb_in_libs = [x11_extensions_in]
fb_in_deps = [x11_extensions_in_dep]