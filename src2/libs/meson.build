common_libs = shared_library('common_libs',
	['buffer_pool.c', 'queue.c'],
	include_directories : include_directories('../include')
  )

common_dep = declare_dependency(
	include_directories : include_directories('../include'),
	link_with : common_libs,
	)