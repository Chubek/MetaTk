cmake_minimum_required(VERSION 3.20)
project(GPPXPreprocessor VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CMakePackageConfigHelpers)
check_include_file(fnmatch.h GPPX_HAVE_FNMATCH_H)

# This checkout contains the upstream, process-global GPP engine. Expose an
# executable, not the unrelated C++ API from earlier GPPX implementations.
add_executable(gppx src/gpp.c)
add_executable(GPPX::preprocessor ALIAS gppx)
set_target_properties(gppx PROPERTIES EXPORT_NAME preprocessor C_STANDARD 11
  C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_compile_definitions(gppx PRIVATE
  "PACKAGE_STRING=\"GPPX ${PROJECT_VERSION} (GPP)\""
  HAVE_FNMATCH_H=$<BOOL:${GPPX_HAVE_FNMATCH_H}> HAVE_STRCASECMP=0 HAVE_STRDUP=1)
if(WIN32)
  target_compile_definitions(gppx PRIVATE WIN_NT)
else()
  target_compile_definitions(gppx PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # Known upstream diagnostics; keep these exceptions local to upstream GPP.
  target_compile_options(gppx PRIVATE -Wall -Wextra -Wpedantic
    -Wno-unused-parameter -Wno-implicit-fallthrough)
endif()
install(TARGETS gppx EXPORT GPPXPreprocessorTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(EXPORT GPPXPreprocessorTargets NAMESPACE GPPX::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GPPXPreprocessor)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GPPXPreprocessorConfig.cmake"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/GPPXPreprocessorTargets.cmake\")\n")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/GPPXPreprocessorConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GPPXPreprocessorConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/GPPXPreprocessorConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GPPXPreprocessor)
