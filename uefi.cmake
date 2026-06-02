# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Intel Corporation

set(TEE_SOURCES
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_efi.c
  ${PROJECT_SOURCE_DIR}/src/uefi/metee_efi.c
  ${PROJECT_SOURCE_DIR}/src/uefi/pci_utils.c
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_core.c
)

set(TEE_HEADERS
  ${PROJECT_SOURCE_DIR}/include/helpers.h
  ${PROJECT_SOURCE_DIR}/include/metee.h
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_efi.h
  ${PROJECT_SOURCE_DIR}/src/uefi/metee_efi.h
  ${PROJECT_SOURCE_DIR}/src/uefi/pci_utils.h
  ${PROJECT_SOURCE_DIR}/src/uefi/heci_core.h
)

add_library(${PROJECT_NAME} ${TEE_SOURCES})

string(REPLACE ";" "\n" TEE_SOURCES_MULTILINE "${TEE_SOURCES}")
string(REPLACE ";" "\n" TEE_HEADERS_MULTILINE "${TEE_HEADERS}")

configure_file (
  "${PROJECT_SOURCE_DIR}/src/uefi/MeTeeLibrary.inf.in"
  "${PROJECT_BINARY_DIR}/MeTeePkg/MeTeeLibrary/MeTeeLibrary.inf"
)

file(RELATIVE_PATH METEE_INCLUDE_RELPATH "${PROJECT_BINARY_DIR}/MeTeePkg" "${PROJECT_SOURCE_DIR}/include" )

configure_file (
  "${PROJECT_SOURCE_DIR}/MeTeePkg/MeTeePkg.dec.in"
  "${PROJECT_BINARY_DIR}/MeTeePkg/MeTeePkg.dec"
)

configure_file (
  "${PROJECT_SOURCE_DIR}/MeTeePkg/MeTeePkg.dsc.in"
  "${PROJECT_BINARY_DIR}/MeTeePkg/MeTeePkg.dsc"
)
