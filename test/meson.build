xcb_copy_test = executable('xcb_copy_test', 'xcb_copy_test.c', 
					dependencies : [dependency('xcb')]
				)
test('xcb_copy_performance_test', xcb_copy_test, is_parallel : false)


xcb_shm_dep = dependency('xcb-shm', required : false)
if xcb_shm_dep.found()
	xcb_shm_test = executable('xcb_shm_test', 'xcb_shm_test.c', 
						dependencies : [dependency('xcb'), xcb_shm_dep]
					)
	test('xcb_shm_performance_test', xcb_shm_test, is_parallel : false)
endif


x11_extensions_test = executable('x11_extensions_test', 'x11_extensions_test.c', 
					dependencies : [dependency('x11'), dependency('xproto'), dependency('xext')]
				)
test('x11_extensions_performance_test', x11_extensions_test, suite: 'x11_extensions_test', is_parallel : false)

blockdpy_test = executable('blockdpy_test', 'blockdpy.c', 
					dependencies : [dependency('x11'), dependency('xcb'), dependency('xext')]
				)
test('blockdpy_test', x11_extensions_test, suite: 'blockdpy_test', is_parallel : false)
