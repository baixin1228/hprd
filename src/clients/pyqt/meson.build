shared_library('pyqt_proxy', 'pyqt_proxy.c',
		include_directories : '../../include',
		dependencies: client_deps,
		link_whole: client_static_lib,
		link_args : ['-ldl', '-lpthread'],
	    install : true)