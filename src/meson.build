common_src = ['ld_help.c', 'util.c', 'buffer.c', 'mini_ring_queue.c']

common = static_library('common-lib',
	common_src,
	dependencies : [ dependency('openmp') ],
	include_directories : include_directories('include'),
  )

common_dep = declare_dependency(
	include_directories : include_directories('include'),
	link_with : common,
	)

subdir('fb_convert')

subdir('codec')

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