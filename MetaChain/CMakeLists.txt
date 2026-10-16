cmake_minimum_required(VERSION 3.20)
project(MetaChain VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(MetaChain INTERFACE)
add_library(MetaChain::MetaChain ALIAS MetaChain)
target_compile_features(MetaChain INTERFACE cxx_std_20)
target_include_directories(MetaChain INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
set(METACHAIN_DSLTK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk" CACHE PATH "Directory containing DSLtk.hpp")
target_include_directories(MetaChain INTERFACE "${METACHAIN_DSLTK_DIR}")
option(METACHAIN_BUILD_TESTS "Build MetaChain tests" ON)
option(METACHAIN_BUILD_CLI "Build the textual command-line driver" ON)
option(METACHAIN_SANITIZERS "Enable ASan and UBSan on tests and driver" OFF)
option(METACHAIN_BUILD_BACKEND "Build instruction-selection, scheduling and IR algorithms" ON)
option(METACHAIN_BUILD_QSCRIPT "Build the QScript C/C++ library and drivers" ON)

function(metachain_executable target)
    target_link_libraries(${target} PRIVATE MetaChain::MetaChain)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(METACHAIN_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
if(METACHAIN_BUILD_BACKEND)
    add_library(MetaChainBackend STATIC
        isel/SMT-Based.cpp isel/BURS.cpp isel/DAGCoverting.cpp
        isel/GraphCovering.cpp isel/TreeCovering.cpp isel/MacroBased.cpp
        isched/Constraint.cpp isched/DelaySlot.cpp isched/Speculative.cpp
        isched/TraceSched.cpp isched/CycleDriven.cpp isched/ListBased.cpp isched/Static.cpp
        regalloc/GraphColoring.cpp regalloc/LinearScan.cpp regalloc/Briggs.cpp
        graph-ir/GSSA.cpp graph-ir/DFG.cpp graph-ir/CFG.cpp graph-ir/PDW.cpp
        graph-ir/VGD.cpp graph-ir/PDG.cpp graph-ir/VSDG.cpp graph-ir/SeaOfNodes.cpp graph-ir/DAG.cpp
        linear-ir/Typed3AC.cpp linear-ir/Untyped3AC.cpp linear-ir/Triples.cpp
        linear-ir/Quadruples.cpp linear-ir/PseudoAssembly.cpp satie/BackendSAT.cpp satie/src/SatieMemory.cpp)
    add_library(MetaChain::Backend ALIAS MetaChainBackend)
    target_link_libraries(MetaChainBackend PUBLIC MetaChain::MetaChain)
    set_source_files_properties(satie/src/SatieMemory.cpp PROPERTIES INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/satie/include"
    )
    target_include_directories(MetaChainBackend PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/satie/third_party/memtkx/include")
    find_package(Threads REQUIRED)
    target_link_libraries(MetaChainBackend PRIVATE Threads::Threads)
    metachain_executable(MetaChainBackend)
endif()
if(METACHAIN_BUILD_QSCRIPT)
    # The QScript grammar and standard library are embedded at build time.
    file(GLOB QSCRIPT_STDLIB CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qscript/stdlib/*.qs")
    set(QSCRIPT_GRAMMAR "${CMAKE_CURRENT_SOURCE_DIR}/qscript/grammar/QScript.grammar")
    set(QSCRIPT_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/qscript_embedded.cpp")
    string(REPLACE ";" "|" QSCRIPT_STDLIB_ARG "${QSCRIPT_STDLIB}")
    add_custom_command(OUTPUT "${QSCRIPT_EMBEDDED}"
        COMMAND "${CMAKE_COMMAND}" "-DGRAMMAR=${QSCRIPT_GRAMMAR}" "-DSTDLIB=${QSCRIPT_STDLIB_ARG}"
                "-DOUTPUT=${QSCRIPT_EMBEDDED}" -P "${CMAKE_CURRENT_SOURCE_DIR}/qscript/cmake/Embed.cmake"
        DEPENDS "${QSCRIPT_GRAMMAR}" ${QSCRIPT_STDLIB} "${CMAKE_CURRENT_SOURCE_DIR}/qscript/cmake/Embed.cmake"
        COMMENT "Embedding QScript grammar and standard library" VERBATIM)
    add_library(QScript STATIC qscript/QScript.c qscript/QScript.cpp
        qscript/src/AST.cpp qscript/src/Visitor.cpp qscript/src/SemaCheck.cpp
        qscript/src/IR.cpp qscript/src/AST2IR.cpp qscript/src/Compiler.cpp
        qscript/src/Optimizer.cpp qscript/src/ISel.cpp qscript/src/ISched.cpp
        qscript/src/Emitter.cpp qscript/src/Linker.cpp qscript/src/Assembler.cpp
        qscript/src/Grammar.cpp "${QSCRIPT_EMBEDDED}")
    add_library(MetaChain::QScript ALIAS QScript)
    target_link_libraries(QScript PUBLIC MetaChain::MetaChain)
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
if(METACHAIN_BUILD_CLI)
    add_executable(metachain tools/metachain.cpp)
    metachain_executable(metachain)
endif()
if(METACHAIN_BUILD_TESTS)
    enable_testing()
    add_executable(metachain_tests tests/test_metachain.cpp tests/odr.cpp)
    metachain_executable(metachain_tests)
    add_test(NAME metachain.unit COMMAND metachain_tests)
    add_executable(metachain_jit_tests tests/test_jit.cpp)
    metachain_executable(metachain_jit_tests)
    add_test(NAME metachain.jit COMMAND metachain_jit_tests)
    add_executable(metachain_example examples/quickstart.cpp)
    metachain_executable(metachain_example)
    add_test(NAME metachain.example COMMAND metachain_example)
    if(METACHAIN_BUILD_BACKEND)
        add_executable(metachain_backend_tests tests/test_backend.cpp)
        target_link_libraries(metachain_backend_tests PRIVATE MetaChain::Backend)
        metachain_executable(metachain_backend_tests)
        add_test(NAME metachain.backend COMMAND metachain_backend_tests)
    endif()
    if(METACHAIN_BUILD_QSCRIPT)
        add_executable(qscript_tests tests/test_qscript.cpp tests/qscript_c.c)
        target_link_libraries(qscript_tests PRIVATE MetaChain::QScript)
        metachain_executable(qscript_tests)
        add_test(NAME metachain.qscript COMMAND qscript_tests)
    endif()
    foreach(header Common ParseTk Rewrite MetaVM MetaIL MetaJIT LDtk ASMtk Native Text Backend ISel ISched Regalloc)
        set(source "${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp")
        file(GENERATE OUTPUT "${source}" CONTENT "#include \"metachain/${header}.hpp\"\n")
        add_library(metachain_header_${header} OBJECT "${source}")
        target_link_libraries(metachain_header_${header} PRIVATE MetaChain::MetaChain)
    endforeach()
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND AND METACHAIN_BUILD_CLI)
        add_test(NAME metachain.integration COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.py" "$<TARGET_FILE:metachain>")
    endif()
    if(Python3_Interpreter_FOUND AND METACHAIN_BUILD_QSCRIPT)
        add_test(NAME metachain.qscript.integration COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qscript_integration.py" "$<TARGET_FILE:qscript>" "$<TARGET_FILE:qscript-repl>")
    endif()
endif()
