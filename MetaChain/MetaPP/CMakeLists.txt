cmake_minimum_required(VERSION 3.24)
project(MetaPP VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)

add_library(metapp src/Preprocessor.cpp src/CAPI.cpp)
add_library(MetaPP::MetaPP ALIAS metapp)
target_compile_features(metapp PUBLIC cxx_std_20)
set_target_properties(metapp PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME MetaPP WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(metapp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(metapp_cli src/Main.cpp)
set_target_properties(metapp_cli PROPERTIES OUTPUT_NAME metapp)
target_link_libraries(metapp_cli PRIVATE MetaPP::MetaPP)
if(UNIX)
    file(RELATIVE_PATH _metapp_lib_from_bin "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
    if(APPLE)
        set_target_properties(metapp_cli PROPERTIES INSTALL_RPATH "@loader_path/${_metapp_lib_from_bin}")
    else()
        set_target_properties(metapp_cli PROPERTIES INSTALL_RPATH "$ORIGIN/${_metapp_lib_from_bin}")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(metapp_tests tests/Preprocessor.cpp)
    target_link_libraries(metapp_tests PRIVATE MetaPP::MetaPP)
    add_test(NAME metapp.core COMMAND metapp_tests)
    add_test(NAME metapp.cli.help COMMAND metapp_cli --help)
    add_executable(metapp_c_tests tests/CAPI.c)
    set_target_properties(metapp_c_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_link_libraries(metapp_c_tests PRIVATE MetaPP::MetaPP)
    add_test(NAME metapp.c_abi COMMAND metapp_c_tests)
    add_test(NAME metapp.package COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
        -DCONFIG=$<CONFIG> -DEXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
        "-DGENERATOR=${CMAKE_GENERATOR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/package.cmake)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    find_program(METAPP_REFERENCE_COMPILER NAMES cc gcc clang)
    if(Python3_Interpreter_FOUND AND METAPP_REFERENCE_COMPILER)
        add_test(NAME metapp.integration COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.py
            $<TARGET_FILE:metapp_cli> ${METAPP_REFERENCE_COMPILER})
    endif()
endif()

install(TARGETS metapp metapp_cli EXPORT MetaPPTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MetaPPTargets NAMESPACE MetaPP:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MetaPP)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/MetaPPConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MetaPPConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MetaPP)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/MetaPPConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MetaPPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MetaPPConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MetaPP)
