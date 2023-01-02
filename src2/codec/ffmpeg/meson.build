ffmpeg_encodec_src = ['ffmpeg_enc.c', 'ffmpeg_util.c']
ffmpeg_codec_deps = [ 	dependency('libavformat'),
						dependency('libavcodec'),
						dependency('libavutil')]

ffmpeg_encodec_lib = static_library('ffmpeg-encodec-lib', ffmpeg_encodec_src,
                                   dependencies: ffmpeg_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

ffmpeg_encodec_dep = declare_dependency(link_with: ffmpeg_encodec_lib,
                                       dependencies: ffmpeg_codec_deps)

encodec_libs += [ ffmpeg_encodec_lib ]
encodec_deps += [ ffmpeg_encodec_dep ]


ffmpeg_decodec_src = ['ffmpeg_dec.c']
ffmpeg_decodec_lib = static_library('ffmpeg-decodec-lib', ffmpeg_decodec_src,
                                   dependencies: ffmpeg_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

ffmpeg_decodec_dep = declare_dependency(link_with: ffmpeg_decodec_lib,
                                       dependencies: ffmpeg_codec_deps)

decodec_libs += [ ffmpeg_decodec_lib ]
decodec_deps += [ ffmpeg_decodec_dep ]