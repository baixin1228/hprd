x264_codec_src = ['libx264_enc.c']
x264_codec_deps = [ dependency('x264')]

x264_codec_lib = static_library('ffmpeg-codec-lib', x264_codec_src,
                                   dependencies: x264_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

x264_codec_dep = declare_dependency(link_with: x264_codec_lib,
                                       dependencies: x264_codec_deps)

encodec_libs += [ x264_codec_lib ]
encodec_deps += [ x264_codec_dep ]