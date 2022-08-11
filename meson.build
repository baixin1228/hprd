project('hprd', 'c')

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'c')
endif

subdir('test')

subdir('src/frame_buffer_input')

subdir('src/server')