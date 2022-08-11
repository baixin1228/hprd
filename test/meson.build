xcb_copy_test = executable('xcb_copy_test', 'xcb_copy_test.c', 
					dependencies : [dependency('xcb')]
				)
test('xcb_copy_performance_test', xcb_copy_test, is_parallel : false)

xcb_shm_test = executable('xcb_shm_test', 'xcb_shm_test.c', 
					dependencies : [dependency('xcb'), dependency('xcb-shm')]
				)
test('xcb_shm_performance_test', xcb_shm_test, is_parallel : false)

x11_extensions_test = executable('x11_extensions_test', 'x11_extensions_test.c', 
					dependencies : [dependency('x11'), dependency('xproto'), dependency('xext')]
				)
test('x11_extensions_performance_test', x11_extensions_test, suite: 'x11_extensions_test', is_parallel : false)