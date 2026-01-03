# Linux platform-specific code

# Platform sources
platform_sources = files('platform_linux.c')

# Platform dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.50')
libevdev_dep = dependency('libevdev')
math_dep = meson.get_compiler('c').find_library('m', required: true)
platform_deps = [glib_dep, libevdev_dep, math_dep]

############ Config module with version info

config_conf = configuration_data()
config_conf.set('VERSION', meson.project_version())
config_conf.set('DAEMON_EXEC', exec_path / 'mousedamper')
config_conf.set('CONFIG_EXEC', bindir / 'mousedamper-config')
config_conf.set('LOCALE_DIR', locale_dir)

config_module = configure_file(
  input : 'config.py.in',
  output: 'config.py',
  configuration: config_conf,
)

install_data(config_module,
  install_dir: exec_path,
)

############ Manager (formerly launcher)

# Install the manager Python script
install_data(
  'mousedamper-launch.py',
  install_dir: exec_path,
  install_mode: ['rwxr-xr-x', 'root', 'root']
)

# Create and install thin wrapper
wrapper_conf = configuration_data()
wrapper_conf.set('manager_exec', exec_path / 'mousedamper-launch.py')

manager_wrapper = configure_file(
  input : 'mousedamper-launch.in',
  output: 'mousedamper-launch',
  configuration: wrapper_conf,
)

install_data(manager_wrapper,
  install_dir: get_option('bindir'),
  install_mode: ['rwxr-xr-x', 'root', 'root']
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
