sw_scale_src = ['libswscale.c']
sw_scale_deps = [ dependency('libswscale'), dependency('libavcodec')]

sw_scale_lib = static_library('ffmpeg-codec-lib', sw_scale_src,
                                   dependencies: sw_scale_deps,
                                   include_directories: [ include_directories('.'),
                                                          include_directories('../../include')])

sw_scale_dep = declare_dependency(link_with: sw_scale_lib,
                                       dependencies: sw_scale_deps)

frame_conv_libs += [ sw_scale_lib ]
frame_conv_deps += [ sw_scale_dep ]