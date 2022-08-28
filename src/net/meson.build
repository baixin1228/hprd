rt_net_piple_src = [
	'rt_net_piple/rt_net_socket.c',
	'rt_net_piple/rt_net_piple.c'
]

rt_net_piple = static_library('rt-net-piple-lib',
				rt_net_piple_src, 
				include_directories : [include_directories('../include'), include_directories('rt_net_piple')],
			)

rt_net_piple_dep = declare_dependency(
	include_directories : include_directories('rt_net_piple'),
	dependencies: [dependency('threads')],
	link_with : rt_net_piple,
	)

executable('rt_net_client_test', ['test/client.c'], 
				include_directories : include_directories('../include'),
				dependencies : rt_net_piple_dep,
			)

executable('rt_net_server_test', ['test/server.c'], 
				include_directories : include_directories('../include'),
				dependencies : rt_net_piple_dep,
			)