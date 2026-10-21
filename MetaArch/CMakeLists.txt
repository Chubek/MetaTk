cmake_minimum_required(VERSION 3.16)
project(MetaArch VERSION 1.0.0 LANGUAGES C)

# ---------------------------------------------------------------------------
# MetaArch — architecture database (DomSymAsCC .dsymspec specs).
#
# Serialisation back-ends (system packages are preferred; when one is missing
# and ARCHDB_FETCH_DEPS is ON it is fetched and built statically):
#   YAML  libyaml   >= 0.2.5   (Debian/Ubuntu: libyaml-dev)
#   JSON  yyjson    >= 0.9     (libyyjson-dev)
#   XML   libxml2   >= 2.9     (libxml2-dev)
#   S-expressions: built in.
# ---------------------------------------------------------------------------

option(ARCHDB_FETCH_DEPS "Download missing dependencies with FetchContent" ON)
option(ARCHDB_BUILD_TOOLS "Build the archdb command-line tool" ON)
option(ARCHDB_BUILD_TESTS "Build the test suite" ON)
option(ARCHDB_BUILD_DSL "Build the C++20 API, architecture DSL and assembler" ON)
option(ARCHDB_TEST_CPP_API "Test the standalone C++17 API" ${ARCHDB_BUILD_DSL})
option(ARCHDB_TEST_FOREIGN_BINDINGS "Test D and Go bindings (requires a shared build)" OFF)
option(ARCHDB_BUILD_PYTHON "Build the nanobind Python extension" OFF)
option(ARCHDB_TEST_PYTHON_BINDINGS "Test the nanobind Python extension" ${ARCHDB_TEST_FOREIGN_BINDINGS})
set(ARCHDB_NANOBIND_DIR "" CACHE PATH "Local nanobind checkout (optional)")
if(ARCHDB_TEST_PYTHON_BINDINGS)
  set(ARCHDB_BUILD_PYTHON ON)
endif()
set(ARCHDB_DSLTK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk" CACHE PATH
    "Directory containing DSLtk.hpp")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
find_package(PkgConfig QUIET)

# ---- libyaml ---------------------------------------------------------------
set(ARCHDB_YAML_TARGET "")
find_package(yaml CONFIG QUIET)
if(TARGET yaml)
  set(ARCHDB_YAML_TARGET yaml)
elseif(PkgConfig_FOUND)
  pkg_check_modules(LIBYAML QUIET IMPORTED_TARGET yaml-0.1)
  if(LIBYAML_FOUND)
    set(ARCHDB_YAML_TARGET PkgConfig::LIBYAML)
  endif()
endif()
if(NOT ARCHDB_YAML_TARGET)
  if(NOT ARCHDB_FETCH_DEPS)
    message(FATAL_ERROR "libyaml not found (install libyaml-dev or enable ARCHDB_FETCH_DEPS)")
  endif()
  message(STATUS "MetaArch: fetching libyaml")
  set(BUILD_TESTING_SAVED ${BUILD_TESTING})
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(INSTALL_LIB_DIR lib CACHE STRING "" FORCE)
  FetchContent_Declare(libyaml
    GIT_REPOSITORY https://github.com/yaml/libyaml.git
    GIT_TAG 0.2.5
    GIT_SHALLOW TRUE)
  set(ARCHDB_SAVED_SHARED ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(libyaml)
  set(BUILD_SHARED_LIBS ${ARCHDB_SAVED_SHARED})
  set(BUILD_TESTING ${BUILD_TESTING_SAVED} CACHE BOOL "" FORCE)
  set_target_properties(yaml PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_definitions(yaml PUBLIC YAML_DECLARE_STATIC)
  set(ARCHDB_YAML_TARGET yaml)
endif()

# ---- yyjson ----------------------------------------------------------------
find_package(yyjson CONFIG QUIET)
if(TARGET yyjson::yyjson)
  set(ARCHDB_JSON_TARGET yyjson::yyjson)
elseif(PkgConfig_FOUND AND NOT TARGET yyjson)
  pkg_check_modules(YYJSON QUIET IMPORTED_TARGET yyjson)
  if(YYJSON_FOUND)
    set(ARCHDB_JSON_TARGET PkgConfig::YYJSON)
  endif()
endif()
if(NOT ARCHDB_JSON_TARGET)
  if(NOT ARCHDB_FETCH_DEPS)
    message(FATAL_ERROR "yyjson not found (install libyyjson-dev or enable ARCHDB_FETCH_DEPS)")
  endif()
  message(STATUS "MetaArch: fetching yyjson")
  FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.10.0
    GIT_SHALLOW TRUE)
  set(ARCHDB_SAVED_SHARED ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(yyjson)
  set(BUILD_SHARED_LIBS ${ARCHDB_SAVED_SHARED})
  set_target_properties(yyjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set(ARCHDB_JSON_TARGET yyjson)
endif()

# ---- libxml2 ---------------------------------------------------------------
find_package(LibXml2 QUIET)
if(TARGET LibXml2::LibXml2)
  set(ARCHDB_XML_TARGET LibXml2::LibXml2)
else()
  if(NOT ARCHDB_FETCH_DEPS)
    message(FATAL_ERROR "libxml2 not found (install libxml2-dev or enable ARCHDB_FETCH_DEPS)")
  endif()
  message(STATUS "MetaArch: fetching libxml2")
  foreach(opt PYTHON ICONV ZLIB LZMA ICU TESTS PROGRAMS HTTP FTP MODULES LEGACY)
    set(LIBXML2_WITH_${opt} OFF CACHE BOOL "" FORCE)
  endforeach()
  set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_Declare(libxml2
    GIT_REPOSITORY https://github.com/GNOME/libxml2.git
    GIT_TAG v2.13.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(libxml2)
  set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED})
  set_target_properties(LibXml2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set(ARCHDB_XML_TARGET LibXml2::LibXml2)
endif()

# ---- library ---------------------------------------------------------------
add_library(metaarch
  MetaArchABI.c
  Dsymspec.c
  ToYAML.c
  ToJSON.c
  ToXML.c
  ToSExpr.c
  ArchDB.c
  ArchBFD.c)
add_library(MetaArch::metaarch ALIAS metaarch)
target_include_directories(metaarch PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/metaarch>)
target_compile_definitions(metaarch PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(metaarch PRIVATE
  ${ARCHDB_YAML_TARGET} ${ARCHDB_JSON_TARGET} ${ARCHDB_XML_TARGET})
if(NOT WIN32)
  target_link_libraries(metaarch PUBLIC m)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(metaarch PRIVATE -Wall -Wextra -Wpedantic -Wshadow
    -Wno-missing-field-initializers -Wno-unused-parameter)
endif()
set_target_properties(metaarch PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  PUBLIC_HEADER "ArchDB.h;ArchBFD.h;DSymspec.h;DsymspecIR.h;MetaArchABI.h;MetaArchAPI.hpp")
set_target_properties(metaarch PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Header-only C++ API; independent of the optional DSL implementation.
add_library(metaarch_cpp_api INTERFACE)
add_library(MetaArch::api ALIAS metaarch_cpp_api)
target_link_libraries(metaarch_cpp_api INTERFACE metaarch)
target_compile_features(metaarch_cpp_api INTERFACE cxx_std_17)
if(ARCHDB_BUILD_TESTS AND ARCHDB_TEST_CPP_API AND NOT ARCHDB_BUILD_DSL)
  enable_language(CXX)
endif()

# ---- tool ------------------------------------------------------------------
if(ARCHDB_BUILD_DSL)
  enable_language(CXX)
  if(NOT EXISTS "${ARCHDB_DSLTK_DIR}/DSLtk.hpp")
    message(FATAL_ERROR "DSLtk.hpp not found; set ARCHDB_DSLTK_DIR or disable ARCHDB_BUILD_DSL")
  endif()
  add_library(metaarch_dsl MetaArch.cpp)
  add_library(MetaArch::dsl ALIAS metaarch_dsl)
  target_compile_features(metaarch_dsl PUBLIC cxx_std_20)
  target_include_directories(metaarch_dsl PRIVATE "${ARCHDB_DSLTK_DIR}")
  target_link_libraries(metaarch_dsl PUBLIC metaarch)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(metaarch_dsl PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(ARCHDB_BUILD_TOOLS)
  add_executable(archdb tools/archdb.c)
  target_compile_definitions(archdb PRIVATE _POSIX_C_SOURCE=200809L)
  target_link_libraries(archdb PRIVATE metaarch)
  add_executable(archbfd tools/archbfd.c)
  target_compile_definitions(archbfd PRIVATE _POSIX_C_SOURCE=200809L)
  target_link_libraries(archbfd PRIVATE metaarch)
  if(ARCHDB_BUILD_DSL)
    add_executable(metaarch_tool tools/metaarch.cpp)
    set_target_properties(metaarch_tool PROPERTIES OUTPUT_NAME metaarch)
    target_link_libraries(metaarch_tool PRIVATE metaarch_dsl)
  endif()
endif()

# ---- tests -----------------------------------------------------------------
if(ARCHDB_BUILD_TESTS)
  enable_testing()
  add_executable(test_archdb tests/test_archdb.c)
  target_compile_definitions(test_archdb PRIVATE _POSIX_C_SOURCE=200809L)
  target_compile_definitions(test_archdb PRIVATE
    ENCODINGS_TSV="${CMAKE_CURRENT_SOURCE_DIR}/tests/encodings.tsv")
  target_link_libraries(test_archdb PRIVATE metaarch)
  add_test(NAME archdb_roundtrip
           COMMAND test_archdb ${CMAKE_CURRENT_SOURCE_DIR}/ArchDB
                     ${CMAKE_CURRENT_SOURCE_DIR}/ArchDBCatalogue.yaml)
  add_executable(test_encode_form tests/test_encode_form.c)
  target_link_libraries(test_encode_form PRIVATE metaarch)
  add_test(NAME archdb_encode_form COMMAND test_encode_form)
  add_executable(test_relocations tests/test_relocations.c)
  target_link_libraries(test_relocations PRIVATE metaarch)
  add_test(NAME archdb_relocations COMMAND test_relocations)
  add_executable(test_archbfd tests/test_archbfd.c)
  target_compile_definitions(test_archbfd PRIVATE _POSIX_C_SOURCE=200809L)
  target_link_libraries(test_archbfd PRIVATE metaarch)
  add_test(NAME archbfd_catalogue
           COMMAND test_archbfd ${CMAKE_CURRENT_SOURCE_DIR}/ArchBFD
                   ${CMAKE_CURRENT_BINARY_DIR}/archbfd-test.o)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME archbfd_schema_check
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bfd_schema.py
                     ${CMAKE_CURRENT_SOURCE_DIR}/ArchBFD)
    set_tests_properties(archbfd_schema_check PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME archbfd_data_check
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bfd_data.py
                     ${CMAKE_CURRENT_SOURCE_DIR}/ArchBFD)
    set_tests_properties(archbfd_data_check PROPERTIES SKIP_RETURN_CODE 77)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/binutils/bfd/bfd.c")
      add_test(NAME archbfd_extractor_check
               COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_bfd.py
                       --check --binutils ${CMAKE_CURRENT_SOURCE_DIR}/binutils
                       --out ${CMAKE_CURRENT_SOURCE_DIR}/ArchBFD)
    endif()
  endif()
  if(ARCHDB_BUILD_TOOLS)
    add_test(NAME archbfd_cli
             COMMAND ${CMAKE_COMMAND}
               -DTOOL=$<TARGET_FILE:archbfd>
               -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}
               -DWORK=${CMAKE_CURRENT_BINARY_DIR}/archbfd-cli-test
               -DOBJECT=${CMAKE_CURRENT_BINARY_DIR}/archbfd-test.o
               -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_archbfd_cli.cmake)
    set_tests_properties(archbfd_cli PROPERTIES DEPENDS archbfd_catalogue)
  endif()
  add_executable(test_abi tests/test_abi.c)
  target_link_libraries(test_abi PRIVATE metaarch)
  add_test(NAME metaarch_c_abi COMMAND test_abi ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/api.yaml)
  if(ARCHDB_TEST_CPP_API)
    add_executable(test_cpp_api tests/test_cpp_api.cpp)
    set_target_properties(test_cpp_api PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_link_libraries(test_cpp_api PRIVATE MetaArch::api)
    add_test(NAME metaarch_cpp_api COMMAND test_cpp_api ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/api.yaml)
  endif()
  if(ARCHDB_TEST_FOREIGN_BINDINGS)
    if(NOT BUILD_SHARED_LIBS)
      message(FATAL_ERROR "ARCHDB_TEST_FOREIGN_BINDINGS requires BUILD_SHARED_LIBS=ON")
    endif()
    find_program(ARCHDB_D_COMPILER NAMES ldc2 dmd)
    if(ARCHDB_D_COMPILER)
      add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_bindings_d
        COMMAND ${ARCHDB_D_COMPILER} -I${CMAKE_CURRENT_SOURCE_DIR}/bindings/d
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bindings.d -L$<TARGET_FILE:metaarch>
          -of=${CMAKE_CURRENT_BINARY_DIR}/test_bindings_d
          -L-rpath=$<TARGET_FILE_DIR:metaarch>
        DEPENDS metaarch tests/test_bindings.d bindings/d/metaarch.di VERBATIM)
      add_custom_target(metaarch_d_binding_test ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_bindings_d)
      add_test(NAME metaarch_d_api COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_bindings_d
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/api.yaml)
    else()
      message(STATUS "MetaArch D binding test skipped: ldc2/dmd not found")
    endif()
    find_program(ARCHDB_GO_EXECUTABLE go)
    if(ARCHDB_GO_EXECUTABLE)
      add_test(NAME metaarch_go_api COMMAND ${CMAKE_COMMAND} -E env
        "CGO_ENABLED=1" "GOTOOLCHAIN=local" "GOPROXY=off" "GOWORK=off"
        "CGO_CFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}"
        "CGO_LDFLAGS=-L$<TARGET_FILE_DIR:metaarch> -Wl,-rpath,$<TARGET_FILE_DIR:metaarch>"
        ${ARCHDB_GO_EXECUTABLE} test -count=1 ./...)
      set_tests_properties(metaarch_go_api PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/go)
    else()
      message(STATUS "MetaArch Go binding test skipped: go not found")
    endif()
  endif()
  if(ARCHDB_BUILD_DSL)
    add_executable(test_metaarch_dsl tests/test_dsl.cpp)
    target_link_libraries(test_metaarch_dsl PRIVATE metaarch_dsl)
    add_test(NAME metaarch_dsl COMMAND test_metaarch_dsl ${CMAKE_CURRENT_SOURCE_DIR})
    if(ARCHDB_BUILD_TOOLS)
      add_test(NAME metaarch_cli COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:metaarch_tool>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake)
    endif()
  endif()
endif()

# ---- install ---------------------------------------------------------------
include(GNUInstallDirs)
if(ARCHDB_BUILD_PYTHON)
  add_subdirectory(bindings/python)
endif()
install(TARGETS metaarch
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch)
if(ARCHDB_BUILD_TOOLS)
  install(TARGETS archdb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(TARGETS archbfd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY ArchDB DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
install(DIRECTORY ArchBFD DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
install(FILES ArchDBCatalogue.yaml DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
install(FILES ArchBFDCatalogue.yaml DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
install(FILES bindings/d/metaarch.di DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/d)
install(DIRECTORY bindings/go/ DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch/bindings/go
  FILES_MATCHING PATTERN "*.go" PATTERN "go.mod" PATTERN "*_test.go" EXCLUDE)
install(DIRECTORY manual/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/manual)
if(ARCHDB_BUILD_DSL)
  install(TARGETS metaarch_dsl
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES MetaArch.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch)
  if(ARCHDB_BUILD_TOOLS)
    install(TARGETS metaarch_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
endif()
