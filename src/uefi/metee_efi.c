/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include "metee.h"
#include "metee_efi.h"

#include "heci_efi.h"
#include "helpers.h"

static inline struct METEE_EFI_IMPL *to_int(PTEEHANDLE _h)
{
	return _h ? (struct METEE_EFI_IMPL *)_h->handle : NULL;
}

#define DEFAULT_OFFSET_H_CB_WW 0x0
#define DEFAULT_OFFSET_H_CSR 0x4
#define DEFAULT_OFFSET_ME_CB_RW 0x8
#define DEFAULT_OFFSET_ME_CSR_HA 0xC

#define FW_STS1 0x40
#define FW_STS2 0x48
#define FW_STS3 0x60
#define FW_STS4 0x64
#define FW_STS5 0x68
#define FW_STS6 0x6C

#define GSC_FSTS_OFF 0xC00

static struct HECI_HW
HwInfoPch(
	IN UINT32 segment,
	IN UINT32 bus,
	IN UINT32 device,
	IN UINT32 function)
{
#define ME_TRC 0x30
	struct HECI_HW hw_info =
		{
			{/* Bdf */
			 segment,
			 bus,
			 device,
			 function},
			{/* RegisterOffset */
			 DEFAULT_OFFSET_H_CB_WW, DEFAULT_OFFSET_H_CSR,
			 DEFAULT_OFFSET_ME_CB_RW, DEFAULT_OFFSET_ME_CSR_HA,
			 0x0},
			{/* FwStatus */
			 {FW_STS1, FW_STS2, FW_STS3, FW_STS4, FW_STS5, FW_STS6},
			 TRUE},
			ME_TRC,
		};
	return hw_info;
}

static struct HECI_HW
HwInfoGfxGsc(
	IN UINT32 segment,
	IN UINT32 bus,
	IN UINT32 device,
	IN UINT32 function)
{
#define GFX_GSC_BASE_ADDRESS_OFFSET	0x374000
	struct HECI_HW hw_info =
		{
			{/* Bdf */
			 segment,
			 bus,
			 device,
			 function},
			{/* RegisterOffset */
			 DEFAULT_OFFSET_H_CB_WW, DEFAULT_OFFSET_H_CSR,
			 DEFAULT_OFFSET_ME_CB_RW, DEFAULT_OFFSET_ME_CSR_HA,
			 GFX_GSC_BASE_ADDRESS_OFFSET},
			{/* FwStatus */
			 {GSC_FSTS_OFF | FW_STS1, GSC_FSTS_OFF | FW_STS2, GSC_FSTS_OFF | FW_STS3,
			  GSC_FSTS_OFF | FW_STS4, GSC_FSTS_OFF | FW_STS5, GSC_FSTS_OFF | FW_STS6},
			 FALSE},
			0x0,
		};
	return hw_info;
}

static TEESTATUS
SetHwInfo(
	IN const struct tee_device_address *device,
	OUT struct METEE_EFI_IMPL *Handle)
{
	TEESTATUS status = TEE_INVALID_PARAMETER;
	FUNC_ENTRY(Handle->TeeHandle);
	switch (device->data.bdf.kind)
	{
	case HECI_DEVICE_KIND_PCH:
		Handle->Hw = HwInfoPch(device->data.bdf.value.segment, device->data.bdf.value.bus,
							   device->data.bdf.value.device, device->data.bdf.value.function);
		DBGPRINT(Handle->TeeHandle, "******** HECI_DEVICE_KIND_PCH\n");
		break;
	case HECI_DEVICE_KIND_GFX_GSC:
		Handle->Hw = HwInfoGfxGsc(device->data.bdf.value.segment, device->data.bdf.value.bus,
								  device->data.bdf.value.device, device->data.bdf.value.function);
		DBGPRINT(Handle->TeeHandle, "******** HECI_DEVICE_KIND_GFX_GSC\n");
		break;
	default:
		DBGPRINT(Handle->TeeHandle, "******** Unsupported device kind %d\n", device->data.bdf.kind);
		status = TEE_INVALID_PARAMETER;
		goto End;
	}
	status = TEE_SUCCESS;
End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return 0;
}

/*! Initializes TEE_DEVICE_TYPE_BDF connection specific properties
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device HECI device Bus Device Function
 *  \param impl_handle pointer to place where to store internal handle pointer
 *  \return 0 if successful, otherwise error code
 */
static TEESTATUS
TeeInitFullTypeEfiDevice(
	IN OUT PTEEHANDLE handle,
	IN const GUID *guid,
	IN const struct tee_device_address *device,
	OUT void **impl_handle)
{
	TEESTATUS status = TEE_INTERNAL_ERROR;
	struct METEE_EFI_IMPL *efi_impl = NULL;

	FUNC_ENTRY(handle);

	efi_impl = (struct METEE_EFI_IMPL *)AllocateZeroPool(sizeof(struct METEE_EFI_IMPL));
	if (efi_impl == NULL)
	{
		status = TEE_INTERNAL_ERROR;
		ERRPRINT(handle, "Can't allocate memory for internal struct");
		goto Cleanup;
	}
	efi_impl->TeeHandle = handle;
	efi_impl->ClientGuid = *guid;
	efi_impl->State = METEE_CLIENT_STATE_NONE;
	SetHwInfo(device, efi_impl);

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

		if (
			device.data.bdf.value.segment != 0 ||
			device.data.bdf.value.bus != 0 ||
			device.data.bdf.value.device != 0 ||
			device.data.bdf.value.function != 0)
		{
			ERRPRINT(handle, "BDF is set.\n");
			status = TEE_INVALID_PARAMETER;
			goto Cleanup;
		}
		struct tee_device_address default_device = device;
		// CSME HECI by default
#define PCI_DEVICE_NUMBER_PCH_HECI1 22
		default_device.data.bdf.kind = HECI_DEVICE_KIND_PCH;
		default_device.data.bdf.value.segment = 0;
		default_device.data.bdf.value.bus = 0;
		default_device.data.bdf.value.device = PCI_DEVICE_NUMBER_PCH_HECI1;
		default_device.data.bdf.value.function = 0;

		status = TeeInitFullTypeEfiDevice(handle, guid, &default_device, &handle->handle);
		if (TEE_SUCCESS != status)
		{
			goto Cleanup;
		}
		break;
	case TEE_DEVICE_TYPE_BDF:
		status = TeeInitFullTypeEfiDevice(handle, guid, &device, &handle->handle);
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
		switch (efi_status)
		{
		case EFI_NO_RESPONSE:
			status = TEE_DEVICE_NOT_READY;
			break;
		case EFI_PROTOCOL_ERROR:
			status = TEE_UNABLE_TO_COMPLETE_OPERATION;
			break;
		case EFI_NOT_FOUND:
			status = TEE_CLIENT_NOT_FOUND;
			break;
		default:
			status = TEE_INTERNAL_ERROR;
			break;
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
		if (efi_status == EFI_MEDIA_CHANGED) {
			status = TEE_DISCONNECTED;
		} else {
			status = TEE_INTERNAL_ERROR;
		}
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
		if (efi_status == EFI_MEDIA_CHANGED) {
			status = TEE_DISCONNECTED;
		} else {
			status = TEE_INTERNAL_ERROR;
		}
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
	FUNC_ENTRY(handle);
	if (NULL == fwStatus)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}
	if (fwStatusNum >= HECI_FW_STS_COUNT)
	{
		status = TEE_INVALID_PARAMETER;
		goto End;
	}
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

	FUNC_ENTRY(handle);
	if (NULL == trc_val)
	{
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}

	if (impl_handle->Hw.TrcOffset == 0x0)
	{
		status = TEE_NOTSUPPORTED;
		goto End;
	}

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

uint32_t TEEAPI TeeGetMaxMsgLen(IN const PTEEHANDLE handle)
{
	if (NULL == handle)
	{
		return 0;
	}
	return (uint32_t)handle->maxMsgLen;
}

uint8_t TEEAPI TeeGetProtocolVer(IN const PTEEHANDLE handle)
{
	if (NULL == handle)
	{
		return 0;
	}
	return handle->protcolVer;
}