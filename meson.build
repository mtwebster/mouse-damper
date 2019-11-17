project('mouse-damper',
  'c',
  version : '0.1.0'
)

gnome = import('gnome')
pkg = import('pkgconfig')
i18n = import('i18n')

cdata = configuration_data()
cdata.set('GETTEXT_PACKAGE', '"mouse-damper"')


##############  Compile python then c for main binary

deps = []
deps += dependency('python3', required: true)
cython = find_program('cython3')

generated_sources = custom_target('mousedamper.c',
  input: 'mousedamper.py',
  output: 'mousedamper.c',
  command: [cython, '@INPUT@', '--embed', '--output-file', meson.current_build_dir()],
  install: false
)

main_binary = executable('mouse-damper',
  generated_sources,
  dependencies: deps,
  install: true,
  install_dir: join_paths(get_option('libexecdir'), 'mouse-damper')
)

############ Starter script

exec_path = join_paths(get_option('prefix'), get_option('libexecdir'), 'mouse-damper', 'mouse-damper')

conf = configuration_data()
conf.set('exec', exec_path)

starter = configure_file(
  input : 'mouse-damper-launch.in',
  output: 'mouse-damper-launch',
  configuration: conf,
)

install_data(starter,
  install_dir: get_option('bindir')
)

meson.add_install_script('setuid_install_script.py', exec_path)

############# Desktop file

conf = configuration_data()
conf.set('exec', join_paths(get_option('prefix'), get_option('bindir'), 'mouse-damper-launch'))

desktop_file = configure_file(
  input : 'mouse-damper.desktop.in.in',
  output: 'mouse-damper.desktop.in',
  configuration: conf,
)

i18n.merge_file(
  input: desktop_file,
  output: 'mouse-damper.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart')
)
