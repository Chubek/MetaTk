# Explicit strata: adding a .cpp requires choosing its owner. In particular,
# deployment runtimes must not accidentally acquire the compiler via a glob.
set(QSCRIPT_PARSER_SOURCES
    qscript/src/Grammar.cpp qscript/src/AST.cpp qscript/src/Visitor.cpp)
set(QSCRIPT_ANALYSIS_SOURCES qscript/src/SemaCheck.cpp)
set(QSCRIPT_LOWERING_SOURCES qscript/src/AST2IR.cpp qscript/src/IR.cpp
    qscript/src/Optimizer.cpp qscript/src/ISched.cpp)
set(QSCRIPT_SESSION_SOURCES qscript/src/Runtime.cpp qscript/src/Value.cpp)
set(QSCRIPT_RUNTIME_ADAPTER_SOURCES qscript/src/RuntimeTables.cpp
    qscript/src/GeneratedRuntime.cpp qscript/src/FFI.cpp qscript/src/CFFI.cpp)
set(QSCRIPT_EMISSION_SOURCES qscript/src/ISel.cpp qscript/src/Emitter.cpp
    qscript/src/Assembler.cpp qscript/src/Linker.cpp qscript/src/NativeRuntime.cpp
    qscript/src/NativeBridge.cpp qscript/src/NativeLink.cpp
    qscript/src/CEmitter.cpp qscript/src/CRuntime.cpp)
set(QSCRIPT_API_SOURCES qscript/src/Compiler.cpp qscript/src/ValueAPI.cpp)
set(QSCRIPT_SOURCES ${QSCRIPT_PARSER_SOURCES} ${QSCRIPT_ANALYSIS_SOURCES}
    ${QSCRIPT_LOWERING_SOURCES} ${QSCRIPT_SESSION_SOURCES}
    ${QSCRIPT_RUNTIME_ADAPTER_SOURCES} ${QSCRIPT_EMISSION_SOURCES} ${QSCRIPT_API_SOURCES})

# A glob audits coverage only; it never chooses compiled sources.
file(GLOB QSCRIPT_SOURCE_AUDIT CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/qscript/src/*.cpp")
set(QSCRIPT_SOURCE_OWNERS ${QSCRIPT_SOURCES})
list(SORT QSCRIPT_SOURCE_AUDIT)
list(SORT QSCRIPT_SOURCE_OWNERS)
if(NOT QSCRIPT_SOURCE_AUDIT STREQUAL QSCRIPT_SOURCE_OWNERS)
    message(FATAL_ERROR "QScript source strata are incomplete or duplicate; update qscript/cmake/Sources.cmake")
endif()
