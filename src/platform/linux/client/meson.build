client_deps = [	common_src_dep,
				frame_conv_dep,
				frame_buffer_input_dep, 
				frame_buffer_output_dep, 
				encodec_dep
				]

client_libs = [	frame_convert,
				frame_buffer_input,
				frame_buffer_output,
				encodec
				]

hprd_client = executable('hprd_client', ['client.c'], 
				include_directories : include_directories('../include'),
				dependencies : client_deps,
				link_whole : client_libs,
				link_args : ['-Wl,-T,@0@/@1@'.format(meson.current_source_dir(), 'main_link.lds')],
				install : true
			)

test('hprd_client_test', hprd_client, is_parallel : false, timeout: 100)