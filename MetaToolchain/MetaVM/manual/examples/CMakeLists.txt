cmake_minimum_required(VERSION 3.18)
project(MetaVMManualExamples LANGUAGES C CXX)
enable_testing()
find_package(MetaVM 0.2 CONFIG REQUIRED COMPONENTS Framework)
set(METAVM_MANUAL_VMWEAVE "" CACHE FILEPATH "Optional installed VMWeave executable for the C factory example")

function(manual_example name source dependency)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE ${dependency})
  set_target_properties(${name} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

manual_example(manual_embed_c embed.c MetaVM::Machinery)
manual_example(manual_embed_cpp embed.cpp MetaVM::Machinery)
manual_example(manual_host host.c MetaVM::Machinery)
manual_example(manual_authoring authoring.cpp MetaVM::Framework)
manual_example(manual_native native.cpp MetaVM::Framework)
set_tests_properties(manual_embed_c manual_embed_cpp manual_host PROPERTIES PASS_REGULAR_EXPRESSION "42")
set_tests_properties(manual_authoring PROPERTIES PASS_REGULAR_EXPRESSION "builder=42 definition=42 memory\\[3\\]=42")

if(METAVM_MANUAL_VMWEAVE)
  if(NOT EXISTS "${METAVM_MANUAL_VMWEAVE}")
    message(FATAL_ERROR "METAVM_MANUAL_VMWEAVE must name a built VMWeave executable")
  endif()
  set(factory "${CMAKE_CURRENT_BINARY_DIR}/answer_factory.c")
  add_custom_command(OUTPUT "${factory}"
    COMMAND "${METAVM_MANUAL_VMWEAVE}" emit-c "${CMAKE_CURRENT_SOURCE_DIR}/answer.mvm"
      "${factory}" application_answer
    DEPENDS "${METAVM_MANUAL_VMWEAVE}" answer.mvm VERBATIM)
  manual_example(manual_factory factory_host.c MetaVM::Machinery)
  target_sources(manual_factory PRIVATE "${factory}")
  set_tests_properties(manual_factory PROPERTIES PASS_REGULAR_EXPRESSION "42")
endif()
