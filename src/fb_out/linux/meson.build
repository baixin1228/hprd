src = ['sdl_fb_out.c']

sdl_fb_out = static_library('sdl-fb-out-lib',
	src,
	include_directories : include_directories('../../include'),
	dependencies: [dependency('sdl2')]
  )

sdl_fb_out_dep = declare_dependency(
	include_directories : include_directories('../../include'),
	link_with : sdl_fb_out,
	link_args : ['-lrt']
	)

fb_out_libs = [sdl_fb_out]
fb_out_deps = [sdl_fb_out_dep]