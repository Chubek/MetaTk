cmake_minimum_required(VERSION 3.20)
project(MetaIR VERSION 1.0.0 LANGUAGES C CXX)

# Repository layout (see AGENTS.md):
#   MetaIR/include/        MetaChain.hpp (umbrella) and metachain/*.hpp (core)
#   MetaIR/satie/include/  DSLtk.hpp, the header-only DSL toolkit
#   MetaIR/{isel,isched,regalloc,graph-ir,linear-ir}  optional backend algorithms
#   qscript/               QScript language, drivers, stdlib, grammar
#   Arch/                  architecture specifications (Symspec)
#   tests/                 test programs, registered with CTest

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(METACHAIN_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/MetaIR" CACHE PATH "MetaChain core sources")
set(METACHAIN_DSLTK_DIR "${METACHAIN_CORE_DIR}/satie/include" CACHE PATH "Directory containing DSLtk.hpp")

option(METACHAIN_BUILD_TESTS "Build MetaChain tests" ON)
option(METACHAIN_BUILD_CLI "Build the textual command-line driver" ON)
option(METACHAIN_SANITIZERS "Enable ASan and UBSan on tests and drivers" OFF)
option(METACHAIN_BUILD_BACKEND "Build instruction-selection, scheduling and IR algorithms" ON)
option(METACHAIN_BUILD_QSCRIPT "Build the QScript library and drivers" ON)
option(METACHAIN_ENABLE_SYMSPEC "Load ASMtk architecture banks through Symspec (requires Boost.JSON)" OFF)
option(QSCRIPT_ENABLE_CFFI "Enable typed C calls through libffi" ON)
option(QSCRIPT_BUILD_SHARED "Build QScript as a shared library" OFF)
set(QSCRIPT_NATIVE_RUNTIME_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(QSCRIPT_NATIVE_RUNTIME_DEFAULT ON)
endif()
option(QSCRIPT_BUILD_NATIVE_RUNTIME "Build the static native grammar/match runtime (Linux x86-64)" ${QSCRIPT_NATIVE_RUNTIME_DEFAULT})

# ---------------------------------------------------------------------------
# Core interface: headers only. Every consumer gets the same include roots.
# ---------------------------------------------------------------------------
add_library(MetaChain INTERFACE)
add_library(MetaChain::MetaChain ALIAS MetaChain)
target_compile_features(MetaChain INTERFACE cxx_std_20)
target_include_directories(MetaChain INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"              # qscript/..., Arch/...
    "${METACHAIN_CORE_DIR}/include"            # MetaChain.hpp, metachain/...
    "${METACHAIN_DSLTK_DIR}")                  # DSLtk.hpp
if(METACHAIN_ENABLE_SYMSPEC)
    find_package(Boost CONFIG REQUIRED COMPONENTS json)
    target_compile_definitions(MetaChain INTERFACE METACHAIN_ENABLE_SYMSPEC=1)
    target_link_libraries(MetaChain INTERFACE Boost::json)
endif()

function(metachain_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(METACHAIN_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(metachain_executable target)
    target_link_libraries(${target} PRIVATE MetaChain::MetaChain)
    metachain_warnings(${target})
endfunction()

# ---------------------------------------------------------------------------
# Optional backend algorithms (instruction selection, scheduling, IR, regalloc).
# ---------------------------------------------------------------------------
if(METACHAIN_BUILD_BACKEND)
    file(GLOB METACHAIN_BACKEND_SOURCES CONFIGURE_DEPENDS
        "${METACHAIN_CORE_DIR}/isel/*.cpp"
        "${METACHAIN_CORE_DIR}/isched/*.cpp"
        "${METACHAIN_CORE_DIR}/regalloc/*.cpp"
        "${METACHAIN_CORE_DIR}/graph-ir/*.cpp"
        "${METACHAIN_CORE_DIR}/linear-ir/*.cpp")
    # The SAT entry point used by the SMT-based selector and constraint
    # scheduler (satie's CDCL solver, compiled with its own DSLtk namespace).
    list(APPEND METACHAIN_BACKEND_SOURCES
        "${METACHAIN_CORE_DIR}/satie/BackendSAT.cpp"
        "${METACHAIN_CORE_DIR}/satie/src/Common.cpp"
        "${METACHAIN_CORE_DIR}/satie/src/SatieMemory.cpp"
        "${METACHAIN_CORE_DIR}/satie/src/SatieCDCL.cpp")
    add_library(MetaChainBackend STATIC ${METACHAIN_BACKEND_SOURCES})
    set_target_properties(MetaChainBackend PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(MetaChain::Backend ALIAS MetaChainBackend)
    target_link_libraries(MetaChainBackend PUBLIC MetaChain::MetaChain)
    target_include_directories(MetaChainBackend PRIVATE
        "${METACHAIN_CORE_DIR}" "${METACHAIN_CORE_DIR}/satie"
        "${METACHAIN_CORE_DIR}/satie/src" "${METACHAIN_CORE_DIR}/satie/third_party/memtkx/include")
    metachain_warnings(MetaChainBackend)
endif()

# ---------------------------------------------------------------------------
# QScript: the language library and its command-line drivers.
# ---------------------------------------------------------------------------
if(METACHAIN_BUILD_QSCRIPT)
    if(QSCRIPT_BUILD_NATIVE_RUNTIME)
        add_library(QScriptNativeRuntime STATIC qscript/native/Entry.cpp qscript/native/Runtime.cpp
            qscript/src/Runtime.cpp qscript/src/Value.cpp)
        add_library(MetaChain::QScriptNativeRuntime ALIAS QScriptNativeRuntime)
        target_link_libraries(QScriptNativeRuntime PRIVATE MetaChain::MetaChain)
        target_compile_features(QScriptNativeRuntime PUBLIC cxx_std_20 c_std_99)
        # Only headers/compile configuration are needed from the core. Do not
        # export optional Boost.JSON linkage into a minimal static executable.
        set_property(TARGET QScriptNativeRuntime PROPERTY INTERFACE_LINK_LIBRARIES "")
        target_include_directories(QScriptNativeRuntime INTERFACE
            "$<TARGET_PROPERTY:MetaChain,INTERFACE_INCLUDE_DIRECTORIES>")
        # Deployment archive: static executables cannot use the ASan shared
        # runtime. The same Session/Value sources are instrumented in QScript.
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(QScriptNativeRuntime PRIVATE -Wall -Wextra -Wpedantic
                -ffunction-sections -fdata-sections -fno-sanitize=all)
        endif()
    endif()
    file(GLOB QSCRIPT_STDLIB CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qscript/stdlib/*.qs")
    set(QSCRIPT_GRAMMAR "${CMAKE_CURRENT_SOURCE_DIR}/qscript/grammar/QScript.grammar")
    set(QSCRIPT_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/qscript_embedded.cpp")
    string(REPLACE ";" "|" QSCRIPT_STDLIB_ARG "${QSCRIPT_STDLIB}")
    add_custom_command(OUTPUT "${QSCRIPT_EMBEDDED}"
        COMMAND "${CMAKE_COMMAND}" "-DGRAMMAR=${QSCRIPT_GRAMMAR}" "-DSTDLIB=${QSCRIPT_STDLIB_ARG}"
                "-DOUTPUT=${QSCRIPT_EMBEDDED}" -P "${CMAKE_CURRENT_SOURCE_DIR}/qscript/cmake/Embed.cmake"
        DEPENDS "${QSCRIPT_GRAMMAR}" ${QSCRIPT_STDLIB} "${CMAKE_CURRENT_SOURCE_DIR}/qscript/cmake/Embed.cmake"
        COMMENT "Embedding QScript grammar and standard library" VERBATIM)

    file(GLOB QSCRIPT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qscript/src/*.cpp")
    set(QSCRIPT_LIBRARY_TYPE STATIC)
    if(QSCRIPT_BUILD_SHARED)
        set(QSCRIPT_LIBRARY_TYPE SHARED)
    endif()
    add_library(QScript ${QSCRIPT_LIBRARY_TYPE}
        qscript/QScript.c qscript/QScript.cpp ${QSCRIPT_SOURCES} "${QSCRIPT_EMBEDDED}")
    add_library(MetaChain::QScript ALIAS QScript)
    set_target_properties(QScript PROPERTIES POSITION_INDEPENDENT_CODE ON WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(TARGET QScriptNativeRuntime)
        add_dependencies(QScript QScriptNativeRuntime)
        target_compile_definitions(QScript PRIVATE
            "QSCRIPT_NATIVE_LINK_DRIVER=\"${CMAKE_CXX_COMPILER}\""
            "QSCRIPT_NATIVE_RUNTIME_ARCHIVE=\"$<TARGET_FILE:QScriptNativeRuntime>\"")
    endif()
    target_link_libraries(QScript PUBLIC MetaChain::MetaChain)
    target_link_libraries(QScript PRIVATE ${CMAKE_DL_LIBS})
    if(METACHAIN_BUILD_BACKEND)
        target_link_libraries(QScript PUBLIC MetaChain::Backend)
    endif()
    if(QSCRIPT_ENABLE_CFFI)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBFFI QUIET IMPORTED_TARGET libffi)
        endif()
        if(TARGET PkgConfig::LIBFFI)
            target_link_libraries(QScript PRIVATE PkgConfig::LIBFFI)
        else()
            find_path(QSCRIPT_FFI_INCLUDE_DIR ffi.h REQUIRED)
            find_library(QSCRIPT_FFI_LIBRARY ffi REQUIRED)
            target_include_directories(QScript PRIVATE "${QSCRIPT_FFI_INCLUDE_DIR}")
            target_link_libraries(QScript PRIVATE "${QSCRIPT_FFI_LIBRARY}")
        endif()
        target_compile_definitions(QScript PRIVATE QSCRIPT_HAS_LIBFFI=1)
    endif()
    target_compile_features(QScript PUBLIC c_std_99)
    metachain_executable(QScript)

    add_executable(qscript qscript/driver/qscript-cli.c qscript/driver/qscript.c qscript/driver/qscript-repl.c)
    target_link_libraries(qscript PRIVATE QScript)
    metachain_executable(qscript)
    add_executable(qscript-repl qscript/driver/qscript.c qscript/driver/qscript-repl.c)
    target_compile_definitions(qscript-repl PRIVATE QSCRIPT_REPL_STANDALONE)
    target_link_libraries(qscript-repl PRIVATE QScript)
    metachain_executable(qscript-repl)
endif()

# ---------------------------------------------------------------------------
# Command-line driver (manual/10-text-and-cli.md).
# ---------------------------------------------------------------------------
if(METACHAIN_BUILD_CLI)
    add_executable(metachain driver/metachain.cpp)
    metachain_executable(metachain)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(METACHAIN_BUILD_TESTS)
    enable_testing()
    add_executable(metachain_tests tests/test_metachain.cpp tests/odr.cpp)
    metachain_executable(metachain_tests)
    add_test(NAME metachain.unit COMMAND metachain_tests)

    add_executable(metachain_assembler_tests tests/test_assembler.cpp)
    metachain_executable(metachain_assembler_tests)
    add_test(NAME metachain.assembler COMMAND metachain_assembler_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/Arch/stdspec/asmtk/amd64.json")
    find_program(METACHAIN_LUA_EXECUTABLE NAMES lua5.4 lua5.3 lua)
    if(METACHAIN_LUA_EXECUTABLE)
        execute_process(COMMAND "${METACHAIN_LUA_EXECUTABLE}" -e "assert(load('return 1 // 1'))"
            RESULT_VARIABLE METACHAIN_LUA_HAS_INTEGER_OPERATORS OUTPUT_QUIET ERROR_QUIET)
        if(METACHAIN_LUA_HAS_INTEGER_OPERATORS EQUAL 0)
            add_test(NAME metachain.arch.generated COMMAND "${METACHAIN_LUA_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/Arch/stdspec/tools/generate_asmtk.lua" --check)
        endif()
    endif()

    add_executable(metachain_jit_tests tests/test_jit.cpp)
    metachain_executable(metachain_jit_tests)
    add_test(NAME metachain.jit COMMAND metachain_jit_tests)

    if(METACHAIN_BUILD_BACKEND)
        add_executable(metachain_backend_tests tests/test_backend.cpp)
        target_link_libraries(metachain_backend_tests PRIVATE MetaChain::Backend)
        target_include_directories(metachain_backend_tests PRIVATE "${METACHAIN_CORE_DIR}")
        metachain_executable(metachain_backend_tests)
        add_test(NAME metachain.backend COMMAND metachain_backend_tests)
    endif()

    if(METACHAIN_BUILD_QSCRIPT)
        add_executable(qscript_tests tests/test_qscript.cpp tests/qscript_c.c)
        target_link_libraries(qscript_tests PRIVATE MetaChain::QScript)
        metachain_executable(qscript_tests)
        add_test(NAME metachain.qscript COMMAND qscript_tests)

        add_executable(qscript_value_tests tests/test_qscript_values.cpp tests/qscript_values_c.c)
        target_link_libraries(qscript_value_tests PRIVATE MetaChain::QScript)
        metachain_executable(qscript_value_tests)
        add_test(NAME metachain.qscript.values COMMAND qscript_value_tests)
        if(TARGET QScriptNativeRuntime)
            add_library(qscript_native_entry_fixture STATIC tests/qscript_native_entry_fixture.cpp)
            add_executable(qscript_native_tests tests/test_qscript_native.cpp)
            target_link_libraries(qscript_native_tests PRIVATE MetaChain::QScript)
            add_dependencies(qscript_native_tests qscript_native_entry_fixture)
            metachain_executable(qscript_native_tests)
            add_test(NAME metachain.qscript.native_backend COMMAND qscript_native_tests "$<TARGET_FILE:qscript_native_entry_fixture>")
        endif()

        # Lets the black-box C99 tests link the C++ runtime with all of its
        # configured dependencies, without hard-coding platform library flags.
        add_library(qscript_generated_fixture SHARED tests/qscript_generated_fixture.cpp)
        target_link_libraries(qscript_generated_fixture PRIVATE MetaChain::QScript)
        metachain_warnings(qscript_generated_fixture)
        if(QSCRIPT_BUILD_SHARED)
            set(QSCRIPT_GENERATED_TEST_RUNTIME "$<TARGET_FILE:QScript>")
        else()
            set(QSCRIPT_GENERATED_TEST_RUNTIME "$<TARGET_FILE:qscript_generated_fixture>")
        endif()

        add_library(qscript_ffi_fixture SHARED tests/qscript_ffi_fixture.c)
        add_executable(qscript_ffi_tests tests/qscript_ffi_c.c)
        target_link_libraries(qscript_ffi_tests PRIVATE MetaChain::QScript)
        metachain_executable(qscript_ffi_tests)
        add_test(NAME metachain.qscript.ffi COMMAND qscript_ffi_tests "$<TARGET_FILE:qscript_ffi_fixture>")

        add_executable(qscript_ffi_cpp_tests tests/test_qscript_ffi.cpp)
        target_link_libraries(qscript_ffi_cpp_tests PRIVATE MetaChain::QScript)
        metachain_executable(qscript_ffi_cpp_tests)
        add_test(NAME metachain.qscript.ffi_cpp COMMAND qscript_ffi_cpp_tests)
    endif()

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND AND TARGET metachain)
        add_test(NAME metachain.cli COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/metachain_cli.py" "$<TARGET_FILE:metachain>")
        find_program(METACHAIN_AS_EXECUTABLE NAMES as)
        find_program(METACHAIN_OBJCOPY_EXECUTABLE NAMES objcopy)
        if(METACHAIN_AS_EXECUTABLE AND METACHAIN_OBJCOPY_EXECUTABLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            execute_process(COMMAND "${METACHAIN_AS_EXECUTABLE}" --version
                OUTPUT_VARIABLE METACHAIN_AS_VERSION ERROR_QUIET)
            if(METACHAIN_AS_VERSION MATCHES "GNU assembler")
                add_test(NAME metachain.assembler.reference COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/assembler_encodings.py" "$<TARGET_FILE:metachain>"
                    "${METACHAIN_AS_EXECUTABLE}" "${METACHAIN_OBJCOPY_EXECUTABLE}")
            endif()
        endif()
    endif()
    if(Python3_Interpreter_FOUND AND METACHAIN_BUILD_QSCRIPT)
        add_test(NAME metachain.qscript.integration COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_integration.py" "$<TARGET_FILE:qscript>" "$<TARGET_FILE:qscript-repl>" "${QSCRIPT_ENABLE_CFFI}" "$<TARGET_FILE:qscript_ffi_fixture>")
        add_test(NAME metachain.qscript.c_backend COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_c_backend.py" "$<TARGET_FILE:qscript>" "${CMAKE_C_COMPILER}" "${QSCRIPT_ENABLE_CFFI}"
            "${QSCRIPT_GENERATED_TEST_RUNTIME}" "${METACHAIN_SANITIZERS}")
        add_test(NAME metachain.qscript.language COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_language.py" "$<TARGET_FILE:qscript>")
        add_test(NAME metachain.qscript.c_language COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_c_language.py" "$<TARGET_FILE:qscript>" "${CMAKE_C_COMPILER}"
            "${QSCRIPT_GENERATED_TEST_RUNTIME}" "${METACHAIN_SANITIZERS}")
        add_test(NAME metachain.qscript.native_language COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_native_language.py" "$<TARGET_FILE:qscript>" "${QSCRIPT_BUILD_NATIVE_RUNTIME}")
    endif()
endif()
