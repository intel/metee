# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020 Intel Corporation
project('metee', 'c',
  version : run_command('get-version.py').stdout().strip(),
  license : 'Apache 2.0',
  meson_version : '>=0.51.0',
  default_options : ['warning_level=2',
                     'default_library=static',
                     'c_std=gnu99',
                     'optimization=2'],
)

metee_sources_linux = [
  'src/linux/metee_linux.c',
  'src/linux/mei.c'
]

metee_sources_windows = [
  'src/Windows/metee_win.c',
  'src/Windows/metee_winhelpers.c'
]

warning_flags = [
  '-Wno-unused-command-line-argument',
  '-Wsign-compare',
  '-Wformat-security',
  '-Wstack-protector',
  '-Winit-self',
  '/WX',
  '/W4'
]

# secure compile flags
security_flags = [
  '-fstack-protector-strong',
  '-Wformat',
  '-Wformat-security',
  '-fno-strict-overflow',
  '-fno-delete-null-pointer-checks',
  '-fwrapv',
  '/GS',
  '/sdl'
]

if get_option('optimization').to_int() >= 2
  security_flags += [
    '-D_FORTIFY_SOURCE=2'
  ]
endif

debug_flags = []
if get_option('buildtype') == 'debug'
  debug_flags += [
    '-O0',
    '-g3',
    '-ggdb'
  ]
endif

cc = meson.get_compiler('c')
if cc.get_id() == 'msvc'
  add_project_arguments('-DUNICODE', '-D_UNICODE',
                        language : 'c')
  if get_option('mvsc_runtime_static')
    if get_option('buildtype') == 'debug'
      add_project_arguments('/MT', language : 'c')
    else
      add_project_arguments('/MTd', language : 'c')
    endif
  endif
endif

add_project_arguments(cc.get_supported_arguments(warning_flags),
                      language : 'c')
add_project_arguments(cc.get_supported_arguments(security_flags),
                      language : 'c')
add_project_arguments(cc.get_supported_arguments(debug_flags),
                      language : 'c')
add_project_arguments('-D_XOPEN_SOURCE=700', language : 'c')

global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  '-Wl,-z,noexecstack',
]
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(
  global_link_args,
  language: 'c'
)

if target_machine.system() == 'linux'
  metee_lib_static = static_library('metee',
     sources : metee_sources_linux,
     include_directories : ['include', 'src/linux']
)
elif target_machine.system() == 'windows'
  metee_lib_static = static_library('metee',
    sources : metee_sources_windows,
    include_directories : ['include', 'src/Windows'],
    link_args : ['CfgMgr32.lib']
)
endif

metee_dep_static = declare_dependency(
  link_with : metee_lib_static,
  include_directories : include_directories('include'),
)
