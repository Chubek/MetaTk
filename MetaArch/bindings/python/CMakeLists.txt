cmake_minimum_required(VERSION 3.18)

# This directory can also be configured directly (including by scikit-build).
if(NOT TARGET metaarch)
  project(MetaArchPython LANGUAGES C CXX)
  set(ARCHDB_BUILD_PYTHON ON CACHE BOOL "" FORCE)
  set(ARCHDB_BUILD_DSL OFF CACHE BOOL "" FORCE)
  set(ARCHDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(ARCHDB_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  add_subdirectory(../.. metaarch-core)
  return()
endif()

enable_language(CXX)
find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
if(ARCHDB_NANOBIND_DIR)
  if(NOT EXISTS "${ARCHDB_NANOBIND_DIR}/cmake/nanobind-config.cmake")
    message(FATAL_ERROR "ARCHDB_NANOBIND_DIR must point to a nanobind checkout")
  endif()
  add_subdirectory("${ARCHDB_NANOBIND_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/nanobind" EXCLUDE_FROM_ALL)
  include("${ARCHDB_NANOBIND_DIR}/cmake/nanobind-config.cmake")
else()
  find_package(nanobind CONFIG QUIET)
  if(NOT nanobind_FOUND)
    execute_process(COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
      OUTPUT_VARIABLE _nanobind_cmake_dir OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    find_package(nanobind CONFIG REQUIRED HINTS "${_nanobind_cmake_dir}")
  endif()
endif()

set_target_properties(metaarch PROPERTIES POSITION_INDEPENDENT_CODE ON)
nanobind_add_module(_native NB_STATIC native.cpp)
target_link_libraries(_native PRIVATE MetaArch::api)
target_compile_features(_native PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(_native PRIVATE -Wall -Wextra -Wpedantic)
endif()
set(_python_package_dir "${CMAKE_CURRENT_BINARY_DIR}/metaarch")
set_target_properties(_native PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "$<1:${_python_package_dir}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${_python_package_dir}>")
file(MAKE_DIRECTORY "${_python_package_dir}")
foreach(_file __init__.py __init__.pyi py.typed)
  configure_file("metaarch/${_file}" "${_python_package_dir}/${_file}" COPYONLY)
endforeach()

if(SKBUILD)
  set(_python_install_dir metaarch)
else()
  set(ARCHDB_PYTHON_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages/metaarch"
    CACHE STRING "Python package installation directory (relative to prefix)")
  set(_python_install_dir "${ARCHDB_PYTHON_INSTALL_DIR}")
  # Shared builds install libmetaarch three levels above the default package.
  if(APPLE)
    set_property(TARGET _native PROPERTY INSTALL_RPATH "@loader_path/../../..")
  elseif(UNIX)
    set_property(TARGET _native PROPERTY INSTALL_RPATH "$ORIGIN/../../..")
  endif()
endif()
install(TARGETS _native LIBRARY DESTINATION "${_python_install_dir}" COMPONENT python)
install(FILES metaarch/__init__.py metaarch/__init__.pyi metaarch/py.typed
  DESTINATION "${_python_install_dir}" COMPONENT python)

if(ARCHDB_BUILD_TESTS AND ARCHDB_TEST_PYTHON_BINDINGS)
  add_test(NAME metaarch_python_api COMMAND "${CMAKE_COMMAND}" -E env
    "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "PYTHONDONTWRITEBYTECODE=1"
    "METAARCH_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "${Python_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -v)
endif()
