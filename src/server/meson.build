hprd_server = executable('hprd_server', ['main.c'], 
				include_directories : include_directories('../include'),
				dependencies : [frame_buffer_input_dep],
				link_whole : [frame_buffer_input],
				link_args : ['-Wl,-T,@0@/@1@'.format(meson.current_source_dir(), 'main_link.lds')],
				#link_args : ['--verbose','-Wl,-T,@0@/@1@'.format(meson.current_source_dir(), 'main_link.lds')],
				install : true
			)

test('hprd_server_test', hprd_server, is_parallel : false)
