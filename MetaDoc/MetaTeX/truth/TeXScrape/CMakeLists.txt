# Self-contained TeX82/TeXScrape engine. Does not depend on MetaTeX's optional plugins.
cmake_minimum_required(VERSION 3.18)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(TeXScrape VERSION 0.2.0 LANGUAGES C CXX)
  include(CTest)
endif()
option(TEXSCRAPE_BUILD_TESTS "Test TeX82 format and DVI output" ON)
add_executable(texscrape Working/TeX.c)
set_target_properties(texscrape PROPERTIES C_STANDARD 11 C_EXTENSIONS ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(texscrape PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(texscrape PRIVATE -O2 -Wall -Wextra -Wno-unused-function
   -Wno-unused-label -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare)
endif()
# Independent DVI verifier usable without any MetaTeX runtime dependencies.
add_library(texscrape_dvi STATIC "${CMAKE_CURRENT_SOURCE_DIR}/../../device/DVI.cpp")
target_compile_features(texscrape_dvi PUBLIC cxx_std_20)
add_executable(texscrape-dvi tools/dviinspect.cpp)
target_link_libraries(texscrape-dvi PRIVATE texscrape_dvi)
set_target_properties(texscrape-dvi PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS texscrape-dvi RUNTIME DESTINATION bin)
# Binary readers/sinks are reusable MetaTeX subsystems, tested without plugins.
add_library(metatex_streams STATIC
  ../../reader/File.cpp ../../reader/STDIN.cpp ../../reader/Pipe.cpp
  ../../reader/MemoryMap.cpp ../../sink/IOSinks.cpp)
target_compile_features(metatex_streams PUBLIC cxx_std_20)
target_include_directories(metatex_streams PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

install(TARGETS texscrape RUNTIME DESTINATION bin)
install(PROGRAMS PlainTeX/Bootstrapped.sh DESTINATION share/texscrape)
install(PROGRAMS ../../bin/mtex82 DESTINATION bin)
install(FILES PlainTeX/PlainTeX.tex DESTINATION share/texscrape)
if(BUILD_TESTING AND TEXSCRAPE_BUILD_TESTS)
  add_executable(texscrape-stream-tests tests/test_streams.cpp)
  target_link_libraries(texscrape-stream-tests PRIVATE metatex_streams)
  add_test(NAME metatex-stream-io COMMAND texscrape-stream-tests "${CMAKE_CURRENT_BINARY_DIR}/stream-fixture.bin")
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME texscrape-dvi-regressions COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_engine.py"
      "$<TARGET_FILE:texscrape>" "${CMAKE_CURRENT_SOURCE_DIR}"
      "$<TARGET_FILE:texscrape-dvi>")
    set_tests_properties(texscrape-dvi-regressions PROPERTIES TIMEOUT 100)
  endif()
endif()
