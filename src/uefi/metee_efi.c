/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

//
// Boot and Runtime Services
//
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

//
// Shell Library
//

#include "metee.h"
#include "metee_efi.h"

#include "heci_efi.h"
#include "helpers.h"

static inline struct METEE_EFI_IMPL *to_int(PTEEHANDLE _h)
{
	return _h ? (struct METEE_EFI_IMPL *)_h->handle : NULL;
}

/*! Initializes TEE_DEVICE_TYPE_EFI_DEVICE connection specific properties
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param heci_device EFI HECI protocol device number
 *  \param impl_handle pointer to place where to store internal handle pointer
 *  \return 0 if successful, otherwise error code
 */
static TEESTATUS
TeeInitFullTypeEfiDevice(
	IN OUT PTEEHANDLE handle,
	IN const GUID *guid,
	IN UINT32 heci_device,
	OUT void **impl_handle)
{
	TEESTATUS status = TEE_INTERNAL_ERROR;
	EFI_STATUS efi_status = EFI_UNSUPPORTED;
	HECI_PROTOCOL *heci_protocol = NULL;

	/// HECI protocol provided for DXE phase
	/// This protocol provides an interface to communicate with Intel ME subsystem via HECI
	EFI_GUID heci_protocol_guid = {0x3c7bc880, 0x41f8, 0x4869, {0xae, 0xfc, 0x87, 0x0a, 0x3e, 0xd2, 0x82, 0x99}};

	struct METEE_EFI_IMPL *efi_impl = NULL;

	FUNC_ENTRY(handle);

	efi_status = gBS->LocateProtocol(&heci_protocol_guid, NULL, (void **)&heci_protocol);
	if (EFI_ERROR(efi_status))
	{
		ERRPRINT(handle, "Could not locate HECI BIOS protocol %u.\n", efi_status);
		status = TEE_DEVICE_NOT_FOUND;
		goto Cleanup;
	}

	efi_impl = (struct METEE_EFI_IMPL *)AllocateZeroPool(sizeof(*impl_handle));
	if (efi_impl == NULL)
	{
		status = TEE_INTERNAL_ERROR;
		ERRPRINT(handle, "Can't allocate memory for internal struct");
		goto Cleanup;
	}
	efi_impl->TeeHandle = handle;
	efi_impl->HeciDevice = heci_device;
	efi_impl->ClientGuid = *guid;
	efi_impl->State = METEE_CLIENT_STATE_NONE;
	efi_impl->HeciProtocol = heci_protocol;

	*impl_handle = efi_impl;
	status = TEE_SUCCESS;

Cleanup:
	FUNC_EXIT(handle, status);
	return status;
}

/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device device address structure
 *  \param log_level log level to set (from enum tee_log_level)
 *  \param log_callback pointer to function to run for log write, set NULL to use built-in function
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS
TEEAPI
TeeInitFull(
	IN OUT PTEEHANDLE handle,
	IN const GUID *guid,
	IN const struct tee_device_address device,
	IN uint32_t log_level,
	IN OPTIONAL TeeLogCallback log_callback)
{
	TEESTATUS status = TEE_INTERNAL_ERROR;

	if (NULL == guid)
	{
		return TEE_INVALID_PARAMETER;
	}

	if (handle == NULL)
	{
		return TEE_INVALID_PARAMETER;
	}

	 __tee_init_handle(handle);
	handle->log_level = (log_level >= TEE_LOG_LEVEL_MAX) ? TEE_LOG_LEVEL_VERBOSE : log_level;
	handle->log_callback = log_callback;

	FUNC_ENTRY(handle);

	if (log_level >= TEE_LOG_LEVEL_MAX)
	{
		ERRPRINT(handle, "LogLevel %u is too big.\n", log_level);
		status = TEE_INVALID_PARAMETER;
		goto Cleanup;
	}

	switch (device.type)
	{
	case TEE_DEVICE_TYPE_NONE:
#define DEFAULT_UEFI_HECI_DEVICE 0
		if (device.data.handle != DEFAULT_UEFI_HECI_DEVICE) {
			ERRPRINT(handle, "Handle is set.\n");
			status = TEE_INVALID_PARAMETER;
			goto Cleanup;
		}
		status = TeeInitFullTypeEfiDevice(handle, guid, DEFAULT_UEFI_HECI_DEVICE, &handle->handle);
		if (TEE_SUCCESS != status)
		{
			goto Cleanup;
		}
		break;
	case TEE_DEVICE_TYPE_EFI_DEVICE:
		status = TeeInitFullTypeEfiDevice(handle, guid, (UINT32)((UINT64)device.data.handle & 0xFFFFFFFF), &handle->handle);
		if (TEE_SUCCESS != status)
		{
			goto Cleanup;
		}
		break;
	default:
		ERRPRINT(handle, "Wrong device type %u.\n", device.type);
		status = TEE_INVALID_PARAMETER;
		goto Cleanup;
		break;
	}

Cleanup:
	if (TEE_SUCCESS != status)
	{
		if (handle->handle)
		{
			FreePool(handle->handle);
		}
	}

	FUNC_EXIT(handle, status);
	return status;
}

/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device optional device path, set NULL to use default
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeInit(IN OUT PTEEHANDLE handle, IN const GUID *guid,
						 IN OPTIONAL const char *device)
{
	return TEE_NOTSUPPORTED;
}

/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device_handle open file handle
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeInitHandle(IN OUT PTEEHANDLE handle, IN const GUID *guid,
							   IN const TEE_DEVICE_HANDLE device_handle)
{
	return TEE_NOTSUPPORTED;
}

/*! Connects to the TEE driver and starts a session
 *  \param handle A handle to the TEE device
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeConnect(OUT PTEEHANDLE handle)
{
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	TEESTATUS status;
	EFI_STATUS efi_status;

	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (NULL == impl_handle)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto Cleanup;
	}

	if (impl_handle->State == METEE_CLIENT_STATE_CONNECTED)
	{
		status = TEE_INTERNAL_ERROR;
		ERRPRINT(handle, "The client is already connected\n");
		goto Cleanup;
	}

	efi_status = HeciConnectClient(impl_handle);
	if (EFI_ERROR(efi_status))
	{
		switch (efi_status) {
			case EFI_NO_RESPONSE: status = TEE_DEVICE_NOT_READY; break;
			case EFI_PROTOCOL_ERROR: status = TEE_UNABLE_TO_COMPLETE_OPERATION; break;
			case EFI_NOT_FOUND: status = TEE_CLIENT_NOT_FOUND; break;
			default: status = TEE_INTERNAL_ERROR; break;
		}
		goto Cleanup;
	}

	impl_handle->State = METEE_CLIENT_STATE_CONNECTED;

	handle->maxMsgLen = impl_handle->HeciClientConnection.properties.MaxMessageLength;
	handle->protcolVer = impl_handle->HeciClientConnection.properties.ProtocolVersion;

	status = TEE_SUCCESS;

Cleanup:

	FUNC_EXIT(handle, status);

	return status;
}

/*! Read data from the TEE device synchronously.
 *  \param handle The handle of the session to read from.
 *  \param buffer A pointer to a buffer that receives the data read from the TEE device.
 *  \param bufferSize The number of bytes to be read.
 *  \param pNumOfBytesRead A pointer to the variable that receives the number of bytes read,
 *         ignored if set to NULL.
 *  \param timeout The timeout to complete read in milliseconds, zero for infinite
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeRead(IN PTEEHANDLE handle, IN OUT void *buffer, IN size_t bufferSize,
						 OUT OPTIONAL size_t *pNumOfBytesRead, IN OPTIONAL uint32_t timeout)
{

	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	TEESTATUS status;
	EFI_STATUS efi_status;

	UINT32 bytesRead = 0;

	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	// HeciReceiveMessage works only with uint32_t
	if (NULL == impl_handle || NULL == buffer || 0 == bufferSize || bufferSize > 0xFFFFFFFF)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}

	if (impl_handle->State != METEE_CLIENT_STATE_CONNECTED)
	{
		status = TEE_DISCONNECTED;
		ERRPRINT(handle, "The client is not connected\n");
		goto End;
	}
	efi_status = HeciReceiveMessage(impl_handle, buffer, (UINT32)bufferSize, &bytesRead, timeout);

	if (EFI_ERROR(efi_status))
	{
		status = TEE_INTERNAL_ERROR;
		goto End;
	}

	if (pNumOfBytesRead != NULL)
	{
		*pNumOfBytesRead = bytesRead;
	}

	status = TEE_SUCCESS;

End:

	FUNC_EXIT(handle, status);

	return status;
}

/*! Writes the specified buffer to the TEE device synchronously.
 *  \param handle The handle of the session to write to.
 *  \param buffer A pointer to the buffer containing the data to be written to the TEE device.
 *  \param bufferSize The number of bytes to be written.
 *  \param numberOfBytesWritten A pointer to the variable that receives the number of bytes written,
 *         ignored if set to NULL.
 *  \param timeout The timeout to complete write in milliseconds, zero for infinite
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeWrite(IN PTEEHANDLE handle, IN const void *buffer, IN size_t bufferSize,
						  OUT OPTIONAL size_t *numberOfBytesWritten, IN OPTIONAL uint32_t timeout)
{
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	TEESTATUS status;
	EFI_STATUS efi_status;
	UINT32 bytesWritten = 0;

	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	// HeciSendMessage works only with uint32_t
	if (NULL == impl_handle || NULL == buffer || 0 == bufferSize || bufferSize > 0xFFFFFFFF)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto End;
	}

	if (impl_handle->State != METEE_CLIENT_STATE_CONNECTED)
	{
		status = TEE_DISCONNECTED;
		ERRPRINT(handle, "The client is not connected");
		goto End;
	}

	efi_status = HeciSendMessage(impl_handle, buffer, (UINT32)bufferSize, &bytesWritten, timeout);

	if (EFI_ERROR(efi_status))
	{
		status = TEE_INTERNAL_ERROR;
		goto End;
	}

	if (numberOfBytesWritten != NULL)
	{
		*numberOfBytesWritten = bytesWritten;
	}
	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);

	return status;
}

/*! Retrieves specified FW status register.
 *  \param handle The handle of the session.
 *  \param fwStatusNum The FW status register number (0-5).
 *  \param fwStatus The memory to store obtained FW status.
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeFWStatus(IN PTEEHANDLE handle,
				 IN uint32_t fwStatusNum, OUT uint32_t *fwStatus)
{
	TEESTATUS status;
	EFI_STATUS efi_status;
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}
	if (NULL == fwStatus)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}

	FUNC_ENTRY(handle);

	efi_status = HeciFwStatus(impl_handle, fwStatusNum, fwStatus);

	if (EFI_ERROR(efi_status))
	{
		status = TEE_INTERNAL_ERROR;
		goto End;
	}
	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);
	return status;
}

/*! Retrieves TRC register.
 *  \param handle The handle of the session.
 *  \param trc_val The memory to store obtained TRC value.
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeGetTRC(IN PTEEHANDLE handle, OUT uint32_t *trc_val)
{
	TEESTATUS status;
	EFI_STATUS efi_status;
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}
	if (NULL == trc_val)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}

	FUNC_ENTRY(handle);

	efi_status = HeciGetTrc(impl_handle, trc_val);

	if (EFI_ERROR(efi_status))
	{
		status = TEE_INTERNAL_ERROR;
		goto End;
	}
	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);
	return status;
}

/*! Closes the session to TEE driver
 *  Make sure that you call this function as soon as you are done with the device,
 *  as other clients might be blocked until the session is closed.
 *  \param handle The handle of the session to close.
 */
void TEEAPI TeeDisconnect(IN PTEEHANDLE handle)
{
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);

	if (NULL == handle)
	{
		return;
	}

	FUNC_ENTRY(handle);
	if (NULL == impl_handle)
	{
		goto Cleanup;
	}

	HeciUninitialize(impl_handle);

	FreePool(impl_handle);
	impl_handle = NULL;

Cleanup:
	FUNC_EXIT(handle, 0);
}

/*! Returns handle of TEE device
 *  Obtains HECI device handle on Windows and mei device file descriptor on Linux
 *  \param handle The handle of the session.
 *  \return device handle
 */
TEE_DEVICE_HANDLE TEEAPI TeeGetDeviceHandle(IN PTEEHANDLE handle)
{
	return TEE_INVALID_DEVICE_HANDLE;
}

/*! Obtains version of the TEE device driver
 *  Not implemented on Linux
 *  \param handle The handle of the session.
 *  \param driverVersion Pointer to driver version struct
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI GetDriverVersion(IN PTEEHANDLE handle, IN OUT teeDriverVersion_t *driverVersion)
{
	return TEE_NOTSUPPORTED;
}

/*! Set log level
 *
 *  \param handle The handle of the session.
 *  \param log_level log level to set
 *  \return previous log level
 */
uint32_t TEEAPI TeeSetLogLevel(IN PTEEHANDLE handle, IN uint32_t log_level)
{
	uint32_t prev_log_level = TEE_LOG_LEVEL_QUIET;

	if (NULL == handle)
	{
		return prev_log_level;
	}
	FUNC_ENTRY(handle);

	prev_log_level = handle->log_level;
	handle->log_level = (log_level > TEE_LOG_LEVEL_VERBOSE) ? TEE_LOG_LEVEL_VERBOSE : log_level;

	FUNC_EXIT(handle, prev_log_level);
	return prev_log_level;
}

/*! Retrieve current log level
 *
 *  \param handle The handle of the session.
 *  \return current log level
 */
uint32_t TEEAPI TeeGetLogLevel(IN const PTEEHANDLE handle)
{
	uint32_t prev_log_level = TEE_LOG_LEVEL_QUIET;

	if (NULL == handle)
	{
		return prev_log_level;
	}
	FUNC_ENTRY(handle);

	prev_log_level = handle->log_level;

	FUNC_EXIT(handle, prev_log_level);

	return prev_log_level;
}

/*! Set log callback
 *
 *  \param handle The handle of the session.
 *  \param log_callback pointer to function to run for log write, set NULL to use built-in function
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeSetLogCallback(IN const PTEEHANDLE handle, TeeLogCallback log_callback)
{
	struct METEE_EFI_IMPL *impl_handle = to_int(handle);
	TEESTATUS status;

	if (NULL == handle)
	{
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (NULL == impl_handle)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto Cleanup;
	}

	handle->log_callback = log_callback;
	status = TEE_SUCCESS;

Cleanup:
	FUNC_EXIT(handle, status);
	return status;
}