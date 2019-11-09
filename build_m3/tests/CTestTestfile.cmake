# CMake generated Testfile for 
# Source directory: /home/ulipe/fun/KalangoRTOS/tests
# Build directory: /home/ulipe/fun/KalangoRTOS/build_m3/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[kalango_test_cortexm3]=] "/home/ulipe/fun/KalangoRTOS/scripts/run_tests.py" "--elf" "/home/ulipe/fun/KalangoRTOS/build_m3/tests/kalango_test_cortexm3.elf")
set_tests_properties([=[kalango_test_cortexm3]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ulipe/fun/KalangoRTOS/tests/CMakeLists.txt;68;add_test;/home/ulipe/fun/KalangoRTOS/tests/CMakeLists.txt;0;")
add_test([=[kalango_test_cortexm4f]=] "/home/ulipe/fun/KalangoRTOS/scripts/run_tests.py" "--elf" "/home/ulipe/fun/KalangoRTOS/build_m3/tests/kalango_test_cortexm4f.elf")
set_tests_properties([=[kalango_test_cortexm4f]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ulipe/fun/KalangoRTOS/tests/CMakeLists.txt;68;add_test;/home/ulipe/fun/KalangoRTOS/tests/CMakeLists.txt;0;")
