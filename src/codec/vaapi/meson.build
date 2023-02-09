libva_codec_src = ['va_display.c']
libva_codec_deps = [ dependency('libva', version : '>=0.36.0')]

libva_codec_src += [ 'vaapi_hevc_encodec.c' ]

#  libva_codec_src += [ 'va_display_x11.c' ]
#  libva_codec_deps += [
#    dependency('x11', required: true),
#    dependency('libva-x11', required: true),
#  ]


  libva_codec_src += [ 'va_display_drm.c' ]
  libva_codec_deps += [
    dependency('libva-drm', required: true),
    dependency('libdrm', required: true),
  ]

#if use_wayland
#  libva_codec_src += [ 'va_display_wayland.c' ]
#  libva_codec_deps += wayland_deps
#endif

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)
libva_codec_deps += m_dep

libva_codec_lib = static_library('libva-codec-lib', libva_codec_src,
                                   dependencies: libva_codec_deps,
                                   include_directories: [include_directories('.'), include_directories('../../include')],
                                   link_args: '-lm')
libva_codec_dep = declare_dependency(link_with: libva_codec_lib,
                                       dependencies: libva_codec_deps)

encodec_libs = [ libva_codec_lib ]
encodec_deps = [ libva_codec_dep ]