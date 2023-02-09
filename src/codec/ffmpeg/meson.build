ffmpeg_codec_deps = [ 	common_dep,
                        dependency('libavformat'),
						dependency('libavcodec'),
						dependency('libavutil')]

ffmpeg_encodec_lib = shared_library(
    'ffmpegenc', 
    ['ffmpeg_enc.c', 'ffmpeg_util.c'],
    dependencies: [ffmpeg_codec_deps, dependency('glib-2.0')],
    include_directories: ['.', '../../include'],
    install : true)

#ffmpeg_decodec_lib = shared_library(
#    'ffmpegdecodec',
#    ['ffmpeg_dec.c'],
#    dependencies: [ffmpeg_codec_deps, dependency('glib-2.0')],
#    include_directories: [ include_directories('.'),
#    include_directories('../../include')]
#    )