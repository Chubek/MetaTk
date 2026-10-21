cmake_minimum_required(VERSION 3.24)

# Also usable independently: cmake -S DSLtk/tests -B build/dsltk-tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DSLtkEnumTests LANGUAGES CXX)
    enable_testing()
    add_library(DSLtk INTERFACE)
    target_include_directories(DSLtk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_features(DSLtk INTERFACE cxx_std_20)
endif()

add_executable(dsltk_enum_tests test_enum_reflection.cpp enum_odr.cpp)
target_link_libraries(dsltk_enum_tests PRIVATE DSLtk)
add_test(NAME dsltk_enum_reflection COMMAND dsltk_enum_tests)

add_executable(dsltk_enum_config_tests test_enum_config.cpp)
target_link_libraries(dsltk_enum_config_tests PRIVATE DSLtk)
add_test(NAME dsltk_enum_configuration COMMAND dsltk_enum_config_tests)

add_executable(dsltk_enum_alias_tests test_enum_aliases.cpp)
target_link_libraries(dsltk_enum_alias_tests PRIVATE DSLtk)
add_test(NAME dsltk_enum_aliases COMMAND dsltk_enum_alias_tests)

add_executable(dsltk_enum_format_tests test_enum_format.cpp)
target_link_libraries(dsltk_enum_format_tests PRIVATE DSLtk)
add_test(NAME dsltk_enum_format COMMAND dsltk_enum_format_tests)

add_executable(dsltk_language_tests test_language_facilities.cpp)
target_link_libraries(dsltk_language_tests PRIVATE DSLtk)
add_test(NAME dsltk_language_facilities COMMAND dsltk_language_tests)

add_executable(dsltk_graph_term_tests test_graph_terms.cpp)
target_link_libraries(dsltk_graph_term_tests PRIVATE DSLtk)
add_test(NAME dsltk_graph_terms COMMAND dsltk_graph_term_tests)

option(DSLtk_TEST_D "Test the D module when a D compiler is available" ON)
if(DSLtk_TEST_D)
    find_program(DSLtk_TEST_D_COMPILER NAMES ldc2 dmd gdc)
    if(DSLtk_TEST_D_COMPILER)
        set(_dsltk_d_test ${CMAKE_CURRENT_BINARY_DIR}/dsltk_d_tests${CMAKE_EXECUTABLE_SUFFIX})
        if(DSLtk_TEST_D_COMPILER MATCHES "gdc")
            set(_dsltk_d_test_flags -funittest -fmain -o ${_dsltk_d_test})
        else()
            set(_dsltk_d_test_flags -unittest -main -of=${_dsltk_d_test})
        endif()
        add_custom_command(
            OUTPUT ${_dsltk_d_test}
            COMMAND ${DSLtk_TEST_D_COMPILER} ${_dsltk_d_test_flags}
                    -I${CMAKE_CURRENT_SOURCE_DIR}/..
                    ${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk.d
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_graph_terms.d
            DEPENDS ../DSLtk.d test_graph_terms.d
            VERBATIM)
        add_custom_target(dsltk_d_test_build ALL DEPENDS ${_dsltk_d_test})
        add_test(NAME dsltk_d_module COMMAND ${_dsltk_d_test})
    endif()
endif()

option(DSLtk_TEST_LUA "Test the optional Lua preprocessing bridge" OFF)
if(DSLtk_TEST_LUA)
    find_package(Lua 5.3 REQUIRED)
    add_executable(dsltk_lua_tests test_lua_preprocessor.cpp)
    target_compile_definitions(dsltk_lua_tests PRIVATE DSLTK_ENABLE_LUA)
    target_include_directories(dsltk_lua_tests PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(dsltk_lua_tests PRIVATE DSLtk ${LUA_LIBRARIES})
    add_test(NAME dsltk_lua_preprocessor COMMAND dsltk_lua_tests)
endif()

find_package(fmt CONFIG QUIET)
if(TARGET fmt::fmt)
    add_executable(dsltk_enum_fmt_tests test_enum_format.cpp)
    target_compile_definitions(dsltk_enum_fmt_tests PRIVATE DSLTK_TEST_FMT)
    target_link_libraries(dsltk_enum_fmt_tests PRIVATE DSLtk fmt::fmt)
    add_test(NAME dsltk_enum_fmt COMMAND dsltk_enum_fmt_tests)
endif()
