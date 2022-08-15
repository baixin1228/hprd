libva_display_src = ['va_display.c']
libva_display_deps = [ dependency('libva', version : '>=0.36.0')]

  libva_display_src += [ 'va_display_x11.c' ]
  libva_display_deps += [
    dependency('x11', required: true),
    dependency('libva-x11', required: true),
  ]

  libva_display_src += [ 'vaapi_hevc_encodec.c' ]

#if use_drm
#  libva_display_src += [ 'va_display_drm.c' ]
#  libva_display_deps += drm_deps
#endif

#if use_wayland
#  libva_display_src += [ 'va_display_wayland.c' ]
#  libva_display_deps += wayland_deps
#endif

libva_display_lib = static_library('common', libva_display_src,
                                   dependencies: libva_display_deps,
                                   include_directories: [include_directories('.'), include_directories('../../include')])
libva_display_dep = declare_dependency(link_with: libva_display_lib,
                                       dependencies: libva_display_deps)

encodec_libs = [ libva_display_lib ]
encodec_deps = [ libva_display_dep ]