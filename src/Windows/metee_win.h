/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2025 Intel Corporation
 */
#ifndef __TEELIBWIN_H
#define __TEELIBWIN_H

#include <Windows.h>
#include <stdbool.h>
#include "metee.h"

#define CANCEL_TIMEOUT 5000

enum METEE_CLIENT_STATE
{
	METEE_CLIENT_STATE_NONE,
	METEE_CLIENT_STATE_CONNECTED,
	METEE_CLIENT_STATE_FAILED
};

#define METEE_WIN_EVT_IOCTL 0
#define METEE_WIN_EVT_READ  1
#define METEE_WIN_EVT_WRITE 2
#define MAX_EVT 3

struct METEE_WIN_IMPL
{
	HANDLE handle;            /**< file descriptor - Handle to the Device File */
	GUID   guid;              /**< fw client guid */
	LPOVERLAPPED evt[MAX_EVT]; /**< event for executing async */
	bool close_on_exit;       /**< close handle on exit */
	enum METEE_CLIENT_STATE state; /**< the client state */
	char *device_path;        /**< device path */
};

/*********************************************************************
**                       Windows Helper Types                       **
**********************************************************************/
typedef LPOVERLAPPED EVENTHANDLE, *PEVENTHANDLE;

typedef enum _TEE_OPERATION
{
	ReadOperation,
	WriteOperation
} TEE_OPERATION;

/*********************************************************************
**                Windows Helper Functions                          **
**********************************************************************/
TEESTATUS BeginOverlappedInternal(IN TEE_OPERATION operation,
				 IN PTEEHANDLE handle, IN PVOID buffer,
				 IN ULONG bufferSize, OUT EVENTHANDLE evt);
TEESTATUS EndOverlapped(IN PTEEHANDLE handle, IN EVENTHANDLE evt, IN DWORD milliseconds,
			OUT OPTIONAL LPDWORD pNumberOfBytesTransferred);
TEESTATUS GetDevicePath(IN PTEEHANDLE handle, IN LPCGUID InterfaceGuid,
			OUT char *path, IN SIZE_T pathSize);
TEESTATUS GetDeviceKind(IN PTEEHANDLE handle, IN OUT OPTIONAL char *kind, IN OUT size_t *kindSize);
TEESTATUS SendIOCTL(IN PTEEHANDLE handle, IN EVENTHANDLE evt, IN DWORD ioControlCode, IN LPVOID pInBuffer,
		    IN DWORD inBufferSize, IN LPVOID pOutBuffer, IN DWORD outBufferSize,
		    OUT LPDWORD pBytesRetuned);
TEESTATUS Win32ErrorToTee(IN DWORD win32Error);

static inline struct METEE_WIN_IMPL *to_int(PTEEHANDLE _h)
{
        return _h ? (struct METEE_WIN_IMPL *)_h->handle : NULL;
}

#endif /* __TEELIBWIN_H */
