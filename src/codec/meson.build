codecs_source = files('ffmpeg_enc.c', 'ffmpeg_util.c')

codecs_lib = static_library('common',
                        codecs_source,
                        dependencies: [common_deps, dependency('libavcodec'),
                        dependency('libavcodec'), dependency('libavformat'),
                        dependency('libavutil')],
                        include_directories : '../include')

server_libs += codecs_lib