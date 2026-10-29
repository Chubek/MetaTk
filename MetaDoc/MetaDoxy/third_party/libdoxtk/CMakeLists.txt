cmake_minimum_required(VERSION 3.20)
project(DoxTk VERSION 0.1.0 LANGUAGES CXX)

option(DOXTK_BUILD_TESTS "Build DoxTk tests" ON)
option(DOXTK_BUILD_TOOLS "Build command-line tools" ON)
option(DOXTK_BUILD_DOCS "Build the manual with Doxygen" OFF)
option(DOXTK_ENABLE_CPP_DSL "Enable the optional DSLtk-backed C++ tree builder" OFF)
set(DOXTK_DSLTK_ROOT "" CACHE PATH "Explicit DSLtk source/include directory (containing DSLtk.hpp)")
option(DOXTK_ENABLE_LUA "Build Lua integration (requires Lua and binding dependencies)" OFF)
option(DOXTK_ENABLE_PDF "Build PDF output integration (requires libharu)" OFF)
option(DOXTK_ENABLE_XML "Enable pugixml-backed XML integration" OFF)
option(DOXTK_ENABLE_YAML "Enable yaml-cpp-backed YAML integration" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(doxtk
    src/DoxTk.cpp
    src/DoxTk-AST.cpp
    src/DoxTk-Binfile.cpp
    src/DoxTk-Bytecode.cpp
    src/DoxTk-Bytestream.cpp
    src/DoxTk-CLI.cpp
    src/DoxTk-Compiler.cpp
    src/DoxTk-Config.cpp
    src/DoxTk-Deserializer.cpp
    src/DoxTk-Driver.cpp
    src/DoxTk-Encoding.cpp
    src/DoxTk-EventStream.cpp
    src/DoxTk-IO.cpp
    src/DoxTk-IRDump.cpp
    src/DoxTk-Markdown.cpp
    src/DoxTk-HTML.cpp
    src/DoxTk-IPC.cpp
    src/DoxTk-Lexer.cpp
    src/DoxTk-Listener.cpp
    src/DoxTk-Lua.cpp
    src/DoxTk-PDFWriter.cpp
    src/DoxTk-Parser.cpp
    src/DoxTk-Serializer.cpp
    src/DoxTk-SpecFile.cpp
    src/DoxTk-Tooling.cpp
    src/DoxTk-Translator.cpp
    src/DoxTk-Txtfile.cpp
    src/DoxTk-Visitor.cpp
    src/DoxTk-XML.cpp
)
add_library(DoxTk::DoxTk ALIAS doxtk)
target_include_directories(doxtk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(doxtk PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(doxtk PRIVATE /W4 /permissive-)
else()
  target_compile_options(doxtk PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DOXTK_ENABLE_LUA)
  message(FATAL_ERROR "DOXTK_ENABLE_LUA is scaffold-only: provide and wire Lua/Kaguya before enabling.")
endif()
if(DOXTK_ENABLE_PDF)
  message(FATAL_ERROR "DOXTK_ENABLE_PDF is scaffold-only: provide and wire libharu before enabling.")
endif()
if(DOXTK_ENABLE_XML)
  message(FATAL_ERROR "DOXTK_ENABLE_XML is scaffold-only: provide and wire pugixml before enabling.")
endif()
if(DOXTK_ENABLE_YAML)
  message(FATAL_ERROR "DOXTK_ENABLE_YAML is scaffold-only: provide and wire yaml-cpp before enabling.")
endif()

if(DOXTK_ENABLE_CPP_DSL)
  include(cmake/CppDSL.cmake)
endif()

if(DOXTK_BUILD_TOOLS)
  add_subdirectory(toolchain)
endif()
if(DOXTK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(DOXTK_BUILD_DOCS)
  add_subdirectory(docs)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/DoxTkConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/DoxTkConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DoxTk)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/DoxTkConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(TARGETS doxtk EXPORT DoxTkTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT DoxTkTargets NAMESPACE DoxTk:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DoxTk)
set_target_properties(doxtk PROPERTIES EXPORT_NAME DoxTk)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/DoxTkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/DoxTkConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DoxTk)
