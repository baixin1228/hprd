codecs_source = files('ffmpeg_enc.c')

codecs_lib = static_library('common',
                        codecs_source,
                        include_directories : inc)

server_dep += codecs_lib