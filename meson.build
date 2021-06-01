project(
  'costream',
  ['c', 'cpp'],
  version : run_command(
    [find_program('python', 'python3'), 'bin/meson_get_version.py']
  ).stdout(),
  license : 'MIT',
  meson_version : '>= 0.52',
  default_options : ['buildtype=debug'] #, 'b_ndebug=if-release', 'c_std=c99', 'cpp_std=c++17']
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
buildtype = get_option('buildtype')
builddir = meson.current_build_dir()

null_dep = dependency('', required : false)

# Arguments for the preprocessor, put these in a separate array from the C and
# C++ (cpp in meson terminology) arguments since they need to be added to the
# default arguments for both C and C++.
pre_args = [
  '-D__STDC_CONSTANT_MACROS',
  '-D__STDC_FORMAT_MACROS',
  '-D__STDC_LIMIT_MACROS',
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DPACKAGE_BUGREPORT="https://gitlab.freedesktop.org/mesa/mesa/-/issues"',
]

extra_cpp_args = ['-Wall', '-std=c++17']

if buildtype == 'release'
  extra_cpp_args += ['-O3', '-g', '-Wno-strict-aliasing', '-msse4.1', '-flto']
endif

extra_c_args = extra_cpp_args

#if cpp.get_id() == 'gcc'
#  extra_cpp_args += ['-std=c++17']
#else
#  extra_cpp_args += []
#endif

link_args = ['-ldl', '-lpthread', '-L' + meson.current_build_dir(), '-lstdc++' ]

model_symbol_list = meson.current_source_dir() + '/model_symbol_export.def'

#pymodule = import('python3')
#python3 = pymodule.find_python()

COSTREAM_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["COSTREAM_HOME"])']).stdout().strip()
CODRIVE_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["CODRIVE_HOME"])']).stdout().strip()
COASM_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["COASM_HOME"])']).stdout().strip()
TVM_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["TVM_HOME"])']).stdout().strip()
LIBGO_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["LIBGO_HOME"])']).stdout().strip()
COMODEL_HOME = run_command([find_program('python', 'python3'), '-c', 'import os; print(os.environ["COMODEL_HOME"])']).stdout().strip()

message('COSTREAM_HOME is ' + COSTREAM_HOME)
message('CODRIVE_HOME is ' + CODRIVE_HOME)
message('COASM_HOME is ' + COASM_HOME)
message('TVM_HOME is ' + TVM_HOME)
message('CMODEL_HOME is ' + COMODEL_HOME)

#costream_inc = [COSTREAM_HOME + '/pps/platform',
#  COSTREAM_HOME + '/pps/device',
#  COSTREAM_HOME + '/cmdio',
#  CODRIVER_HOME + '/common'
#  ]


costream_inc = ['.', 'include', 'src']
  #CODRIVER_HOME + '/common'
costream_inc += COMODEL_HOME + '/src/inc'
costream_inc += COMODEL_HOME + '/src/behavior/gpu/inc'
costream_inc += CODRIVE_HOME + '/include'

message(costream_inc)

costream_src = [
  'src/common/small_heap.cpp',
  'src/common/OsSupport.cpp',
  'src/core/ISignal.cpp',
  'src/core/IQueue.cpp',
  'src/core/Runtime.cpp',
  'src/core/DefaultSignal.cpp',
  'src/core/util/timer.cpp',
  'src/core/util/lnx/os_linux.cpp',
  'src/core/InterruptSignal.cpp',
  'src/core/HardQueue.cpp',
  'src/core/SoftQueue.cpp',
  'src/core/EventPool.cpp',
  'src/core/Shared.cpp',
  #'src/core/StreamApi.cpp',
  'src/core/Stream.cpp',
  #'src/scheduler/StreamProcessor.cpp',
  #'src/scheduler/Pipe.cpp',
  'src/scheduler/Scheduler.cpp',
  'src/task/Task.cpp',
  'src/processor/Processor.cpp',
  'src/processor/fcontext.cpp',
  'src/processor/CoLocalStorage.cpp',
  'src/processor/jump_x86_64_sysv_elf_gas.S',
  'src/processor/make_x86_64_sysv_elf_gas.S',
  ]

#costream_link_args = link_args + ['-Wl,--version-script='+costream_symbol_list]
costream_link_args = link_args + ['-lpthread']

costream = shared_library(
  'costream',
  costream_src,
  include_directories : include_directories(costream_inc),
  install : false,
  cpp_args : extra_cpp_args,
  link_args : costream_link_args,
  )


test_src = [
  'tests/test/simple.cpp'
  ]

test_src += costream_src

test_inc = costream_inc

cotest = executable(
  'cotest',
  test_src,
  include_directories : test_inc,
  install :false,
  link_args : costream_link_args,
  #dependencies: declare_dependency(link_with : costream)
  )
