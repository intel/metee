/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef HECI_PCI_UTILS_H_
#define HECI_PCI_UTILS_H_

#include "metee_efi.h"
#include "heci_efi.h"

/**
  Get HECI Bus Number

  Heuristic says that HECI device should be located at the same bus as LPC Controller

  @retval UINT8 HECI Bus Number
**/

UINT8
LocateHeciDeviceBus(
	IN struct METEE_EFI_IMPL *Handle);

/**
  Get HECI controller address that can be passed to the PCI Segment Library functions.

  @param[in] HeciFunc              HECI device function to be accessed.

  @retval HECI controller address in PCI Segment Library representation
**/
UINT64
HeciPciCfgBase(
	IN struct METEE_EFI_IMPL *Handle);

/**
  This function provides a standard way to verify the HECI cmd and MBAR regs
  in its PCI cfg space are setup properly and that the local mHeciContext
  variable matches this info.

  @param[in] HeciDev              HECI device to be accessed.

  @retval HeciMemBar              HECI Memory BAR.
								  0 - invalid BAR value returned.
**/
UINTN
CheckAndFixHeciForAccess(
	IN struct METEE_EFI_IMPL *Handle);

#endif // HECI_PCI_UTILS_H_