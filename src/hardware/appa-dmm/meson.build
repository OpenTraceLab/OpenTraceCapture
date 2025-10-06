appa_dmm_sources = files([
  'api.c',
  'protocol.c',
])

appa_dmm_lib = static_library('appa-dmm',
  appa_dmm_sources,
  include_directories: [inc, include_directories('../..')],
  c_args: compile_args,
)
