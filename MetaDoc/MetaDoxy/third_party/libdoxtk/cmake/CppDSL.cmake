# Do not configure the complete metatk tree or fetch dependencies. DSLtk is a
# standalone header; install the selected header with its embedded MIT notices.
unset(DOXTK_DSLTK_INCLUDE_DIR CACHE)
unset(DOXTK_DSLTK_INCLUDE_DIR)
if(DOXTK_DSLTK_ROOT)
  find_path(DOXTK_DSLTK_INCLUDE_DIR DSLtk.hpp
    PATHS "${DOXTK_DSLTK_ROOT}" "${DOXTK_DSLTK_ROOT}/include" NO_DEFAULT_PATH)
else()
  find_package(DSLtk CONFIG QUIET)
  if(TARGET DSLtk::DSLtk)
    get_target_property(_dsltk_hints DSLtk::DSLtk INTERFACE_INCLUDE_DIRECTORIES)
  endif()
  find_path(DOXTK_DSLTK_INCLUDE_DIR DSLtk.hpp HINTS ${_dsltk_hints})
endif()
if(NOT DOXTK_DSLTK_INCLUDE_DIR)
  message(FATAL_ERROR "DOXTK_ENABLE_CPP_DSL requires DSLtk.hpp. Install DSLtk or set DOXTK_DSLTK_ROOT to third_party/metatk/DSLtk (or another header directory). No dependencies are downloaded.")
endif()

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${DOXTK_DSLTK_INCLUDE_DIR}")
# Recheck when a caller changes the dependency directory/compiler configuration.
unset(DOXTK_DSLTK_COMPATIBLE CACHE)
check_cxx_source_compiles("#include <DSLtk.hpp>
struct Probe : dsl::DSL<Probe, dsl::Pipeline> {};
int main() { auto value = Probe::wrap(1) | dsl::pipe([](int v) { return v + 1; }); return value != 2; }"
  DOXTK_DSLTK_COMPATIBLE)
cmake_pop_check_state()
if(NOT DOXTK_DSLTK_COMPATIBLE)
  message(FATAL_ERROR "The selected DSLtk.hpp cannot compile its DSL/Pipeline API with this C++20 toolchain. Check the configure compilation log or select a compatible DOXTK_DSLTK_ROOT.")
endif()

add_library(doxtk_cpp_dsl INTERFACE)
add_library(DoxTk::CPPDSL ALIAS doxtk_cpp_dsl)
set_target_properties(doxtk_cpp_dsl PROPERTIES EXPORT_NAME CPPDSL)
target_link_libraries(doxtk_cpp_dsl INTERFACE DoxTk::DoxTk)
target_compile_definitions(doxtk_cpp_dsl INTERFACE DOXTK_ENABLE_CPP_DSL=1)
target_include_directories(doxtk_cpp_dsl SYSTEM INTERFACE
  $<BUILD_INTERFACE:${DOXTK_DSLTK_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include/doxtk/dsltk>)
include(GNUInstallDirs)
install(TARGETS doxtk_cpp_dsl EXPORT DoxTkTargets)
install(FILES "${DOXTK_DSLTK_INCLUDE_DIR}/DSLtk.hpp" DESTINATION include/doxtk/dsltk)
add_executable(doxtk_cppdsl_example examples/cppdsl.cpp)
target_link_libraries(doxtk_cppdsl_example PRIVATE DoxTk::CPPDSL)
