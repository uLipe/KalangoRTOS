kalango_path = join_paths(meson.source_root(), meson.current_source_dir())
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
   include_directories(kalango_path + '/ext/segger/rtt'),
   include_directories(kalango_path + '/ext/segger/systemview'),
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
)

kernel_sources += files(
   kalango_path + '/src/kalango_sources.c',
)

arch_sources += files(
   kalango_path + '/src/arch/arm_v7m/arch_armv7m.c',
   kalango_path + '/src/arch/arm_v7m/context_switch_armv7m.S',
)

kalango_srcs = kernel_sources + arch_sources + lib_sources + log_sources