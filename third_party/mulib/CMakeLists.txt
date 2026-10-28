cmake_minimum_required(VERSION 3.16)
project(MuLIB VERSION 0.1.0 LANGUAGES C)

add_library(mulib INTERFACE)
target_include_directories(mulib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mulib INTERFACE c_std_11)

option(MULIB_BUILD_TESTS "Build MuLIB tests" ON)
option(MULIB_BUILD_MODULE_EXAMPLES "Build standalone example for each module" OFF)

if(MULIB_BUILD_TESTS)
  enable_testing()
  add_executable(mulib_smoke tests/smoke.c)
  target_link_libraries(mulib_smoke PRIVATE mulib m)
  add_test(NAME mulib_smoke COMMAND mulib_smoke)

  # In the functional executable, the implementation resides in ONE source TU.
  add_executable(mulib_functional tests/implementation.c tests/functional.c)
  target_link_libraries(mulib_functional PRIVATE mulib m)
  add_test(NAME mulib_functional COMMAND mulib_functional)

  add_executable(mulib_all_headers tests/all_headers.c)
  target_link_libraries(mulib_all_headers PRIVATE mulib m)
  add_test(NAME mulib_all_headers COMMAND mulib_all_headers)
endif()

if(MULIB_BUILD_MODULE_EXAMPLES)
  file(GLOB module_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/modules/*.c")
  foreach(source_file IN LISTS module_examples)
    get_filename_component(module_name "${source_file}" NAME_WE)
    add_executable("mulib_example_${module_name}" "${source_file}")
    target_link_libraries("mulib_example_${module_name}" PRIVATE mulib m)
    if(MULIB_BUILD_TESTS)
      add_test(NAME "module_${module_name}" COMMAND "mulib_example_${module_name}")
    endif()
  endforeach()
endif()
