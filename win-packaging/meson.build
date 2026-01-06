# Windows installer configuration
# Convert version to quad format for Windows (0.9.0 -> 0.9.0.0)
version_parts = meson.project_version().split('.')
while version_parts.length() < 4
    version_parts += ['0']
endwhile
version_quad = '.'.join(version_parts)

installer_conf = configuration_data()
installer_conf.set('VERSION', meson.project_version())
installer_conf.set('VERSION_QUAD', version_quad)

configure_file(
    input: 'installer.nsi.in',
    output: 'installer.nsi',
    configuration: installer_conf,
)
