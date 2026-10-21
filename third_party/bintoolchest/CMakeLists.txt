cmake_minimum_required(VERSION 3.20)
project(Tourchstone VERSION 0.1.0 LANGUAGES C CXX)

include(../cmake/LibstoneBuild.cmake)

option(TOURCHSTONE_BUILD_XBIND "Build the XBind generator and tool" ON)
set(_tourchstone_native_default OFF)
if(TARGET qbfd::qbfd OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../yellowstone/qbfd/CMakeLists.txt")
  set(_tourchstone_native_default ON)
endif()
option(TOURCHSTONE_BUILD_DYNLOAD "Build the dynload loader/inspection library and tool" "${_tourchstone_native_default}")
option(TOURCHSTONE_BUILD_DYNINVOKE "Build the dyninvoke foreign-call library and tool" "${_tourchstone_native_default}")
if((TOURCHSTONE_BUILD_DYNLOAD OR TOURCHSTONE_BUILD_DYNINVOKE) AND NOT _tourchstone_native_default)
  message(FATAL_ERROR "Tourchstone native loader/invocation needs the active Yellowstone qBFD component. Disable TOURCHSTONE_BUILD_DYNLOAD and TOURCHSTONE_BUILD_DYNINVOKE for XBind-only builds.")
endif()

if(TOURCHSTONE_BUILD_XBIND)
  add_subdirectory(xbind)
endif()

if(TOURCHSTONE_BUILD_DYNLOAD)
  add_subdirectory(dynload)
endif()

if(TOURCHSTONE_BUILD_DYNINVOKE)
  add_subdirectory(dyninvoke)
endif()

if(XBIND_BUILD_TESTS AND TARGET xbind_cli AND TARGET tdynload_fixture AND TARGET tdyninvoke)
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_test(NAME tourchstone.bindings
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/xbind/tests/test_codegen.py
      --root ${CMAKE_CURRENT_SOURCE_DIR}/xbind --cli $<TARGET_FILE:xbind_cli>
      --cc ${CMAKE_C_COMPILER} --cxx ${CMAKE_CXX_COMPILER}
      --work ${CMAKE_CURRENT_BINARY_DIR}
      --native-lib $<TARGET_FILE:tdynload> --native-lib $<TARGET_FILE:tdyninvoke>
      --native-lib $<TARGET_FILE:xbind>
      "--native-flags=${CMAKE_CXX_FLAGS}"
      --fixture $<TARGET_FILE:tdynload_fixture>
      Codegen.test_tourchstone_lua_bindings_execute)
  set_tests_properties(tourchstone.bindings PROPERTIES TIMEOUT 180)
endif()
