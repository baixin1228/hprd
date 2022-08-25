x264_codec_src = ['libx264_enc.c']
x264_codec_deps = [ dependency('x264')]

x264_codec_lib = static_library('x264-codec-lib', x264_codec_src,
                                   dependencies: x264_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

x264_codec_dep = declare_dependency(link_with: x264_codec_lib,
                                       dependencies: x264_codec_deps)

encodec_libs += [ x264_codec_lib ]
encodec_deps += [ x264_codec_dep ]





#openh264_codec_src = ['openh264_dec.c']

#openh264_codec_lib = static_library('openh264-codec-lib', openh264_codec_src,
#                                   include_directories: [ include_directories('.'),
#                                                          include_directories('../../include')])

#openh264_codec_dep = declare_dependency(link_with: openh264_codec_lib)

#decodec_libs += [ openh264_codec_lib ]
#decodec_deps += [ openh264_codec_dep ]