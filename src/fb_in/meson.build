if build_machine.system() == 'linux'
    subdir('linux')
endif

fb_in_src = ['fb_in.c']
frame_buffer_input = static_library('frame-buffer-input-lib',
	fb_in_src,
	include_directories : include_directories('../include'),
	dependencies: fb_in_deps,
	link_whole : fb_in_libs,
  )

frame_buffer_input_dep = declare_dependency(
	include_directories : include_directories('../include'),
	link_with : frame_buffer_input,
	)