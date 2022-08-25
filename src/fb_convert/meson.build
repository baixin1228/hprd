frame_conv_libs = []
frame_conv_deps = []

if build_machine.system() == 'linux'
    subdir('libswscale')
endif

frame_conv_src = ['fb_convert.c']
fb_convert = static_library('frame-convert-lib',
	frame_conv_src,
	include_directories : include_directories('../include'),
	dependencies: frame_conv_deps,
	link_whole : frame_conv_libs,
  )

frame_conv_dep = declare_dependency(
	link_with : fb_convert,
	)