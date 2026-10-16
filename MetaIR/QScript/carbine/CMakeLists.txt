cmake_minimum_required(VERSION 3.16)
project(carbine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

# Core tooling: every file in tooling/ except the host entry point, the spec
# loader (both need QScript) and the generic preprocessor.
file(GLOB CARBINE_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tooling/*.c)
list(REMOVE_ITEM CARBINE_CORE_SOURCES
  ${CMAKE_SOURCE_DIR}/tooling/gpp.c
  ${CMAKE_SOURCE_DIR}/tooling/carbine_main.c
  ${CMAKE_SOURCE_DIR}/tooling/spec.c)

add_library(carbine_core STATIC ${CARBINE_CORE_SOURCES})
target_include_directories(carbine_core PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tooling)
target_link_libraries(carbine_core PUBLIC m)

add_executable(gpp tooling/gpp.c)
target_compile_options(gpp PRIVATE -std=gnu11)
target_link_libraries(gpp PRIVATE m)

# The carbine host runs the QScript engine, so it needs a QScript tree:
#   cmake -DCARBINE_WITH_QSCRIPT=ON -DQSCRIPT_ROOT=/path/to/qscript ..
option(CARBINE_WITH_QSCRIPT "Build the carbine host (requires QScript)" OFF)
set(QSCRIPT_ROOT "" CACHE PATH "QScript source tree: provides include/ and a built qscript library")

if(CARBINE_WITH_QSCRIPT)
  if(NOT QSCRIPT_ROOT)
    message(FATAL_ERROR "CARBINE_WITH_QSCRIPT needs QSCRIPT_ROOT")
  endif()
  find_library(QSCRIPT_LIB qscript
    HINTS ${QSCRIPT_ROOT}/build ${QSCRIPT_ROOT}/lib ${QSCRIPT_ROOT}/build/lib REQUIRED)
  add_executable(carbine tooling/carbine_main.c tooling/spec.c)
  target_compile_definitions(carbine PRIVATE CARBINE_WITH_QSCRIPT)
  target_include_directories(carbine PRIVATE ${QSCRIPT_ROOT}/include)
  target_link_libraries(carbine PRIVATE carbine_core ${QSCRIPT_LIB} m)
endif()

enable_testing()
add_test(NAME engine COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/engine_test.py)

add_executable(unit_test tests/unit_test.c)
target_include_directories(unit_test PRIVATE ${CMAKE_SOURCE_DIR}/tooling ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(unit_test PRIVATE carbine_core)
add_test(NAME unit COMMAND unit_test)
