# Windows installer configuration
# Convert version to quad format for Windows (0.9.0 -> 0.9.0.0)
# Assumes version is always x.x.x format
version_quad = meson.project_version() + '.0'

installer_conf = configuration_data()
installer_conf.set('VERSION', meson.project_version())
installer_conf.set('VERSION_QUAD', version_quad)

configure_file(
    input: 'installer.nsi.in',
    output: 'installer.nsi',
    configuration: installer_conf,
)
