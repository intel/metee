/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef HECI_EFI_H_
#define HECI_EFI_H_

#include "metee_efi.h"

EFI_STATUS
HeciConnectClient(
    IN struct METEE_EFI_IMPL *Handle);

EFI_STATUS
HeciReceiveMessage(
    IN struct METEE_EFI_IMPL *Handle,
    OUT UINT8 *Buffer,
    IN UINT32 BufferSize,
    OUT UINT32 *NumOfBytesRead,
    IN UINT32 timeout);

EFI_STATUS
HeciSendMessage(
    IN struct METEE_EFI_IMPL *Handle,
    IN const UINT8 *buffer,
    IN UINT32 bufferLength,
    OUT UINT32 *BytesWritten,
    IN UINT32 timeout);

EFI_STATUS
HeciFwStatus(
    IN struct METEE_EFI_IMPL *Handle,
    IN UINT32 fwStatusNum,
    OUT UINT32 *fwStatus);

EFI_STATUS
HeciGetTrc(
    IN struct METEE_EFI_IMPL *Handle,
    OUT UINT32 *trcVal);    

EFI_STATUS
HeciUninitialize(
    IN struct METEE_EFI_IMPL *Handle);

#endif // HECI_EFI_H_