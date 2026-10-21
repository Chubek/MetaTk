# CLI test for the archbfd tool.
#
#   cmake -DTOOL=... -DSOURCE=... -DWORK=... -P tests/test_archbfd_cli.cmake

file(MAKE_DIRECTORY "${WORK}")

# validate every document
file(GLOB DOCS "${SOURCE}/ArchBFD/*.bfd.yaml")
execute_process(COMMAND "${TOOL}" validate ${DOCS}
                RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "archbfd validate failed: ${rc}: ${err}")
endif()

# an invalid document must be rejected with a located diagnostic
file(WRITE "${WORK}/bad.yaml" "schema: 1\nname: bad\nformat: nonsense\n")
execute_process(COMMAND "${TOOL}" validate "${WORK}/bad.yaml"
                RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT rc EQUAL 1 OR NOT err MATCHES "bad.yaml:3:")
  message(FATAL_ERROR "invalid document must fail with a location: ${rc}: ${err}")
endif()

# list the catalogue
execute_process(COMMAND "${TOOL}" list "${SOURCE}/ArchBFDCatalogue.yaml"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "ELF64")
  message(FATAL_ERROR "archbfd list failed: ${rc}: ${out} ${err}")
endif()

# info and relocations
execute_process(COMMAND "${TOOL}" info "${SOURCE}/ArchBFD/ELF64.bfd.yaml"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "elf64-x86-64" OR NOT out MATCHES "records:")
  message(FATAL_ERROR "archbfd info failed: ${rc}: ${out} ${err}")
endif()
execute_process(COMMAND "${TOOL}" relocs "${SOURCE}/ArchBFD/ELF64.bfd.yaml" R_X86_64_PC32
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "number 2")
  message(FATAL_ERROR "archbfd relocs failed: ${rc}: ${out} ${err}")
endif()
execute_process(COMMAND "${TOOL}" records "${SOURCE}/ArchBFD/ELF64.bfd.yaml" ehdr
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "e_ident")
  message(FATAL_ERROR "archbfd records failed: ${rc}: ${out} ${err}")
endif()

# convert to JSON and back
execute_process(COMMAND "${TOOL}" convert -f json "${SOURCE}/ArchBFD/ELF32.bfd.yaml"
                        -o "${WORK}/elf32.json"
                RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "archbfd convert failed: ${rc}: ${err}")
endif()
execute_process(COMMAND "${TOOL}" validate "${WORK}/elf32.json"
                RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the converted document does not validate: ${rc}: ${err}")
endif()

# the catalogue can be regenerated and must be stable
execute_process(COMMAND "${TOOL}" catalogue "${SOURCE}/ArchBFD" --root ArchBFD
                        -o "${WORK}/catalogue.yaml"
                RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "archbfd catalogue failed: ${rc}: ${err}")
endif()
file(READ "${WORK}/catalogue.yaml" generated)
file(READ "${SOURCE}/ArchBFDCatalogue.yaml" committed)
if(NOT generated STREQUAL committed)
  message(FATAL_ERROR "the committed catalogue is out of date")
endif()

# an object file is read with the description that matches it
if(EXISTS "${OBJECT}")
  execute_process(COMMAND "${TOOL}" object --catalogue "${SOURCE}/ArchBFDCatalogue.yaml"
                          "${OBJECT}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0 OR NOT out MATCHES "elf64" OR NOT out MATCHES "R_X86_64_PC32")
    message(FATAL_ERROR "archbfd object failed: ${rc}: ${out} ${err}")
  endif()
  execute_process(COMMAND "${TOOL}" object --catalogue "${SOURCE}/ArchBFDCatalogue.yaml"
                          -f ELF32 "${OBJECT}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 1 OR NOT err MATCHES "ELFCLASS")
    message(FATAL_ERROR "a mismatching description must be rejected: ${rc}: ${err}")
  endif()
endif()
