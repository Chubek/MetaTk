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
  FetchContent_MakeAvailable(libyaml)
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
  FetchContent_MakeAvailable(yyjson)
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
  Dsymspec.c
  ToYAML.c
  ToJSON.c
  ToXML.c
  ToSExpr.c
  ArchDB.c)
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
  PUBLIC_HEADER "ArchDB.h;DSymspec.h;DsymspecIR.h")

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
install(TARGETS metaarch
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch)
if(ARCHDB_BUILD_TOOLS)
  install(TARGETS archdb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY ArchDB DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
install(FILES ArchDBCatalogue.yaml DESTINATION ${CMAKE_INSTALL_DATADIR}/metaarch)
if(ARCHDB_BUILD_DSL)
  install(TARGETS metaarch_dsl
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES MetaArch.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch)
  if(ARCHDB_BUILD_TOOLS)
    install(TARGETS metaarch_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
endif()
