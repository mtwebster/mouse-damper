project('mouse-damper',
  'c',
  version : '0.9.0'
)

# i18n configuration
i18n = import('i18n')
gettext_domain = 'mousedamper'

exec_path = join_paths(get_option('prefix'), get_option('libexecdir'), 'mousedamper')
bindir = join_paths(get_option('prefix'), get_option('bindir'))
sys_data_dir = join_paths(get_option('prefix'), get_option('datadir'))
locale_dir = join_paths(get_option('prefix'), get_option('localedir'))

subdir('po')
subdir('data')
subdir('src')
