
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef HECI_CORE_H_
#define HECI_CORE_H_
#include "metee.h"

struct METEE_EFI_IMPL;

#define NON_BLOCKING 0
#define BLOCKING 1   

/**
	Function forces a reinit of the heci interface by following the reset heci interface via host algorithm
	in HPS section 4.1.1.1

	@param[in] Handle               The HECI handle to be accessed.

	@retval EFI_SUCCESS             Interface reset successful
	@retval EFI_TIMEOUT             ME is not ready
**/
EFI_STATUS
EFIAPI
ResetHeciInterface(
		IN struct METEE_EFI_IMPL *Handle);

/**
	Function sends one message (of any length) through the HECI circular buffer.

	@param[in] Handle               HECI handle to be accessed.
	@param[in] Message              Pointer to the message data to be sent.
	@param[in] Length               Length of the message in bytes.
	@param[in] HostAddress          The address of the host processor.
	@param[in] MeAddress            Address of the ME subsystem the message is being sent to.

	@retval EFI_SUCCESS             One message packet sent.
	@retval EFI_DEVICE_ERROR        Failed to initialize HECI
	@retval EFI_NOT_READY           HECI is not ready for communication
	@retval EFI_TIMEOUT             FW failed to empty the circular buffer
	@retval EFI_UNSUPPORTED         Current ME mode doesn't support send this message through this HECI
**/
EFI_STATUS
EFIAPI
HeciSend(
		IN struct METEE_EFI_IMPL *Handle,
		IN UINT32 *Message,
		IN UINT32 Length,
		IN UINT8 HostAddress,
		IN UINT8 MeAddress);

/**
	Reads a message from FW through HECI.

	@param[in] Handle              HECI handle to be accessed.
	@param[in] Blocking             Used to determine if the read is BLOCKING or NON_BLOCKING.
	@param[in, out] MessageBody     Pointer to a buffer used to receive a message.
	@param[in, out] Length          Pointer to the length of the buffer on input and the length
									of the message on return. (in bytes)

	@retval EFI_SUCCESS             One message packet read.
	@retval EFI_DEVICE_ERROR        Failed to initialize HECI
	@retval EFI_NOT_READY           HECI is not ready for communication
	@retval EFI_TIMEOUT             Failed to receive a full message on time
	@retval EFI_NO_RESPONSE         No response from FW
	@retval EFI_BUFFER_TOO_SMALL    The caller's buffer was not large enough
**/
EFI_STATUS
EFIAPI
HeciReceive(
		IN struct METEE_EFI_IMPL *Handle,
		IN UINT32 Blocking,
		IN OUT UINT32 *MessageBody,
		IN OUT UINT32 *Length);

#endif // HECI_CORE_H_