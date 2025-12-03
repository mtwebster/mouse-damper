glib_dep = dependency('glib-2.0', version: '>= 2.50')
libevdev_dep = dependency('libevdev')
math_dep = meson.get_compiler('c').find_library('m', required: true)

mousedamper_sources = ['mousedamper.c']

mousedamper_deps = [
  glib_dep,
  libevdev_dep,
  math_dep,
]

mousedamper_exe = executable('mousedamper',
  mousedamper_sources,
  dependencies: mousedamper_deps,
  install: true,
  install_dir: exec_path,
  install_mode: ['rwsr-xr-x', 'root', 'root']
)

############ Starter script

main_conf = configuration_data()
main_conf.set('exec', exec_path / 'mousedamper')

main_starter = configure_file(
  input : 'mousedamper-launch.in',
  output: 'mousedamper-launch',
  configuration: main_conf,
)

install_data(main_starter,
  install_dir: get_option('bindir')
)

########### Config launcher

config_conf = configuration_data()
config_conf.set('exec', exec_path / 'mousedamper-config.py')

config_starter = configure_file(
  input : 'mousedamper-config.in',
  output: 'mousedamper-config',
  configuration: config_conf,
)

install_data(config_starter,
  install_dir: get_option('bindir'),
  install_mode: ['rwxr-xr-x', 'root', 'root']
)

install_data(
  'mousedamper-config.py',
  install_dir: exec_path,
  install_mode: ['rwxr-xr-x', 'root', 'root']
)
