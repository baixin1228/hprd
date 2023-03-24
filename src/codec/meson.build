if build_target == 'server' or build_target == 'all'
        ffmpeg_enc_lib = static_library('ffmpeg_enc',
                                ['ffmpeg_enc.c', 'ffmpeg_util.c'],
                                dependencies: [common_deps,
                                dependency('libavcodec'),
                                dependency('libavformat'),
                                dependency('libswscale'),
                                dependency('libavutil')],
                                include_directories : '../include')

        server_libs += ffmpeg_enc_lib

        openh264_enc_lib = static_library('openh264_enc',
                                ['openh264_enc.c'],
                                dependencies: [common_deps,
                                dependency('openh264')],
                                include_directories : '../include')

        server_libs += openh264_enc_lib
endif

if build_target == 'client' or build_target == 'all'
        openh264_dec_lib = static_library('openh264_dec',
                                ['openh264_dec.c'],
                                dependencies: [common_deps,
                                dependency('openh264')],
                                include_directories : '../include')

        client_libs += openh264_dec_lib
endif