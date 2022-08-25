encodec_libs = []
encodec_deps = []
decodec_libs = []
decodec_deps = []

if build_machine.system() == 'linux'
    subdir('vaapi')
    subdir('libx264')
    subdir('libx265')
    subdir('ffmpeg')
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

decodec_src = ['decodec.c']
decodec = static_library('decodec-lib',
	decodec_src,
	include_directories : include_directories('../include'),
	dependencies: decodec_deps,
	link_whole : decodec_libs,
  )

decodec_dep = declare_dependency(
	link_with : decodec,
	)