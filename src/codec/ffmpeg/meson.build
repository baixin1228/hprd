ffmpeg_codec_src = ['ffmpeg_enc.c']
ffmpeg_codec_deps = [ dependency('libavformat'),
                      dependency('libavcodec'),
                      dependency('libavutil')]

ffmpeg_codec_lib = static_library('ffmpeg-codec-lib', ffmpeg_codec_src,
                                   dependencies: ffmpeg_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

ffmpeg_codec_dep = declare_dependency(link_with: ffmpeg_codec_lib,
                                       dependencies: ffmpeg_codec_deps)

encodec_libs += [ ffmpeg_codec_lib ]
encodec_deps += [ ffmpeg_codec_dep ]