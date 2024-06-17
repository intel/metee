/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PciSegmentLib.h>
#include <Library/BaseLib.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <IndustryStandard/Pci22.h>
#include <IndustryStandard/Acpi10.h>
#include <Include/Protocol/PciRootBridgeIo.h>

#include "metee.h"
#include "metee_efi.h"
#include "heci_efi.h"

#include "helpers.h"

#define BIOS_FIXED_HOST_ADDR 0
#define POLICY_CONNECTED_HOST_ADDR 1

#define HECI_HBM_MSG_ADDR 0x00

#define NON_BLOCKING 0
#define BLOCKING 1

#define HECI_TIMEOUT_STALL_SEC (40 * 1000 * 1000) ///< 40 seconds

///
/// The default PCH PCI segment and bus number
///
#define DEFAULT_PCI_SEGMENT_NUMBER_PCH 0
#define DEFAULT_PCI_BUS_NUMBER_PCH 0

//
// CSME HECI #1
//
#define PCI_DEVICE_NUMBER_PCH_HECI1 22
#define PCI_FUNCTION_NUMBER_PCH_HECI1 0

#define CALC_EFI_PCI_ADDRESS(Bus, Dev, Func, Reg) \
	((UINT64)((((UINTN)Bus) << 24) + (((UINTN)Dev) << 16) + (((UINTN)Func) << 8) + ((UINTN)Reg)))

#pragma pack(1)
//
// Common part of the PCI configuration space header for devices, P2P bridges,
// and cardbus bridges
//
typedef struct
{
	UINT16 VendorId;
	UINT16 DeviceId;

	UINT16 Command;
	UINT16 Status;

	UINT8 RevisionId;
	UINT8 ClassCode[3];

	UINT8 CacheLineSize;
	UINT8 PrimaryLatencyTimer;
	UINT8 HeaderType;
	UINT8 BIST;

} PCI_COMMON_HEADER;

#pragma pack()

static EFI_STATUS
PciGetProtocolAndResource(
	IN EFI_HANDLE Handle,
	OUT EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL **IoDev,
	OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR **Descriptors)
/*++

Routine Description:

  This function gets the protocol interface from the given handle, and
  obtains its address space descriptors.

Arguments:

  Handle          The PCI_ROOT_BRIDIGE_IO_PROTOCOL handle
  IoDev           Handle used to access configuration space of PCI device
  Descriptors     Points to the address space descriptors

Returns:

  EFI_SUCCESS     The command completed successfully

--*/
{
	EFI_STATUS Status;

	//
	// Get inferface from protocol
	//
	Status = gBS->HandleProtocol(
		Handle,
		&gEfiPciRootBridgeIoProtocolGuid,
		IoDev);

	if (EFI_ERROR(Status))
	{
		return Status;
	}
	//
	// Call Configuration() to get address space descriptors
	//
	Status = (*IoDev)->Configuration(*IoDev, Descriptors);
	if (Status == EFI_UNSUPPORTED)
	{
		*Descriptors = NULL;
		return EFI_SUCCESS;
	}
	else
	{
		return Status;
	}
}

static EFI_STATUS
PciGetNextBusRange(
	IN OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR **Descriptors,
	OUT UINT16 *MinBus,
	OUT UINT16 *MaxBus,
	OUT BOOLEAN *IsEnd)
/*++

Routine Description:

  This function get the next bus range of given address space descriptors.
  It also moves the pointer backward a node, to get prepared to be called
  again.

Arguments:

  Descriptors     points to current position of a serial of address space
				  descriptors
  MinBus          The lower range of bus number
  ManBus          The upper range of bus number
  IsEnd           Meet end of the serial of descriptors

Returns:

  EFI_SUCCESS     The command completed successfully

--*/
{
	*IsEnd = FALSE;

	//
	// When *Descriptors is NULL, Configuration() is not implemented, so assume
	// range is 0~PCI_MAX_BUS
	//
	if ((*Descriptors) == NULL)
	{
		*MinBus = 0;
		*MaxBus = PCI_MAX_BUS;
		return EFI_SUCCESS;
	}
	//
	// *Descriptors points to one or more address space descriptors, which
	// ends with a end tagged descriptor. Examine each of the descriptors,
	// if a bus typed one is found and its bus range covers bus, this handle
	// is the handle we are looking for.
	//

	while ((*Descriptors)->Desc != ACPI_END_TAG_DESCRIPTOR)
	{
		if ((*Descriptors)->ResType == ACPI_ADDRESS_SPACE_TYPE_BUS)
		{
			*MinBus = (UINT16)(*Descriptors)->AddrRangeMin;
			*MaxBus = (UINT16)(*Descriptors)->AddrRangeMax;
			(*Descriptors)++;
			return EFI_SUCCESS;
		}

		(*Descriptors)++;
	}

	if ((*Descriptors)->Desc == ACPI_END_TAG_DESCRIPTOR)
	{
		*IsEnd = TRUE;
	}

	return EFI_SUCCESS;
}

/**
  Get HECI Bus Number

  HECI device should be located at the same bus as LPC Controller

  @retval UINT8   HECI Bus Number
**/

UINT8
LocateHeciDeviceBus(
	IN struct METEE_EFI_IMPL *Handle)
{

	UINT16 Segment;
	UINT16 Bus = 0;
	UINT16 Device;
	UINT16 Func;
	UINT64 Address;
	EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *IoDev;
	EFI_STATUS Status;
	PCI_COMMON_HEADER PciHeader;
	UINTN Index;
	UINTN SizeOfHeader;
	BOOLEAN PrintTitle;
	UINTN HandleBufSize;
	EFI_HANDLE *HandleBuf;
	UINTN HandleCount;
	EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptors;
	UINT16 MinBus;
	UINT16 MaxBus;
	BOOLEAN IsEnd;

#define INTEL_VENDOR_ID 0x8086
#define INVALID_VENDOR_ID_1 0x0000
#define INVALID_VENDOR_ID_2 0xFFFF
#define LPC_DEVICE 0x1F
#define LPC_FUNCTION 0
#define LPC_BASECLASS 0x06
#define LPC_SUBCLASS 0x01

	FUNC_ENTRY(Handle->TeeHandle);

	/* reference https://github.com/tianocore/edk-Shell/blob/master/pci/pci.c */

	//
	// Get all instances of PciRootBridgeIo. Allocate space for 1 EFI_HANDLE and
	// call LibLocateHandle(), if EFI_BUFFER_TOO_SMALL is returned, allocate enough
	// space for handles and call it again.
	//
	HandleBufSize = sizeof(EFI_HANDLE);
	HandleBuf = (EFI_HANDLE *)AllocatePool(HandleBufSize);
	if (HandleBuf == NULL)
	{
		DBGPRINT(Handle->TeeHandle, "Cannot allocate memory for HandleBuf: %d bytes\n", HandleBufSize);
		Status = EFI_OUT_OF_RESOURCES;
		goto End;
	}

	Status = gBS->LocateHandle(
		ByProtocol,
		&gEfiPciRootBridgeIoProtocolGuid,
		NULL,
		&HandleBufSize,
		HandleBuf);

	if (Status == EFI_BUFFER_TOO_SMALL)
	{
		HandleBuf = ReallocatePool(sizeof(EFI_HANDLE), HandleBufSize, HandleBuf);
		if (HandleBuf == NULL)
		{
			DBGPRINT(Handle->TeeHandle, "Cannot reallocate memory for HandleBuf: %d bytes\n", HandleBufSize);
			Status = EFI_OUT_OF_RESOURCES;
			goto End;
		}

		Status = gBS->LocateHandle(
			ByProtocol,
			&gEfiPciRootBridgeIoProtocolGuid,
			NULL,
			&HandleBufSize,
			HandleBuf);
	}

	if (EFI_ERROR(Status))
	{
		DBGPRINT(Handle->TeeHandle, "Cannot LocateHandle gEfiPciRootBridgeIoProtocolGuid, %d\n", Status);
		goto End;
	}

	HandleCount = HandleBufSize / sizeof(EFI_HANDLE);

	//
	// For each handle, which decides a segment and a bus number range,
	// enumerate all devices on it.
	//
	for (Index = 0; Index < HandleCount; Index++)
	{

		Status = PciGetProtocolAndResource(
			HandleBuf[Index],
			&IoDev,
			&Descriptors);
		if (EFI_ERROR(Status))
		{
			DBGPRINT(Handle->TeeHandle, "Cannot PciGetProtocolAndResource for Index %d, %d\n", Index, Status);
			goto End;
		}
		//
		// No document say it's impossible for a RootBridgeIo protocol handle
		// to have more than one address space descriptors, so find out every
		// bus range and for each of them do device enumeration.
		//
		while (TRUE)
		{
			Status = PciGetNextBusRange(&Descriptors, &MinBus, &MaxBus, &IsEnd);

			if (EFI_ERROR(Status))
			{
				DBGPRINT(Handle->TeeHandle, "Cannot PciGetNextBusRange for Index %d, %d\n", Index, Status);
				goto End;
			}

			if (IsEnd)
			{
				break;
			}
			MinBus = (MinBus == 0) ? 1 : MinBus;
			for (Bus = MinBus; Bus <= MaxBus; Bus++)
			{
				//
				// For each devices, enumerate all functions it contains
				//
				for (Device = 0; Device <= PCI_MAX_DEVICE; Device++)
				{
					//
					// For each function, read its configuration space and print summary
					//
					// LPC Device is always at function 0
					// Keep enumeration code to match the reference
					for (Func = 0; Func <= LPC_FUNCTION; Func++)
					{

						Address = CALC_EFI_PCI_ADDRESS(Bus, Device, Func, 0);
						IoDev->Pci.Read(
							IoDev,
							EfiPciWidthUint16,
							Address,
							1,
							&PciHeader.VendorId);
						DBGPRINT(Handle->TeeHandle, "VendorId:%X\n", PciHeader.VendorId);

						//
						// If VendorId = 0xffff, there does not exist a device at this
						// location. For each device, if there is any function on it,
						// there must be 1 function at Function 0. So if Func = 0, there
						// will be no more functions in the same device, so we can break
						// loop to deal with the next device.
						//
						if (PciHeader.VendorId == 0xffff && Func == 0)
						{
							break;
						}

						if (PciHeader.VendorId == INVALID_VENDOR_ID_1 || PciHeader.VendorId == INVALID_VENDOR_ID_2)
						{
							continue;
						}

						if (PciHeader.VendorId == INTEL_VENDOR_ID)
						{

							IoDev->Pci.Read(
								IoDev,
								EfiPciWidthUint32,
								Address,
								sizeof(PciHeader) / sizeof(UINT32),
								&PciHeader);

							if (Device == LPC_DEVICE && PciHeader.ClassCode[1] == LPC_SUBCLASS && PciHeader.ClassCode[2] == LPC_BASECLASS)
							{
								DBGPRINT(Handle->TeeHandle,
										 "LPC Device => Segment:%02X Bus:%02X Device:%02X Func:%02X VendorId:%X DeviceId:%X ClassCode[0]:%02X ClassCode[1]:%02X ClassCode[2]:%02X\n",
										 IoDev->SegmentNumber, Bus, Device, Func, PciHeader.VendorId, PciHeader.DeviceId, PciHeader.ClassCode[0], PciHeader.ClassCode[1], PciHeader.ClassCode[2]);
								goto End;
							}
						}
						//
						// If this is not a multi-function device, we can leave the loop
						// to deal with the next device.
						//
						if (Func == 0 && ((PciHeader.HeaderType & HEADER_TYPE_MULTI_FUNCTION) == 0x00))
						{
							break;
						}
					}
				}
			}
			//
			// If Descriptor is NULL, Configuration() returns EFI_UNSUPPRORED,
			// we assume the bus range is 0~PCI_MAX_BUS. After enumerated all
			// devices on all bus, we can leave loop.
			//
			if (Descriptors == NULL)
			{
				break;
			}
		}
	}
	Bus = 0;

End:
	if (HandleBuf != NULL)
	{
		FreePool(HandleBuf);
	}

	FUNC_EXIT(Handle->TeeHandle, Bus);
	return Bus;
}

/**
  Get HECI controller address that can be passed to the PCI Segment Library functions.

  @param[in] HeciFunc              HECI device function to be accessed.

  @retval HECI controller address in PCI Segment Library representation
**/
UINT64
HeciPciCfgBase(
	IN struct METEE_EFI_IMPL *Handle)
{
	if (Handle->HeciDeviceBus == -1)
	{
		Handle->HeciDeviceBus = LocateHeciDeviceBus(Handle);
	}
	return PCI_SEGMENT_LIB_ADDRESS(
		DEFAULT_PCI_SEGMENT_NUMBER_PCH,
		Handle->HeciDeviceBus,
		PCI_DEVICE_NUMBER_PCH_HECI1,
		Handle->HeciDeviceFunction,
		0);
}

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
	IN struct METEE_EFI_IMPL *Handle)
{
	UINT64 HeciBaseAddress;
	UINT64 MemBar;

#define B_PCI_BAR_MEMORY_TYPE_MASK (BIT1 | BIT2)
#define B_PCI_BAR_MEMORY_TYPE_64 BIT2

	///
	/// Check if HECI MBAR has changed
	///
	HeciBaseAddress = HeciPciCfgBase(Handle);
	if (HeciBaseAddress == 0)
	{
		DBGPRINT(Handle->TeeHandle, "HECI%d HeciPciCfgBase is 0\n", Handle->HeciDeviceFunction);
		return 0;
	}
	///
	/// Check for HECI PCI device availability
	///
	if (PciSegmentRead16(HeciBaseAddress + PCI_DEVICE_ID_OFFSET) == 0xFFFF)
	{
		DBGPRINT(Handle->TeeHandle, "HECI%d is not enabled in this phase\n", Handle->HeciDeviceFunction);
		return 0;
	}

	MemBar = PciSegmentRead32(HeciBaseAddress + PCI_BASE_ADDRESSREG_OFFSET) & 0xFFFFFFF0;
	if ((PciSegmentRead32(HeciBaseAddress + PCI_BASE_ADDRESSREG_OFFSET) & B_PCI_BAR_MEMORY_TYPE_MASK) == B_PCI_BAR_MEMORY_TYPE_64)
	{
		MemBar += LShiftU64((UINT64)PciSegmentRead32(HeciBaseAddress + (PCI_BASE_ADDRESSREG_OFFSET + 4)), 32);
	}

	if (MemBar == 0)
	{
		DBGPRINT(Handle->TeeHandle, "MMIO Bar for HECI%d isn't programmed in this phase\n", Handle->HeciDeviceFunction);
		return 0;
	}
	///
	/// Enable HECI MSE
	///
	PciSegmentOr8(HeciBaseAddress + PCI_COMMAND_OFFSET, EFI_PCI_COMMAND_MEMORY_SPACE);

	return (UINTN)MemBar;
}
