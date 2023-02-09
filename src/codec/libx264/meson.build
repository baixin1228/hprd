#x264_codec_src = ['libx264_enc.c']
#x264_codec_deps = [ dependency('x264')]

#x264_codec_lib = shared_library('x264-codec-lib', x264_codec_src,
#                                   dependencies: x264_codec_deps,
#                                   include_directories: ['.', '../../include'])

#x264_codec_dep = declare_dependency(link_with: x264_codec_lib,
#                                       dependencies: x264_codec_deps)

#encodec_libs += [ x264_codec_lib ]
#encodec_deps += [ x264_codec_dep ]

openh264_codec_lib = shared_library('openh264dec',
    ['openh264_dec.c'],
    dependencies: [common_dep, dependency('openh264'), dependency('glib-2.0')],
    include_directories: ['.', '../../include'])
