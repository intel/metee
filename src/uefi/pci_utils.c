/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/PciSegmentLib.h>
#include <IndustryStandard/Pci22.h>

#include "metee.h"
#include "metee_efi.h"
#include "heci_efi.h"

#include "helpers.h"


/**
  Get HECI controller address that can be passed to the PCI Segment Library functions.

  @param[in] Handle  The HECI Handle to be accessed.

  @retval HECI controller address in PCI Segment Library representation
**/
UINT64
HeciPciCfgBase(
	IN struct METEE_EFI_IMPL *Handle)
{
	return PCI_SEGMENT_LIB_ADDRESS(
		Handle->Hw.Bdf.Segment,
		Handle->Hw.Bdf.Bus,
		Handle->Hw.Bdf.Device,
		Handle->Hw.Bdf.Function,
		0);
}

/**
  This function provides a standard way to verify the HECI cmd and MBAR regs
  in its PCI cfg space are setup properly.

  @param[in] Handle  The HECI Handle to be accessed.

  @retval HeciMemBar HECI Memory BAR. 
          0 - invalid BAR value returned.
**/
UINT64
CheckAndFixHeciForAccess(
	IN struct METEE_EFI_IMPL *Handle)
{
	UINT64 HeciBaseAddress;
	UINT64 MemBar;
	UINT32 LowPart;
	UINT64 HiPart;

#define B_PCI_BAR_MEMORY_TYPE_64 0x4

	HeciBaseAddress = HeciPciCfgBase(Handle);
	if (HeciBaseAddress == 0)
	{
		EFIPRINT(Handle->TeeHandle, "HeciPciCfgBase is 0\n");
		return 0;
	}
	EFIPRINT(Handle->TeeHandle, "HeciBaseAddress 0x%p\n", HeciBaseAddress);
	///
	/// Check for HECI PCI device availability
	///
	if (PciSegmentRead16(HeciBaseAddress + PCI_DEVICE_ID_OFFSET) == 0xFFFF)
	{
		EFIPRINT(Handle->TeeHandle, "is not enabled in this phase\n");
		return 0;
	}
	LowPart = PciSegmentRead32(HeciBaseAddress + PCI_BASE_ADDRESSREG_OFFSET);
	HiPart = (UINT64)PciSegmentRead32(HeciBaseAddress + (PCI_BASE_ADDRESSREG_OFFSET + 4));
	MemBar = LowPart & 0xFFFFFFF0;

	if ((LowPart & B_PCI_BAR_MEMORY_TYPE_64) == B_PCI_BAR_MEMORY_TYPE_64)
	{
		MemBar += LShiftU64(HiPart, 32);
	}

	if (MemBar == 0)
	{
		EFIPRINT(Handle->TeeHandle, "MMIO Bar for isn't programmed in this phase\n");
		return 0;
	}
	///
	/// Enable HECI MSE
	///
	PciSegmentOr8(HeciBaseAddress + PCI_COMMAND_OFFSET, EFI_PCI_COMMAND_MEMORY_SPACE);
	
	return MemBar + Handle->Hw.RegisterOffset.BaseAddressOffset;
}
