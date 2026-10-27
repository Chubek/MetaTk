cmake_minimum_required(VERSION 3.16)
# Build either as part of GPPX or separately against an installed CLI.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
 project(GPPXVMSpec LANGUAGES NONE)
 set(CMAKE_INSTALL_DATADIR "share" CACHE PATH "Read-only architecture-independent data")
 include(CTest)
 find_program(GPPX_VMSPEC_PROCESSOR NAMES gppx)
 if(NOT GPPX_VMSPEC_PROCESSOR)
  message(FATAL_ERROR "Set GPPX_VMSPEC_PROCESSOR to an installed gppx executable")
 endif()
 set(vmwv_processor "${GPPX_VMSPEC_PROCESSOR}")
 set(vmwv_dependency "${GPPX_VMSPEC_PROCESSOR}")
else()
 set(vmwv_processor "$<TARGET_FILE:gppx_cli>")
 set(vmwv_dependency gppx_cli)
endif()

set(vmwv_names P4 SECD CESK YARV "CPython Lua" CMCM CAM Krivine G-Machine
 B-Machine BCPL JVM CLR Dalvik BEAM LuaVM ZendVM V8 SOM Smalltalk80 FORTH
 ZINC TIM WAM WASM Z-Machine LispMachine)
set(vmwv_outputs)
set(vmwv_sources)
foreach(name IN LISTS vmwv_names)
 set(source "${CMAKE_CURRENT_SOURCE_DIR}/${name}.vmwv")
 set(output "${CMAKE_CURRENT_BINARY_DIR}/json/${name}.json")
 add_custom_command(OUTPUT "${output}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/json"
  COMMAND "${vmwv_processor}" --output "${output}" -- "${source}"
  DEPENDS ${vmwv_dependency} "${source}"
  COMMENT "Expanding VM specification: ${name}" VERBATIM)
 list(APPEND vmwv_outputs "${output}")
 list(APPEND vmwv_sources "${source}")
endforeach()
add_custom_target(gppx_vmspec ALL DEPENDS ${vmwv_outputs})

# The library and catalogue build need no Python dependency. When Python is
# available, test the structured output and the validator's rejection paths.
if(GPPX_BUILD_TESTS OR BUILD_TESTING)
 find_package(Python3 COMPONENTS Interpreter QUIET)
 if(Python3_Interpreter_FOUND)
  add_test(NAME gppx_vmspec_catalogue COMMAND "${Python3_EXECUTABLE}"
   "${CMAKE_CURRENT_SOURCE_DIR}/validate.py" --self-test ${vmwv_outputs})
 endif()
endif()
install(FILES ${vmwv_sources} README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/gppx/vmspec")
install(FILES ${vmwv_outputs} DESTINATION "${CMAKE_INSTALL_DATADIR}/gppx/vmspec/json")
install(FILES validate.py DESTINATION "${CMAKE_INSTALL_DATADIR}/gppx/vmspec")
