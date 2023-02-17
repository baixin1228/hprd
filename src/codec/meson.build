ffmpeg_enc_lib = static_library('common',
                        ['ffmpeg_enc.c', 'ffmpeg_util.c'],
                        dependencies: [common_deps,
                        dependency('libavcodec'),
                        dependency('libavformat'),
                        dependency('libswscale'),
                        dependency('libavutil')],
                        include_directories : '../include')

server_libs += ffmpeg_enc_lib