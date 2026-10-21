# Check the XBind specs of the project.
#
#   cmake -DXBIND=<path to the xbind tool> -DCXX=<C++ compiler>
#         -DSOURCE=<MetaArch directory> -DWORK=<scratch directory>
#         -P tests/test_xbind_specs.cmake
#
# Every spec must validate, its C signatures must match the headers and the
# Lua emitter must be able to lower it.

if(NOT XBIND)
  message(FATAL_ERROR "XBIND is not set")
endif()
if(NOT CXX)
  set(CXX c++)
endif()
file(MAKE_DIRECTORY "${WORK}")

foreach(spec MetaArch MetaBFD ArchBFD)
  set(path "${SOURCE}/${spec}.xbind")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "missing spec: ${path}")
  endif()

  execute_process(COMMAND "${XBIND}" --check-spec "${path}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${spec}: the spec does not validate: ${rc}: ${out} ${err}")
  endif()

  # The C signatures must match the headers exactly.
  execute_process(COMMAND "${XBIND}" --emit-check "${path}"
                  OUTPUT_FILE "${WORK}/${spec}-check.cpp" RESULT_VARIABLE rc
                  ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${spec}: --emit-check failed: ${rc}: ${err}")
  endif()
  execute_process(COMMAND "${CXX}" -std=c++20 -fsyntax-only "-I${SOURCE}"
                  "${WORK}/${spec}-check.cpp"
                  RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${spec}: the C signatures do not match: ${err}")
  endif()

  # The Lua emitter must be able to lower the spec.
  execute_process(COMMAND "${XBIND}" --emit-lua "${path}"
                  OUTPUT_FILE "${WORK}/${spec}-lua.cpp" RESULT_VARIABLE rc
                  ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${spec}: --emit-lua failed: ${rc}: ${err}")
  endif()
endforeach()
