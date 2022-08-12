if build_machine.system() == 'linux'
    subdir('linux')
endif

fb_out_src = ['fb_out.c']
frame_buffer_output = static_library('frame-buffer-output-lib',
	fb_out_src,
	include_directories : include_directories('../include'),
	dependencies: fb_out_deps,
	link_whole : fb_out_libs,
  )

frame_buffer_output_dep = declare_dependency(
	include_directories : include_directories('../include'),
	link_with : frame_buffer_output,
	)