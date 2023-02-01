ffmpeg_codec_deps = [ 	dependency('libavformat'),
						dependency('libavcodec'),
						dependency('libavutil')]

ffmpeg_encodec_lib = static_library(
    'ffmpeg-encodec-lib', 
    ['ffmpeg_enc.c', 'ffmpeg_util.c'],
    dependencies: ffmpeg_codec_deps,
    include_directories: [ include_directories('.'),
    include_directories('../../include')]
    )

ffmpeg_decodec_lib = static_library(
    'ffmpeg-decodec-lib', 
    ['ffmpeg_dec.c'],
    dependencies: ffmpeg_codec_deps,
    include_directories: [ include_directories('.'),
    include_directories('../../include')]
    )