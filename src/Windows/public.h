/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2023 Intel Corporation
 */
/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

#ifndef __PUBLIC_H
#define __PUBLIC_H

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID(GUID_DEVINTERFACE_HECI, 0xE2D1FF34, 0x3458, 0x49A9,
  0x88, 0xDA, 0x8E, 0x69, 0x15, 0xCE, 0x9B, 0xE5);
// {1b6cc5ff-1bba-4a0d-9899-13427aa05156}

#define FILE_DEVICE_HECI  0x8000

// Define Interface reference/dereference routines for
// Interfaces exported by IRP_MN_QUERY_INTERFACE

#define IOCTL_TEEDRIVER_GET_VERSION \
    CTL_CODE(FILE_DEVICE_HECI, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_TEEDRIVER_CONNECT_CLIENT \
    CTL_CODE(FILE_DEVICE_HECI, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_TEEDRIVER_GET_FW_STS \
    CTL_CODE(FILE_DEVICE_HECI, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_TEEDRIVER_GET_TRC  \
    CTL_CODE(FILE_DEVICE_HECI, 0x813, METHOD_BUFFERED, FILE_READ_ACCESS|FILE_WRITE_ACCESS)

#pragma pack(1)
typedef struct _FW_CLIENT
{
	UINT32 MaxMessageLength;
	UINT8  ProtocolVersion;
} FW_CLIENT;
#pragma pack( )

#endif
