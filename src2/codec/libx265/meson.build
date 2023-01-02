x265_codec_src = ['libx265_enc.c']
x265_codec_deps = [ dependency('x265')]

x265_codec_lib = static_library('ffmpeg-codec-lib', x265_codec_src,
                                   dependencies: x265_codec_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

x265_codec_dep = declare_dependency(link_with: x265_codec_lib,
                                       dependencies: x265_codec_deps)

encodec_libs += [ x265_codec_lib ]
encodec_deps += [ x265_codec_dep ]