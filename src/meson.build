common_src = ['ld_help.c']
common_src = static_library('frame-buffer-output-lib',
	common_src,
	include_directories : include_directories('include'),
  )

common_src_dep = declare_dependency(
	include_directories : include_directories('include'),
	link_with : common_src,
	)

if get_option('build_target') == 'server'
  subdir('fb_in')
endif

if get_option('build_target') == 'client'
  subdir('fb_out')
endif

if get_option('build_target') == 'all'
  subdir('fb_in')
  subdir('fb_out')
endif

subdir('platform')