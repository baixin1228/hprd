executable('hprd_server', ['main.c'], 
				include_directories : include_directories('../include'),
				dependencies : [frame_buffer_input_dep],
				link_whole : frame_buffer_input,
				#link_args : ['--verbose'],
				link_depends : ['main_link.lds'],
				install : true
			)
