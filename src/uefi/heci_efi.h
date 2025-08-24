/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024-2025 Intel Corporation
 */
#ifndef HECI_EFI_H_
#define HECI_EFI_H_

#include "metee_efi.h"

EFI_STATUS
EfiTeeHeciConnectClient(
    IN struct METEE_EFI_IMPL *Handle);

EFI_STATUS
EfiTeeHeciReceiveMessage(
    IN struct METEE_EFI_IMPL *Handle,
    OUT UINT8 *Buffer,
    IN UINT32 BufferSize,
    OUT UINT32 *NumOfBytesRead,
    IN UINT32 timeout);

EFI_STATUS
EfiTeeHeciSendMessage(
    IN struct METEE_EFI_IMPL *Handle,
    IN const UINT8 *buffer,
    IN UINT32 bufferLength,
    OUT UINT32 *BytesWritten,
    IN UINT32 timeout);

EFI_STATUS
EfiTeeHeciFwStatus(
    IN struct METEE_EFI_IMPL *Handle,
    IN UINT32 fwStatusNum,
    OUT UINT32 *fwStatus);

EFI_STATUS
EfiTeeHeciGetTrc(
    IN struct METEE_EFI_IMPL *Handle,
    OUT UINT32 *trcVal);    

EFI_STATUS
EfiTeeHeciUninitialize(
    IN struct METEE_EFI_IMPL *Handle);

#endif // HECI_EFI_H_