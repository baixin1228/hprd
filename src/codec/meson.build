if build_machine.system() == 'linux'
    subdir('vaapi')
endif

encodec_src = ['encodec.c']
encodec = static_library('encodec-lib',
	encodec_src,
	include_directories : include_directories('../include'),
	dependencies: encodec_deps,
	link_whole : encodec_libs,
  )

encodec_dep = declare_dependency(
	link_with : encodec,
	)