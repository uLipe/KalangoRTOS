project('nrf52_blink', 'c')

compiler_obj = meson.get_compiler('c')
size = find_program('arm-none-eabi-size')
objcopy = find_program('arm-none-eabi-objcopy')

cputype = 'cortex-m4'
cpudefine = 'nrf52'

kalango_path = '../../..'
kernel_sources = []
arch_sources = []
target_includes = []
log_sources = []
lib_sources = []

target_includes += [
   include_directories(kalango_path + '/include'),
   include_directories(kalango_path + '/ext/cmsis'),
   include_directories(kalango_path + '/ext/nrfx'),
   include_directories(kalango_path + '/ext/printf'), 
   include_directories(kalango_path + '/ext/segger'),
   include_directories(kalango_path + '/ext/segger/rtt'),
   include_directories(kalango_path + '/ext/segger/systemview'),
   include_directories(kalango_path + '/lib'),
   include_directories(kalango_path + '/examples/kernel_examples'),
]

log_sources += files(
   kalango_path + '/ext/segger/rtt/SEGGER_RTT.c',
   kalango_path + '/ext/segger/rtt/SEGGER_RTT.c',
   kalango_path + '/ext/segger/systemview/SEGGER_SYSVIEW.c',
   kalango_path + '/ext/segger/systemview/SEGGER_SYSVIEW.c',
   kalango_path + '/ext/printf/printf.c',
)

lib_sources += files(
   kalango_path + '/lib/print_out.c',
   kalango_path + '/lib/tlsf.c',
)

kernel_sources += files(
   kalango_path + '/src/kalango_sources.c',
)

arch_sources += files(
   kalango_path + '/src/arch/arm_v7m/arch_armv7m.c',
   kalango_path + '/src/arch/arm_v7m/context_switch_armv7m.S',
)

kalango_srcs = kernel_sources + arch_sources + lib_sources + log_sources

nrf52_sources =  []
app_sources =  []

target_includes += [
   include_directories('../mdk'),
   include_directories('config'),
]

nrf52_sources += files(
   '../common_src/gcc_startup_nrf52833.S',
   '../common_src/system_nrf52833.c',
)

app_sources += files(
   'main.c',
)

srcs = kalango_srcs + app_sources + nrf52_sources

cargs =  [
   '-includekalango_config.h',
   '-DNRF52',
   '-DNRF52832_XXAA',
   '-mcpu=cortex-m4',
   '-mthumb',
   '-mabi=aapcs',
   '-Wall', 
   '-Werror',
   '-O0', 
   '-g3',
   '-mfloat-abi=hard',
   '-mfpu=fpv4-sp-d16',
   '-ffunction-sections',
   '-fdata-sections',
   '-fno-strict-aliasing',
   '-fno-builtin',
   '--short-enums',
]

linkargs =  [
   '-mthumb',
   '-mabi=aapcs',
   '-T' + meson.current_source_dir() + '/../ldscripts/nrf52832_xxaa.ld',
   '-mcpu=cortex-m4',
   '-mfloat-abi=hard',
   '-mfpu=fpv4-sp-d16',
   '-Wl,--gc-sections',
   '--specs=nano.specs',
   '-lc',
   '-lnosys',
   '-lm',
]

artifacts = ['blink', srcs, cargs, linkargs]

exe = executable(artifacts[0], artifacts[1],
                 c_args: artifacts[2],
                 link_args: artifacts[3],
                 include_directories : target_includes,
                 build_by_default: true)

run_target('hex', command: [objcopy, ['-Obinary', exe.full_path(),
           exe.full_path() + '.hex']], depends: exe)
run_target('size', command: [size, exe.full_path(), '-B'], depends: exe)

if meson.is_cross_build()
   message('cross compiling for ' + cputype)
   message('''
              ninja           - generates elf file.
              ninja hex       - generates hex file.
              ninja size      - gives the summary of hex file size.
              (C) uLipe 2019''')
endif

