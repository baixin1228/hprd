src = ['x11_extensions_input.c']

frame_buffer_input = static_library('frame-buffer-input-lib',
	src,
	include_directories : include_directories('../include'),
	dependencies: [dependency('x11'), dependency('xproto'), dependency('xext')]
  )

frame_buffer_input_dep = declare_dependency(
	include_directories : include_directories('../include'),
	link_with : frame_buffer_input,
	link_args : ['-lrt']
	)