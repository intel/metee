# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Intel Corporation

set(TEE_SOURCES
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_efi.c
  ${PROJECT_SOURCE_DIR}/src/uefi/metee_efi.c
  ${PROJECT_SOURCE_DIR}/src/uefi/pci_utils.c
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_core.c
)

# Private headers live next to the library sources inside the staged package.
set(TEE_PRIVATE_HEADERS
  ${PROJECT_SOURCE_DIR}/include/helpers.h
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_efi.h
  ${PROJECT_SOURCE_DIR}/src/uefi/metee_efi.h
  ${PROJECT_SOURCE_DIR}/src/uefi/pci_utils.h
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_core.h
)

# Public headers live under MeTeePkg/Include and are exported via the DEC.
set(TEE_PUBLIC_HEADERS
  ${PROJECT_SOURCE_DIR}/include/metee.h
)

add_library(${PROJECT_NAME} ${TEE_SOURCES})

# Stage a self-contained EDK2 package under ${PROJECT_BINARY_DIR}/MeTeePkg so
# the tree can be copied verbatim into any EDK2 workspace. All INF/DEC/DSC
# references use only package-relative paths; source files are colocated with
# the INF that owns them.
set(METEE_UEFI_PKG_DIR "${PROJECT_BINARY_DIR}/MeTeePkg")
set(METEE_UEFI_LIB_DIR "${METEE_UEFI_PKG_DIR}/Library/MeTeeLibrary")
set(METEE_UEFI_INC_DIR "${METEE_UEFI_PKG_DIR}/Include")

set(_tee_inf_headers "")
foreach(_hdr ${TEE_PRIVATE_HEADERS})
  get_filename_component(_name "${_hdr}" NAME)
  configure_file("${_hdr}" "${METEE_UEFI_LIB_DIR}/${_name}" COPYONLY)
  list(APPEND _tee_inf_headers "  ${_name}")
endforeach()

set(_tee_inf_sources "")
foreach(_src ${TEE_SOURCES})
  get_filename_component(_name "${_src}" NAME)
  configure_file("${_src}" "${METEE_UEFI_LIB_DIR}/${_name}" COPYONLY)
  list(APPEND _tee_inf_sources "  ${_name}")
endforeach()

foreach(_pub ${TEE_PUBLIC_HEADERS})
  get_filename_component(_name "${_pub}" NAME)
  configure_file("${_pub}" "${METEE_UEFI_INC_DIR}/${_name}" COPYONLY)
endforeach()

string(REPLACE ";" "\n" TEE_HEADERS_MULTILINE "${_tee_inf_headers}")
string(REPLACE ";" "\n" TEE_SOURCES_MULTILINE "${_tee_inf_sources}")

if(BUILD_SAMPLES)
  set(METEE_SAMPLES_LIST
    MeTeePkg/Samples/MeTeeBasic/MeTeeBasic.inf
    MeTeePkg/Samples/MeTeeGsc/MeTeeGsc.inf
  )

  string(REPLACE ";" "\n" METEE_SAMPLES "${METEE_SAMPLES_LIST}")
endif(BUILD_SAMPLES)

configure_file(
  "${PROJECT_SOURCE_DIR}/src/uefi/MeTeeLibrary.inf.in"
  "${METEE_UEFI_LIB_DIR}/MeTeeLibrary.inf"
)

configure_file(
  "${PROJECT_SOURCE_DIR}/MeTeePkg/MeTeePkg.dec.in"
  "${METEE_UEFI_PKG_DIR}/MeTeePkg.dec"
)

configure_file(
  "${PROJECT_SOURCE_DIR}/MeTeePkg/MeTeePkg.dsc.in"
  "${METEE_UEFI_PKG_DIR}/MeTeePkg.dsc"
)