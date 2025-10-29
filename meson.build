project('opentracecapture', ['c', 'cpp'],
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

# Force-include Windows compatibility header on Windows to ensure proper header order
if host_machine.system() == 'windows'
  add_project_arguments(
    '/FI' + join_paths(meson.source_root(), 'include', 'opentracecapture', 'otc_win_compat.h'),
    '/DWIN32_LEAN_AND_MEAN',
    '/DNOMINMAX',
    '/D_CRT_SECURE_NO_WARNINGS',
    '/D_WIN32_WINNT=0x0601',
    '/DWINVER=0x0601',
    language: ['c', 'cpp']
  )
endif

# Version handling - allow CI to override version from Git tags
otc_version_opt = get_option('version_override')
otc_version = otc_version_opt != '' ? otc_version_opt : meson.project_version()

# Split version into major/minor/micro
vparts = otc_version.split('.')
vmaj = (vparts.length() >= 1) ? vparts[0] : '0'
vmin = (vparts.length() >= 2) ? vparts[1] : '0'
vmic = (vparts.length() >= 3) ? vparts[2] : '0'

# Whether to append git describe suffix (dev builds)
embed_git_suffix = get_option('embed_git_suffix')

git_suffix = ''
if embed_git_suffix
  r = run_command('git', 'describe', '--tags', '--always', '--dirty', check: false)
  if r.returncode() == 0
    git_suffix = '-' + r.stdout().strip()
  endif
endif

# Generate git-version.h (installed header so apps see the same suffix if included)
git_version_h = configure_file(
  input: 'include/opentracecapture/git-version.h.in',
  output: 'git-version.h',
  configuration: { 'GIT_SUFFIX': git_suffix },
  install: true,
  install_dir: 'include/opentracecapture',
)

# Generate version.h from template
confv = configuration_data()
confv.set('OTC_PACKAGE_VERSION_MAJOR', vmaj)
confv.set('OTC_PACKAGE_VERSION_MINOR', vmin)
confv.set('OTC_PACKAGE_VERSION_MICRO', vmic)
confv.set('OTC_PACKAGE_VERSION_STRING_PREFIX', '@0@.@1@.@2@'.format(vmaj, vmin, vmic))

# Keep these in Meson (you bump on ABI changes)
confv.set('OTC_LIB_VERSION_CURRENT',  '4')
confv.set('OTC_LIB_VERSION_REVISION', '0')
confv.set('OTC_LIB_VERSION_AGE',      '0')

version_h = configure_file(
  input: 'include/opentracecapture/version.h.in',
  output: 'version.h',
  configuration: confv,
  install: true,
  install_dir: 'include/opentracecapture',
)

# Create symlinks in build directory for proper include paths
run_command('mkdir', '-p', meson.current_build_dir() / 'opentracecapture', check: true)
run_command('ln', '-sf', meson.current_build_dir() / 'version.h', 
            meson.current_build_dir() / 'opentracecapture' / 'version.h', check: false)
run_command('ln', '-sf', meson.current_build_dir() / 'git-version.h', 
            meson.current_build_dir() / 'opentracecapture' / 'git-version.h', check: false)

cc = meson.get_compiler('c')
fs = import('fs')

# Core dependencies (always required) - matching original libsigrok
deps_core = [
  dependency('glib-2.0', version: '>= 2.32.0'),
  dependency('libusb-1.0', version: '>= 1.0.16'),
  dependency('libzip', version: '>= 0.10'),  # MANDATORY in original libsigrok
  cc.find_library('m', required: false),
]

# Optional dependencies with proper detection (matching original)
dep_libftdi1 = dependency('libftdi1', version: '>= 1.0', required: false)
dep_libserialport = dependency('libserialport', version: '>= 0.1.1', required: false)

# HID API detection (matching original logic)
dep_hidapi = dependency('hidapi-hidraw', version: '>= 0.8.0', required: false)
if not dep_hidapi.found()
  dep_hidapi = dependency('hidapi-libusb', version: '>= 0.8.0', required: false)
endif
if not dep_hidapi.found()
  dep_hidapi = dependency('hidapi', version: '>= 0.8.0', required: false)
endif

# nettle (crypto) - optional
opt_nettle = get_option('with_nettle')
dep_nettle = dependency('nettle', required: opt_nettle.enabled())
have_nettle = dep_nettle.found() and cc.has_header('nettle/aes.h', dependencies: dep_nettle)

# ieee1284 (parallel port) - optional, Linux only
opt_ieee1284 = get_option('with_ieee1284')
dep_ieee1284 = dependency('ieee1284', required: false, method: 'pkg-config')
if not dep_ieee1284.found()
  dep_ieee1284 = cc.find_library('ieee1284', required: opt_ieee1284.enabled())
endif
have_ieee1284 = dep_ieee1284.found() and cc.has_header('ieee1284.h', dependencies: dep_ieee1284)

# gpib - optional, Linux only
opt_gpib = get_option('with_gpib')
dep_gpib = cc.find_library('gpib', required: opt_gpib.enabled())
have_gpib = dep_gpib.found() and cc.has_header('gpib/ib.h', dependencies: dep_gpib)

# Configuration and driver tracking
conf = configuration_data()

# Enable drivers
conf.set('HAVE_DRIVERS', 1)

# Basic configuration
conf.set_quoted('PACKAGE_NAME', 'opentracecapture')
conf.set_quoted('CONF_HOST', host_machine.system() + '-' + host_machine.cpu_family())
conf.set_quoted('CONF_LIBZIP_VERSION', deps_core[2].version())  # libzip is always available

# C++ standard library feature detection
cpp = meson.get_compiler('cpp')
if cpp.compiles('#include <string>\nint main() { std::stoi("1"); return 0; }', name: 'stoi function')
  conf.set('HAVE_STOI_STOD', 1)
endif
if cpp.compiles('#include <string>\nint main() { std::stoul("1"); return 0; }', name: 'stoul function')
  conf.set('HAVE_STOUL', 1)
endif

# Feature flags (matching original libsigrok logic)
conf.set('HAVE_LIBUSB_1_0', true)  # Always true since it's mandatory
conf.set('HAVE_LIBFTDI1', dep_libftdi1.found())
conf.set('HAVE_LIBSERIALPORT', dep_libserialport.found())
conf.set('HAVE_LIBZIP', true)  # Always true since it's mandatory
conf.set('HAVE_HIDAPI', dep_hidapi.found())
conf.set('HAVE_NETTLE', have_nettle)
conf.set('HAVE_IEEE1284', have_ieee1284)
conf.set('HAVE_GPIB', have_gpib)

# Serial communication support (matches original logic)
have_serial_comm = dep_libserialport.found() or dep_hidapi.found()
conf.set('HAVE_SERIAL_COMM', have_serial_comm)

enabled_drivers = []
disabled_drivers = []

# Include directories
inc = include_directories('include', '.')

# Build final dependency list (core deps are always included)
all_deps = deps_core
compile_args = ['-DOPENTRACECAPTURE_BUILD']

# Windows-specific dependencies
if host_machine.system() == 'windows'
  all_deps += cc.find_library('ws2_32', required: true)
endif

# Add optional dependencies when available
if dep_hidapi.found()
  all_deps += dep_hidapi
endif
if dep_libftdi1.found()
  all_deps += dep_libftdi1
endif
if dep_libserialport.found()
  all_deps += dep_libserialport
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

# Add network support for TCP (not needed on MSVC)
if cc.get_id() != 'msvc'
  compile_args += '-D_GNU_SOURCE'
endif

# Include subdirectories
subdir('src/core')
subdir('src/formats')
subdir('src/drivers')
subdir('src/input')
subdir('src/output')
subdir('src/transform')
subdir('src/tp')

# Create separate driver libraries like libsigrok does
driver_head_lib = static_library('drivers_head',
  sources: ['src/driver_list_start.c'],
  include_directories: inc,
  dependencies: all_deps,
  c_args: compile_args)

driver_tail_lib = static_library('drivers_tail',
  sources: ['src/driver_list_stop.c'],
  include_directories: inc,
  dependencies: all_deps,
  c_args: compile_args)

driver_main_lib = static_library('drivers_main',
  sources: driver_sources,
  include_directories: inc,
  dependencies: all_deps,
  c_args: compile_args)

# Link in correct order: head -> main -> tail (like libsigrok)
lib = library('opentracecapture',
  sources: [core_sources, format_sources, input_sources, output_sources, transform_sources, tp_sources],
  include_directories: inc,
  dependencies: all_deps,
  c_args: compile_args,
  link_whole: [driver_head_lib, driver_main_lib, driver_tail_lib],
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

# Install udev rules (Linux only)
if host_machine.system() == 'linux'
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
endif

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
  'libzip': true,  # Always available since it's mandatory
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

# C++ bindings
if get_option('bindings_cxx')
  subdir('bindings/cxx')
endif
