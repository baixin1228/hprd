project('hprd', 'c', default_options: 'werror=true')

add_global_arguments('-Dwerror=true', language : 'c')

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'c')
endif

subdir('src2')