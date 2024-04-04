/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2024 Intel Corporation
 */
#include <assert.h>
#include <windows.h>
#include <initguid.h>
#include "helpers.h"
#include "metee.h"
#include "metee_win.h"
#include <cfgmgr32.h>
#include <Objbase.h>
#include <Devpkey.h>
#include <Strsafe.h>


/*********************************************************************
**                       Windows Helper Functions                   **
**********************************************************************/
void DebugPrint(const char* args, ...)
{
	char msg[DEBUG_MSG_LEN + 1];
	va_list varl;
	va_start(varl, args);
	vsprintf_s(msg, DEBUG_MSG_LEN, args, varl);
	va_end(varl);

#ifdef SYSLOG
	OutputDebugStringA(msg);
#else
	fprintf(stderr, "%s", msg);
#endif /* SYSLOG */
}

/*
**	Start Overlapped Operation
**
**	Parameters:
**
**	Return:
**		TEE_INVALID_PARAMETER
**		TEE_INTERNAL_ERROR
*/
TEESTATUS BeginOverlappedInternal(IN TEE_OPERATION operation, IN PTEEHANDLE handle,
		                  IN PVOID buffer, IN ULONG bufferSize, OUT EVENTHANDLE evt)
{
	TEESTATUS       status;
	DWORD           bytesTransferred= 0;
	BOOLEAN         optSuccesed     = FALSE;
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);

	FUNC_ENTRY(handle);

	if (INVALID_HANDLE_VALUE == impl_handle->handle || NULL == buffer || 0 == bufferSize || NULL == evt) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto Cleanup;
	}

	if (operation == ReadOperation) {
		if (ReadFile(impl_handle->handle, buffer, bufferSize, &bytesTransferred, evt)) {
			optSuccesed = TRUE;
		}
	}
	else if (operation == WriteOperation) {
		if (WriteFile(impl_handle->handle, buffer, bufferSize, &bytesTransferred, evt)) {
			optSuccesed = TRUE;
		}
	}

	if (optSuccesed == FALSE) {
		DWORD err = GetLastError();

		if (ERROR_IO_PENDING != err) {
			status = Win32ErrorToTee(err);
			ERRPRINT(handle, "Error in ReadFile/Write, error: %d\n", err);
		}
		else {
			DBGPRINT(handle, "Pending in ReadFile/Write\n");
			status = TEE_SUCCESS;
		}
	}
	else {
		status = TEE_SUCCESS;
	}

Cleanup:
	FUNC_EXIT(handle, status);

	return status;

}

TEESTATUS EndOverlapped(IN PTEEHANDLE handle, IN EVENTHANDLE evt, IN DWORD milliseconds,
			OUT OPTIONAL LPDWORD pNumberOfBytesTransferred)
{
	TEESTATUS       status;
	DWORD           err;
	DWORD           bytesTransferred        = 0;
	LPDWORD         pBytesTransferred       = NULL;
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);

	FUNC_ENTRY(handle);

	if (INVALID_HANDLE_VALUE == impl_handle->handle || NULL == evt) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto Cleanup;
	}

	pBytesTransferred = pNumberOfBytesTransferred ? pNumberOfBytesTransferred : &bytesTransferred;

	// wait for the answer
	err = WaitForSingleObject(evt->hEvent, milliseconds);
	if (err == WAIT_TIMEOUT) {
		status = TEE_TIMEOUT;
		ERRPRINT(handle, "WaitForSingleObject timed out!\n");
		goto Cleanup;
	}

	if (err != WAIT_OBJECT_0) {
		assert(WAIT_FAILED == err);
		err = GetLastError();
		status = Win32ErrorToTee(err);

		ERRPRINT(handle, "WaitForSingleObject reported error: %d\n", err);
		goto Cleanup;
	}

	 // last parameter is true b/c if we're here the operation has been completed)
	if (!GetOverlappedResult(impl_handle->handle, evt, pBytesTransferred, TRUE)) {
		err = GetLastError();
		status = Win32ErrorToTee(err);
		ERRPRINT(handle, "Error in GetOverlappedResult, error: %d\n", err);
		goto Cleanup;
	}

	status = TEE_SUCCESS; //not really needed, but for completeness...

Cleanup:
	FUNC_EXIT(handle, status);

	return status;
}

/*
**	Return the given Device Path according to it's device GUID
**
**	Parameters:
**              handle - metee handle
**		InterfaceGuid - Device GUID
**		path - Device path buffer
**		pathSize - Device Path buffer size
**
**	Return:
**		TEE_DEVICE_NOT_FOUND
**		TEE_INVALID_PARAMETER
**		TEE_INTERNAL_ERROR
*/
TEESTATUS GetDevicePath(IN PTEEHANDLE handle, IN LPCGUID InterfaceGuid,
			OUT char *path, IN SIZE_T pathSize)
{
	CONFIGRET     cr;
	char         *deviceInterfaceList         = NULL;
	ULONG         deviceInterfaceListLength   = 0;
	HRESULT       hr                          = E_FAIL;
	TEESTATUS     status                      = TEE_INTERNAL_ERROR;

	FUNC_ENTRY(handle);

	if (InterfaceGuid == NULL || path == NULL || pathSize < 1) {
		status = TEE_INTERNAL_ERROR;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto Cleanup;
	}

	path[0] = 0x00;

	cr = CM_Get_Device_Interface_List_SizeA(
		&deviceInterfaceListLength,
		(LPGUID)InterfaceGuid,
		NULL,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (cr != CR_SUCCESS) {
		ERRPRINT(handle, "Error 0x%x retrieving device interface list size.\n", cr);
		status = TEE_INTERNAL_ERROR;
		goto Cleanup;
	}

	if (deviceInterfaceListLength <= 1) {
		status = TEE_DEVICE_NOT_FOUND;
		ERRPRINT(handle, "SetupDiGetClassDevs returned status %d", GetLastError());
		goto Cleanup;
	}

	deviceInterfaceList = (char*)malloc(deviceInterfaceListLength * sizeof(char));
	if (deviceInterfaceList == NULL) {
		ERRPRINT(handle, "Error allocating memory for device interface list.\n");
		status = TEE_INTERNAL_ERROR;
		goto Cleanup;
	}
	ZeroMemory(deviceInterfaceList, deviceInterfaceListLength * sizeof(char));

	cr = CM_Get_Device_Interface_ListA(
		(LPGUID)InterfaceGuid,
		NULL,
		deviceInterfaceList,
		deviceInterfaceListLength,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (cr != CR_SUCCESS) {
		ERRPRINT(handle, "Error 0x%x retrieving device interface list.\n", cr);
		status = TEE_INTERNAL_ERROR;
		goto Cleanup;
	}

	hr = StringCchCopyA(path, pathSize, deviceInterfaceList);
	if (FAILED(hr)) {
		status = TEE_INTERNAL_ERROR;
		ERRPRINT(handle, "Error: StringCchCopy failed with HRESULT 0x%x", hr);
		goto Cleanup;
	}

	status = TEE_SUCCESS;

Cleanup:
	if (deviceInterfaceList != NULL) {
		free(deviceInterfaceList);
	}

	FUNC_EXIT(handle, status);

	return status;
}

TEESTATUS SendIOCTL(IN PTEEHANDLE handle, IN EVENTHANDLE evt, IN DWORD ioControlCode,
		    IN LPVOID pInBuffer, IN DWORD inBufferSize,
		    IN LPVOID pOutBuffer, IN DWORD outBufferSize, OUT LPDWORD pBytesRetuned)
{
	TEESTATUS       status;
	DWORD           err;
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);

	FUNC_ENTRY(handle);

	if (INVALID_HANDLE_VALUE == impl_handle->handle || NULL == pBytesRetuned) {
		status = ERROR_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto Cleanup;
	}

	if (!DeviceIoControl(impl_handle->handle, ioControlCode,
			     pInBuffer, inBufferSize,
			     pOutBuffer, outBufferSize,
			     pBytesRetuned, evt)) {

		err = GetLastError();
		// it's ok to get an error here, because it's overlapped
		if (ERROR_IO_PENDING != err) {
			ERRPRINT(handle, "Error in DeviceIoControl, error: %d\n", err);
			status = Win32ErrorToTee(err);
			goto Cleanup;
		}
	}


	if (!GetOverlappedResult(impl_handle->handle, evt, pBytesRetuned, TRUE)) {
		err = GetLastError();
		ERRPRINT(handle, "Error in GetOverlappedResult, error: %d\n", err);
		status = Win32ErrorToTee(err);
		goto Cleanup;
	}

	status = TEE_SUCCESS;

Cleanup:

	FUNC_EXIT(handle, status);

	return status;
}

TEESTATUS Win32ErrorToTee(IN DWORD win32Error)
{
	switch (win32Error) {
	case ERROR_INVALID_HANDLE:
		return TEE_INVALID_PARAMETER;
	case ERROR_INSUFFICIENT_BUFFER:
		return TEE_INSUFFICIENT_BUFFER;
	case ERROR_GEN_FAILURE:
		return TEE_UNABLE_TO_COMPLETE_OPERATION;
	case ERROR_DEVICE_NOT_CONNECTED:
		return TEE_DEVICE_NOT_READY;
	case ERROR_NOT_FOUND:
		return TEE_CLIENT_NOT_FOUND;
	case ERROR_ACCESS_DENIED:
		return TEE_PERMISSION_DENIED;
	case ERROR_OPERATION_ABORTED:
		return TEE_UNABLE_TO_COMPLETE_OPERATION;
	default:
		return TEE_INTERNAL_ERROR;
	}
}
