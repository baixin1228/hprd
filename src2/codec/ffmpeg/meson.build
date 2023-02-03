ffmpeg_codec_deps = [ 	dependency('libavformat'),
						dependency('libavcodec'),
						dependency('libavutil')]

ffmpeg_encodec_lib = static_library(
    'ffmpegencodec', 
    ['ffmpeg_enc.c', 'ffmpeg_util.c'],
    dependencies: ffmpeg_codec_deps,
    include_directories: [ include_directories('.'),
    include_directories('../../include')]
    )

ffmpeg_decodec_lib = static_library(
    'ffmpegdecodec',
    ['ffmpeg_dec.c'],
    dependencies: ffmpeg_codec_deps,
    include_directories: [ include_directories('.'),
    include_directories('../../include')]
    )