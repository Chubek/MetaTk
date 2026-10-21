# Locate the vendored QaMRpp Lua-compatible interpreter used by XBind specs.

if(DEFINED XBIND_QAMRPP_ROOT)
  set(_xbind_qamrpp_default "${XBIND_QAMRPP_ROOT}")
elseif(DEFINED LIBSTONE_THIRD_PARTY_DIR)
  set(_xbind_qamrpp_default "${LIBSTONE_THIRD_PARTY_DIR}/qamrpp")
elseif(DEFINED MLVM_THIRD_PARTY_DIR)
  set(_xbind_qamrpp_default "${MLVM_THIRD_PARTY_DIR}/qamrpp")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/qamrpp/include/QaMRpp.hpp")
  get_filename_component(_xbind_qamrpp_default
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/qamrpp" ABSOLUTE)
else()
  get_filename_component(_xbind_qamrpp_default
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/qamrpp" ABSOLUTE)
endif()

set(XBIND_QAMRPP_ROOT "${_xbind_qamrpp_default}" CACHE PATH
  "Path to the vendored QaMRpp source tree used to execute XBind Lua specs")

if(NOT EXISTS "${XBIND_QAMRPP_ROOT}/include/QaMRpp.hpp")
  message(FATAL_ERROR
    "QaMRpp was not found at XBIND_QAMRPP_ROOT=${XBIND_QAMRPP_ROOT}; "
    "expected include/QaMRpp.hpp")
endif()

if(NOT EXISTS "${XBIND_QAMRPP_ROOT}/qlib/C/QaMRpp-Library.c")
  message(FATAL_ERROR
    "QaMRpp C library ABI support was not found at ${XBIND_QAMRPP_ROOT}/qlib/C")
endif()

add_library(xbind_qamrpp_headers INTERFACE)
add_library(XBind::QaMRpp ALIAS xbind_qamrpp_headers)
target_compile_features(xbind_qamrpp_headers INTERFACE cxx_std_20)
target_include_directories(xbind_qamrpp_headers SYSTEM INTERFACE
  "${XBIND_QAMRPP_ROOT}/include"
  "${XBIND_QAMRPP_ROOT}/qlib/C"
  "${XBIND_QAMRPP_ROOT}/qlib/C++"
  "${XBIND_QAMRPP_ROOT}/third_party"
  "${XBIND_QAMRPP_ROOT}/third_party/SerdeTk/include")
if(CMAKE_DL_LIBS)
  target_link_libraries(xbind_qamrpp_headers INTERFACE ${CMAKE_DL_LIBS})
endif()

set(XBIND_QAMRPP_LIBRARY_SOURCE
  "${XBIND_QAMRPP_ROOT}/qlib/C/QaMRpp-Library.c")
