/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef HECI_PCI_UTILS_H_
#define HECI_PCI_UTILS_H_

struct METEE_EFI_IMPL;

#include <Uefi.h>

/**
  Get HECI controller address that can be passed to the PCI Segment Library functions.

  @param[in] Handle  The HECI Handle to be accessed.

  @retval HECI controller address in PCI Segment Library representation
**/
UINT64
HeciPciCfgBase(
	IN struct METEE_EFI_IMPL *Handle);

/**
  This function provides a standard way to verify the HECI cmd and MBAR regs
  in its PCI cfg space are setup properly.

  @param[in] Handle  The HECI Handle to be accessed.

  @retval HeciMemBar HECI Memory BAR. 
          0 - invalid BAR value returned.
**/
UINT64
CheckAndFixHeciForAccess(
	IN struct METEE_EFI_IMPL *Handle);

#endif // HECI_PCI_UTILS_H_