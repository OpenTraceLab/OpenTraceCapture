project('opentracecapture', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

cc = meson.get_compiler('c')
fs = import('fs')

# Core dependencies (always required)
deps_core = [
  dependency('glib-2.0', version: '>= 2.32.0'),
  dependency('libusb-1.0', version: '>= 1.0.16'),
  cc.find_library('m', required: false),
]

# Optional dependencies with proper detection
dep_libftdi1 = dependency('libftdi1', required: false)
dep_libserialport = dependency('libserialport', required: false)
dep_libzip = dependency('libzip', required: false)

# HID API detection
opt_hidapi = get_option('hidapi')
dep_hidapi = dependency('hidapi-hidraw', required: false)
if not dep_hidapi.found()
  dep_hidapi = dependency('hidapi-libusb', required: false)
endif
if not dep_hidapi.found()
  dep_hidapi = dependency('hidapi', required: opt_hidapi.enabled())
endif

# --- Tricky dependencies ---
# nettle (crypto)
opt_nettle = get_option('with_nettle')
dep_nettle = dependency('nettle', required: opt_nettle.enabled(), method: 'auto')
have_nettle = dep_nettle.found() and cc.has_header('nettle/aes.h', dependencies: dep_nettle)

# ieee1284 (parallel port)
opt_1284 = get_option('with_ieee1284')
dep_ieee1284 = dependency('ieee1284', required: false, method: 'pkg-config')
if not dep_ieee1284.found()
  dep_ieee1284 = cc.find_library('ieee1284', required: opt_1284.enabled())
endif
have_ieee1284 = dep_ieee1284.found() and cc.has_header('ieee1284.h', dependencies: dep_ieee1284)

# gpib
opt_gpib = get_option('with_gpib')
dep_gpib = cc.find_library('gpib', required: opt_gpib.enabled())
have_gpib = dep_gpib.found() and cc.has_header('gpib/ib.h', dependencies: dep_gpib)

# Configuration and driver tracking
conf = configuration_data()
enabled_drivers = []
disabled_drivers = []

# Include directories
inc = include_directories('include')

# Build final dependency list
all_deps = deps_core
compile_args = []
if dep_hidapi.found()
  all_deps += dep_hidapi
endif
if dep_libftdi1.found()
  all_deps += dep_libftdi1
endif
if dep_libserialport.found()
  all_deps += dep_libserialport
  compile_args += '-DHAVE_SERIAL_COMM'
endif
if dep_libzip.found()
  all_deps += dep_libzip
endif
if have_nettle
  all_deps += dep_nettle
endif
if have_ieee1284
  all_deps += dep_ieee1284
endif
if have_gpib
  all_deps += dep_gpib
endif

# Add network support for TCP
compile_args += '-D_GNU_SOURCE'

# Include subdirectories
subdir('src/core')
subdir('src/formats')
subdir('src/drivers')

lib = library('opentracecapture',
  sources: [core_sources, driver_sources, format_sources],
  include_directories: inc,
  dependencies: all_deps,
  c_args: compile_args,
  install: true,
  version: meson.project_version(),
  soversion: '0')

# Install public headers
install_subdir('include/opentracecapture', install_dir: get_option('includedir'))

# Generate and install opentracecapture.pc
pkg = import('pkgconfig')
pkg.generate(
  name: 'opentracecapture',
  description: 'OpenTraceCapture core library',
  version: meson.project_version(),
  libraries: lib,
  subdirs: 'opentracecapture',
  requires: ['glib-2.0', 'libusb-1.0']
)

# Install udev rules
udev_dep = dependency('udev', required: false)
if udev_dep.found()
  _udevbase = udev_dep.get_pkgconfig_variable('udevdir')
  if _udevbase == ''
    _udevbase = '/usr/lib/udev'
  endif
else
  _udevbase = '/usr/lib/udev'
endif

udev_base = get_option('udevdir') != '' ? get_option('udevdir') : _udevbase
udev_rulesdir = join_paths(udev_base, 'rules.d')

install_data('contrib/60-opentracecapture.rules', install_dir: udev_rulesdir)

# Test executable
test_exe = executable('otc-smoke',
  sources: ['tests/smoke.c'],
  dependencies: all_deps,
  link_with: lib,
  include_directories: inc)

test('smoke', test_exe)

# Generate config header
configure_file(
  output: 'config.h',
  configuration: conf
)

# Pretty summary
summary({
  'nettle (crypto)': have_nettle,
  'ieee1284 (parallel)': have_ieee1284,
  'gpib': have_gpib,
  'hidapi': dep_hidapi.found(),
  'libserialport': dep_libserialport.found(),
  'libftdi1': dep_libftdi1.found(),
  'libzip': dep_libzip.found(),
}, section: 'Dependencies', bool_yn: true)

summary({
  'drivers enabled': enabled_drivers.length(),
  'drivers disabled': disabled_drivers.length(),
}, section: 'Drivers')

if enabled_drivers.length() > 0
  summary({'enabled': '\n  - ' + '\n  - '.join(enabled_drivers)}, section: 'Drivers')
endif
if disabled_drivers.length() > 0
  summary({'disabled': '\n  - ' + '\n  - '.join(disabled_drivers)}, section: 'Drivers')
endif
