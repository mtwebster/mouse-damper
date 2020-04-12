libexec_path = join_paths(get_option('prefix'), get_option('libexecdir'), 'mouse-damper', 'mousedamper')
nuitka = find_program('nuitka3')

custom_target('binary',
  input: 'mousedamper.py',
  output: 'mousedamper',
  command: [
    'python3',
    join_paths(meson.current_source_dir(), 'compile_mousedamper.py'),
    join_paths(meson.current_source_dir(), 'mousedamper.py')
  ],
  build_by_default: true
)

install_subdir(
  join_paths(meson.current_build_dir(), 'mouse-damper'),
  install_dir: join_paths(get_option('libexecdir')),
)

meson.add_install_script('setuid_install_script.py', libexec_path)

############ Starter script

conf = configuration_data()
conf.set('exec', libexec_path)

starter = configure_file(
  input : 'mouse-damper-launch.in',
  output: 'mouse-damper-launch',
  configuration: conf,
)

install_data(starter,
  install_dir: get_option('bindir')
)

install_data(
  'mousedamper-config.py',
  install_dir: join_paths(get_option('libexecdir'), 'mousedamper'),
)

