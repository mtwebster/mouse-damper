project('mouse-damper',
  'c',
  version : '0.1.0'
)

exec_path = join_paths(get_option('prefix'), get_option('libexecdir'), 'mousedamper')
bindir = join_paths(get_option('prefix'), get_option('bindir'))
sys_data_dir = join_paths(get_option('prefix'), get_option('datadir'))

subdir('data')
subdir('src')
