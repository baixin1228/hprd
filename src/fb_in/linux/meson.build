src = ['x11_extensions_in.c']

x11_extensions_in = static_library('x11-extensions-in-lib',
	src,
	include_directories : [include_directories('../../include'), include_directories('..')],
	dependencies: [dependency('x11'), dependency('xproto'), dependency('xext')]
  )

x11_extensions_in_dep = declare_dependency(
	link_with : x11_extensions_in,
	link_args : ['-lrt']
	)

fb_in_libs = [x11_extensions_in]
fb_in_deps = [x11_extensions_in_dep]