# Build within MetaVM root; machinery target and native support are shared.
if(NOT TARGET metavm_catalog)
  message(FATAL_ERROR "Configure the MetaVM root directory")
endif()
