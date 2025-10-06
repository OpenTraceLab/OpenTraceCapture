uss_scale_sources = files([
  'api.c',
  'protocol.c',
])

uss_scale_lib = static_library('uss-scale',
  uss_scale_sources,
  include_directories: [inc, include_directories('../..')],
  c_args: compile_args,
)
