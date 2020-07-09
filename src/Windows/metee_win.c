/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2020 Intel Corporation
 */
#include <assert.h>
#include <windows.h>
#include <initguid.h>
#include "helpers.h"
#include "Public.h"
#include "metee.h"
#include "metee_win.h"

static inline struct METEE_WIN_IMPL *to_int(PTEEHANDLE _h)
{
	return _h ? (struct METEE_WIN_IMPL *)_h->handle : NULL;
}

static TEESTATUS __CreateFile(const char *devicePath, PHANDLE deviceHandle)
{
	TEESTATUS status = TEE_SUCCESS;

	*deviceHandle = CreateFileA(devicePath,
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_FLAG_OVERLAPPED,
					NULL);

	if (*deviceHandle == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		ERRPRINT("Error in CreateFile, error: %lu\n", err);
		if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
			status = TEE_DEVICE_NOT_FOUND;
		else
			status = TEE_DEVICE_NOT_READY;
	}

	return status;
}

static TEESTATUS __TeeInit(PTEEHANDLE handle, const GUID *guid, const char *devicePath)
{
	TEESTATUS       status               = INIT_STATUS;
	HANDLE          deviceHandle         = INVALID_HANDLE_VALUE;
	error_status_t  result;
	struct METEE_WIN_IMPL *impl_handle   = NULL;

	FUNC_ENTRY();

	impl_handle = (struct METEE_WIN_IMPL *)malloc(sizeof(*impl_handle));
	if (NULL == impl_handle) {
		status = TEE_INTERNAL_ERROR;
		ERRPRINT("Can't allocate memory for internal struct");
		goto Cleanup;
	}
	impl_handle->close_on_exit = true;
	impl_handle->state = METEE_CLIENT_STATE_NONE;
	impl_handle->device_path = NULL;

	__tee_init_handle(handle);
	handle->handle = impl_handle;

	status = __CreateFile(devicePath, &deviceHandle);
	if (status != TEE_SUCCESS) {
		goto Cleanup;
	}

	result  = memcpy_s(&impl_handle->guid, sizeof(impl_handle->guid),
			   guid, sizeof(GUID));
	if (result != 0) {
		ERRPRINT("Error in in guid copy: result %u\n", result);
		status = TEE_UNABLE_TO_COMPLETE_OPERATION;
		goto Cleanup;
	}

	impl_handle->device_path = _strdup(devicePath);
	if (impl_handle->device_path == NULL) {
		ERRPRINT("Error in in device path copy\n");
		status = TEE_UNABLE_TO_COMPLETE_OPERATION;
		goto Cleanup;
	}

	impl_handle->handle = deviceHandle;

	status = TEE_SUCCESS;

Cleanup:

	if (TEE_SUCCESS != status) {
		CloseHandle(deviceHandle);
		if (impl_handle)
			free(impl_handle->device_path);
		free(impl_handle);
		if (handle)
			handle->handle = NULL;
	}

	FUNC_EXIT(status);

	return status;
}

/**********************************************************************
 **                          TEE Lib Function                         *
 **********************************************************************/
TEESTATUS TEEAPI TeeInit(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			 IN OPTIONAL const char *device)
{
	TEESTATUS   status               = INIT_STATUS;
	char        devicePath[MAX_PATH] = {0};
	const char *devicePathP          = NULL;

	FUNC_ENTRY();

	if (NULL == guid || NULL == handle) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal");
		return status;
	}

	if (device != NULL) {
		devicePathP = device;
	} else {
		status = GetDevicePath(&GUID_DEVINTERFACE_HECI, devicePath, MAX_PATH);
		if (status) {
			ERRPRINT("Error in GetDevicePath, error: %d\n", status);
			return status;
		}
		devicePathP = devicePath;
	}

	status = __TeeInit(handle, guid, devicePathP);

	FUNC_EXIT(status);

	return status;
}

TEESTATUS TEEAPI TeeInitGUID(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			     IN OPTIONAL const GUID *device)
{
	TEESTATUS status               = INIT_STATUS;
	char      devicePath[MAX_PATH] = {0};
	LPCGUID   currentUUID          = NULL;

	FUNC_ENTRY();

	if (NULL == guid || NULL == handle) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal");
		return status;
	}

	currentUUID = (device != NULL) ? device : &GUID_DEVINTERFACE_HECI;

	// get device path
	status = GetDevicePath(currentUUID, devicePath, MAX_PATH);
	if (status) {
		ERRPRINT("Error in GetDevicePath, error: %d\n", status);
		return status;
	}

	status = __TeeInit(handle, guid, devicePath);

	FUNC_EXIT(status);

	return status;
}

TEESTATUS TEEAPI TeeInitHandle(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			       IN const TEE_DEVICE_HANDLE device_handle)
{
	TEESTATUS              status        = INIT_STATUS;
	struct METEE_WIN_IMPL *impl_handle   = NULL;
	error_status_t         result;

	FUNC_ENTRY();

	impl_handle = (struct METEE_WIN_IMPL *)malloc(sizeof(*impl_handle));
	if (NULL == impl_handle) {
		status = TEE_INTERNAL_ERROR;
		ERRPRINT("Can't allocate memory for internal struct");
		goto Cleanup;
	}
	impl_handle->close_on_exit = false;
	impl_handle->device_path = NULL;
	impl_handle->handle = device_handle;
	impl_handle->state = METEE_CLIENT_STATE_NONE;
	result = memcpy_s(&impl_handle->guid, sizeof(impl_handle->guid), guid, sizeof(GUID));
	if (result != 0) {
		ERRPRINT("Error in in guid copy: result %u\n", result);
		status = TEE_UNABLE_TO_COMPLETE_OPERATION;
	}

	__tee_init_handle(handle);
	handle->handle = impl_handle;

	status = TEE_SUCCESS;

Cleanup:

	if (TEE_SUCCESS != status) {
		if (impl_handle)
			free(impl_handle);
	}

	FUNC_EXIT(status);

	return status;
}


TEESTATUS TEEAPI TeeConnect(OUT PTEEHANDLE handle)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	TEESTATUS       status        = INIT_STATUS;
	DWORD           bytesReturned = 0;
	FW_CLIENT       fwClient      = {0};


	FUNC_ENTRY();

	if (NULL == impl_handle) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal\n");
		goto Cleanup;
	}

	if (impl_handle->state == METEE_CLIENT_STATE_CONNECTED) {
		status = TEE_INTERNAL_ERROR;
		ERRPRINT("The client is already connected\n");
		goto Cleanup;
	}

	if (impl_handle->state == METEE_CLIENT_STATE_FAILED && impl_handle->close_on_exit) {

		/* the handle have to be reopened in this case to reconnect to work */
		CloseHandle(impl_handle->handle);
		impl_handle->handle = NULL;
		status = __CreateFile(impl_handle->device_path, &impl_handle->handle);
		if (status != TEE_SUCCESS) {
			goto Cleanup;
		}
	}

	status = SendIOCTL(impl_handle->handle, (DWORD)IOCTL_TEEDRIVER_CONNECT_CLIENT,
						(LPVOID)&impl_handle->guid, sizeof(GUID),
						&fwClient, sizeof(FW_CLIENT),
						&bytesReturned);
	if (status) {
		DWORD err = GetLastError();
		status = Win32ErrorToTee(err);
		// Connect IOCTL returns invalid handle if client is not found
		if (status == TEE_INVALID_PARAMETER)
			status = TEE_CLIENT_NOT_FOUND;
		ERRPRINT("Error in SendIOCTL, error: %lu\n", err);
		goto Cleanup;
	}
	impl_handle->state = METEE_CLIENT_STATE_CONNECTED;

	handle->maxMsgLen  = fwClient.MaxMessageLength;
	handle->protcolVer = fwClient.ProtocolVersion;

	status = TEE_SUCCESS;

Cleanup:

	FUNC_EXIT(status);

	return status;
}

TEESTATUS TEEAPI TeeRead(IN PTEEHANDLE handle, IN OUT void* buffer, IN size_t bufferSize,
			 OUT OPTIONAL size_t* pNumOfBytesRead, IN OPTIONAL uint32_t timeout)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	TEESTATUS       status = INIT_STATUS;
	EVENTHANDLE     evt    = NULL;
	DWORD           bytesRead = 0;

	FUNC_ENTRY();

	if (NULL == impl_handle || NULL == buffer || 0 == bufferSize) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal\n");
		goto Cleanup;
	}

	if (impl_handle->state != METEE_CLIENT_STATE_CONNECTED) {
		status = TEE_DISCONNECTED;
		ERRPRINT("The client is not connected\n");
		goto Cleanup;
	}

	status = BeginReadInternal(impl_handle->handle, buffer, (ULONG)bufferSize, &evt);
	if (status) {
		ERRPRINT("Error in BeginReadInternal, error: %d\n", status);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}

	impl_handle->evt = evt;

	if (timeout == 0)
		timeout = INFINITE;

	status = EndReadInternal(impl_handle->handle, evt, timeout, &bytesRead);
	if (status) {
		ERRPRINT("Error in EndReadInternal, error: %d\n", status);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}
	if (pNumOfBytesRead != NULL) {
		*pNumOfBytesRead = bytesRead;
	}

	status = TEE_SUCCESS;

Cleanup:
	if (impl_handle)
		impl_handle->evt = NULL;

	FUNC_EXIT(status);

	return status;
}

TEESTATUS TEEAPI TeeWrite(IN PTEEHANDLE handle, IN const void* buffer, IN size_t bufferSize,
			  OUT OPTIONAL size_t* numberOfBytesWritten, IN OPTIONAL uint32_t timeout)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	TEESTATUS       status = INIT_STATUS;
	EVENTHANDLE     evt    = NULL;
	DWORD           bytesWritten = 0;

	FUNC_ENTRY();

	if (NULL == impl_handle || NULL == buffer || 0 == bufferSize) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal");
		goto Cleanup;
	}

	if (impl_handle->state != METEE_CLIENT_STATE_CONNECTED) {
		status = TEE_DISCONNECTED;
		ERRPRINT("The client is not connected");
		goto Cleanup;
	}

	status = BeginWriteInternal(impl_handle->handle, (PVOID)buffer, (ULONG)bufferSize, &evt);
	if (status) {
		ERRPRINT("Error in BeginWrite, error: %d\n", status);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}

	impl_handle->evt = evt;

	if (timeout == 0)
		timeout = INFINITE;

	status = EndWriteInternal(impl_handle->handle, evt, timeout, &bytesWritten);
	if (status) {
		ERRPRINT("Error in EndWrite, error: %d\n", status);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}
	if (numberOfBytesWritten != NULL) {
		*numberOfBytesWritten = bytesWritten;
	}

	status = TEE_SUCCESS;

Cleanup:
	if (impl_handle)
		impl_handle->evt = NULL;
	FUNC_EXIT(status);

	return status;
}

TEESTATUS TEEAPI TeeFWStatus(IN PTEEHANDLE handle,
			     IN uint32_t fwStatusNum, OUT uint32_t *fwStatus)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	TEESTATUS status = INIT_STATUS;
	DWORD bytesReturned = 0;
	DWORD fwSts = 0;
	DWORD fwStsNum = fwStatusNum;

	FUNC_ENTRY();

	if (NULL == impl_handle || NULL == fwStatus) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal");
		goto Cleanup;
	}
	if (fwStatusNum > 5) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("fwStatusNum should be 0..5");
		goto Cleanup;
	}

	status = SendIOCTL(impl_handle->handle, (DWORD)IOCTL_TEEDRIVER_GET_FW_STS,
		&fwStsNum, sizeof(DWORD),
		&fwSts, sizeof(DWORD),
		&bytesReturned);
	if (status) {
		DWORD err = GetLastError();
		status = Win32ErrorToTee(err);
		ERRPRINT("Error in SendIOCTL, error: %lu\n", err);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}

	*fwStatus = fwSts;
	status = TEE_SUCCESS;

Cleanup:
	FUNC_EXIT(status);
	return status;
}

VOID TEEAPI TeeDisconnect(IN PTEEHANDLE handle)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	DWORD ret;

	FUNC_ENTRY();
	if (NULL == impl_handle) {
		goto Cleanup;
	}

	if (CancelIo(impl_handle->handle)) {
		ret = WaitForSingleObject(impl_handle->evt, CANCEL_TIMEOUT);
		if (ret != WAIT_OBJECT_0) {
			ERRPRINT("Error in WaitForSingleObject, return: %lu, error: %lu\n",
				 ret, GetLastError());
		}
	}

	if (impl_handle->close_on_exit)
		CloseHandle(impl_handle->handle);
	free(impl_handle->device_path);
	free(impl_handle);
	handle->handle = NULL;

Cleanup:
	FUNC_EXIT(0);
}

TEE_DEVICE_HANDLE TEEAPI TeeGetDeviceHandle(IN PTEEHANDLE handle)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);

	FUNC_ENTRY();
	if (NULL == impl_handle) {
		FUNC_EXIT(TEE_INVALID_PARAMETER);
		return TEE_INVALID_DEVICE_HANDLE;
	}
	FUNC_EXIT(TEE_SUCCESS);
	return impl_handle->handle;
}

#pragma pack(1)
//HECI_VERSION_V3
struct HECI_VERSION
{
	uint16_t major;
	uint16_t minor;
	uint16_t hotfix;
	uint16_t build;
};
#pragma pack()

TEESTATUS TEEAPI GetDriverVersion(IN PTEEHANDLE handle, IN OUT teeDriverVersion_t *driverVersion)
{
	struct METEE_WIN_IMPL *impl_handle = to_int(handle);
	TEESTATUS status = INIT_STATUS;
	DWORD bytesReturned = 0;
	struct HECI_VERSION ver;

	FUNC_ENTRY();

	if (NULL == impl_handle || NULL == driverVersion) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT("One of the parameters was illegal");
		goto Cleanup;
	}

	status = SendIOCTL(impl_handle->handle, (DWORD)IOCTL_HECI_GET_VERSION,
						NULL, 0,
						&ver, sizeof(ver),
						&bytesReturned);
	if (status) {
		DWORD err = GetLastError();
		status = Win32ErrorToTee(err);
		ERRPRINT("Error in SendIOCTL, error: %lu\n", err);
		impl_handle->state = METEE_CLIENT_STATE_FAILED;
		goto Cleanup;
	}

	driverVersion->major = ver.major;
	driverVersion->minor = ver.minor;
	driverVersion->hotfix = ver.hotfix;
	driverVersion->build = ver.build;

	status = TEE_SUCCESS;

Cleanup:
	FUNC_EXIT(status);
	return status;
}
