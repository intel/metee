/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2020 Intel Corporation
 */
#ifndef __TEELIBWIN_H
#define __TEELIBWIN_H

#include <Windows.h>
#include <stdbool.h>
#include "metee.h"
#if (_MSC_PLATFORM_TOOLSET < 140)
#ifndef _Out_writes_

#define _Out_writes_(x)

#endif
#endif
#define CANCEL_TIMEOUT 5000

enum METEE_CLIENT_STATE
{
	METEE_CLIENT_STATE_NONE,
	METEE_CLIENT_STATE_CONNECTED,
	METEE_CLIENT_STATE_FAILED
};

struct METEE_WIN_IMPL
{
	HANDLE handle;            /**< file descriptor - Handle to the Device File */
	GUID   guid;              /**< fw client guid */
	LPOVERLAPPED evt;         /**< event for executing async */
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
} TEE_OPERATION, *PTEE_OPERATION;

/*
 * This callback function is called when an asynchronous TEE operation is completed.

 * @param  status - The operation status. This parameter is 0 is the operation was successful.
 *                  Otherwise it returns a Win32 error value.
 * @param  numberOfBytesTransfered - The number of bytes transferred.
 *                                   If an error occurs, this parameter is zero.
*/
typedef
void
(*LPTEE_COMPLETION_ROUTINE)(
	IN    TEESTATUS status,
	IN    size_t numberOfBytesTransfered
	);

typedef struct _OPERATION_CONTEXT
{
	HANDLE                          handle;
	LPOVERLAPPED                    pOverlapped;
	LPTEE_COMPLETION_ROUTINE        completionRoutine;
} OPERATION_CONTEXT, *POPERATION_CONTEXT;


/*********************************************************************
**                Windows Helper Functions                          **
**********************************************************************/
TEESTATUS BeginOverlappedInternal(IN TEE_OPERATION operation,
				 IN HANDLE handle, IN PVOID buffer,
				 IN ULONG bufferSize, OUT PEVENTHANDLE evt);
TEESTATUS EndOverlapped(IN HANDLE handle, IN EVENTHANDLE evt, IN DWORD milliseconds,
			OUT OPTIONAL LPDWORD pNumberOfBytesTransferred);
DWORD WINAPI WaitForOperationEnd(LPVOID lpThreadParameter);
TEESTATUS EndReadInternal(IN HANDLE handle, IN EVENTHANDLE evt, DWORD milliseconds,
			  OUT OPTIONAL LPDWORD pNumberOfBytesRead);
TEESTATUS BeginReadInternal(IN HANDLE handle, IN PVOID buffer, IN ULONG bufferSize,
			    OUT PEVENTHANDLE evt);
TEESTATUS BeginWriteInternal(IN HANDLE handle,
			     IN const PVOID buffer, IN ULONG bufferSize, OUT PEVENTHANDLE evt);
TEESTATUS EndWriteInternal(IN HANDLE handle, IN EVENTHANDLE evt, DWORD milliseconds,
			   OUT OPTIONAL LPDWORD pNumberOfBytesWritten);
TEESTATUS BeginOverlapped(IN TEE_OPERATION operation, IN PTEEHANDLE handle,
			  IN PVOID buffer, IN ULONG bufferSize,
			  IN LPTEE_COMPLETION_ROUTINE completionRoutine);
TEESTATUS GetDevicePath(_In_ LPCGUID InterfaceGuid,
			_Out_writes_(pathSize) char *path, _In_ SIZE_T pathSize);
TEESTATUS SendIOCTL(IN HANDLE handle, IN DWORD ioControlCode, IN LPVOID pInBuffer,
		    IN DWORD inBufferSize, IN LPVOID pOutBuffer, IN DWORD outBufferSize,
		    OUT LPDWORD pBytesRetuned);
TEESTATUS Win32ErrorToTee(_In_ DWORD win32Error);

#endif /* __TEELIBWIN_H */
