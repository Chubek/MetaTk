# MLVM algorithms remain QScript. This C99 target embeds their sources and
# transports immutable values through QScript's existing typed C API.
set(MLVM_BRIDGE_LIBRARIES ir parser control_flow compute optimization pipeline dataflow serialize metair)
set(MLVM_BRIDGE_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/bridge/Entry.qs")
foreach(name IN LISTS MLVM_BRIDGE_LIBRARIES)
    list(APPEND MLVM_BRIDGE_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/source/${name}.qs")
endforeach()
string(REPLACE ";" "|" MLVM_BRIDGE_LIBRARY_ARG "${MLVM_BRIDGE_LIBRARIES}")
set(MLVM_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/mlvm_sources.c")
add_custom_command(OUTPUT "${MLVM_EMBEDDED}"
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/source"
        "-DENTRY=${CMAKE_CURRENT_SOURCE_DIR}/bridge/Entry.qs" "-DLIBRARIES=${MLVM_BRIDGE_LIBRARY_ARG}"
        "-DOUTPUT=${MLVM_EMBEDDED}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Embed.cmake"
    DEPENDS ${MLVM_BRIDGE_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Embed.cmake"
    COMMENT "Embedding the QScript MLVM bridge" VERBATIM)
add_library(MLVMBridge STATIC bridge/Bridge.c "${MLVM_EMBEDDED}")
add_library(MLVM::Bridge ALIAS MLVMBridge)
set_target_properties(MLVMBridge PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(MLVMBridge PUBLIC c_std_99)
target_include_directories(MLVMBridge PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(MLVMBridge PUBLIC MetaChain::QScript)
metachain_warnings(MLVMBridge)
