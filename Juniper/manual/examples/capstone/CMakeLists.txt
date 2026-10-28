cmake_minimum_required(VERSION 3.24)
project(JuniperManualCapstone LANGUAGES NONE)
find_package(Juniper CONFIG REQUIRED)
option(CAPSTONE_USE_FUSION "Select the fused multiply-add profile" ON)
if(CAPSTONE_USE_FUSION)
  set(fusion 1)
else()
  set(fusion 0)
endif()

set(artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(expanded "${artifacts}/expanded.junir")
set(optimized "${artifacts}/optimized.junir")
set(scheduled "${artifacts}/scheduled.json")

juniper_compile(capstone_expand
  INPUT Juniperfile OUTPUT "${expanded}" MODE emit
  DEFINES USE_FUSION=${fusion})

add_custom_command(OUTPUT "${optimized}"
  COMMAND Juniper::juniper optimize "${expanded}" --no-preprocess
    --passes ConstantFolding,AddZeroRight --rounds 2
    -o "${optimized}" --depfile "${optimized}.d"
  DEPENDS Juniper::juniper capstone_expand "${expanded}"
  DEPFILE "${optimized}.d" VERBATIM)

add_custom_command(OUTPUT "${scheduled}"
  COMMAND Juniper::juniper schedule "${optimized}" --no-preprocess
    -o "${scheduled}" --depfile "${scheduled}.d"
  DEPENDS Juniper::juniper "${optimized}"
  DEPFILE "${scheduled}.d" VERBATIM)

add_custom_target(capstone ALL DEPENDS "${scheduled}")
