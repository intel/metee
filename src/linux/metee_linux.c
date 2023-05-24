/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2022 Intel Corporation
 */
#include <errno.h>
#include <fcntl.h>
#include <libmei.h>
#include <linux/mei.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

#include "metee.h"
#include "helpers.h"

#define MAX_FW_STATUS_NUM 5

#define MILISEC_IN_SEC 1000

/* use inline function instead of macro to avoid -Waddress warning in GCC */
static inline struct mei *to_mei(PTEEHANDLE _h) __attribute__((always_inline));
static inline struct mei *to_mei(PTEEHANDLE _h)
{
	return _h ? (struct mei *)_h->handle : NULL;
}

static inline int __mei_select(struct mei *me, bool on_read, unsigned long timeout)
{
	int rv;
	struct pollfd pfd;
	pfd.fd = me->fd;
	pfd.events = (on_read) ? POLLIN : POLLOUT;

	errno = 0;
	rv = poll(&pfd, 1, (int)(timeout * MILISEC_IN_SEC));
	if (rv < 0)
		return -errno;
	if (rv == 0)
		return -ETIME;
	return 0;
}

static inline TEESTATUS errno2status(int err)
{
	switch (err) {
		case 0      : return TEE_SUCCESS;
		case -ENOTTY: return TEE_CLIENT_NOT_FOUND;
		case -EBUSY : return TEE_BUSY;
		case -ENODEV: return TEE_DISCONNECTED;
		case -ETIME : return TEE_TIMEOUT;
		case -EACCES: return TEE_PERMISSION_DENIED;
		default     : return TEE_INTERNAL_ERROR;
	}
}

static inline TEESTATUS errno2status_init(int err)
{
	switch (err) {
		case 0      : return TEE_SUCCESS;
		case -ENOENT: return TEE_DEVICE_NOT_FOUND;
		case -ENAMETOOLONG: return TEE_DEVICE_NOT_FOUND;
		case -EBUSY : return TEE_BUSY;
		case -ENODEV: return TEE_DEVICE_NOT_READY;
		case -ETIME : return TEE_TIMEOUT;
		case -EACCES: return TEE_PERMISSION_DENIED;
		default     : return TEE_INTERNAL_ERROR;
	}
}

TEESTATUS TEEAPI TeeInit(IN OUT PTEEHANDLE handle, IN const GUID *guid, IN OPTIONAL const char *device)
{
	struct mei *me;
	TEESTATUS  status;
	int rc;
#if defined(DEBUG) && !defined(SYSLOG)
	bool verbose = true;
#else
	bool verbose = false;
#endif // DEBUG and !SYSLOG

	if (guid == NULL || handle == NULL) {
		return TEE_INVALID_PARAMETER;
	}

	__tee_init_handle(handle);
	me = malloc(sizeof(struct mei));
	if (!me) {
		ERRPRINT(handle, "Cannot alloc mei structure\n");
		status = TEE_INTERNAL_ERROR;
		goto End;
	}
	rc = mei_init(me, device ? device : MEI_DEFAULT_DEVICE, guid, 0, verbose);
	if (rc) {
		free(me);
		ERRPRINT(handle, "Cannot init mei, rc = %d\n", rc);
		status = errno2status_init(rc);
		goto End;
	}
	handle->handle = me;
	status = TEE_SUCCESS;

End:
	return status;
}

TEESTATUS TEEAPI TeeInitHandle(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			       IN const TEE_DEVICE_HANDLE device_handle)
{
	struct mei *me;
	TEESTATUS  status;
	int rc;
#if defined(DEBUG) && !defined(SYSLOG)
	bool verbose = true;
#else
	bool verbose = false;
#endif // DEBUG and !SYSLOG

	if (guid == NULL || handle == NULL) {
		return TEE_INVALID_PARAMETER;
	}

	__tee_init_handle(handle);
	me = malloc(sizeof(struct mei));
	if (!me) {
		ERRPRINT(handle, "Cannot alloc mei structure\n");
		status = TEE_INTERNAL_ERROR;
		goto End;
	}
	rc = mei_init_fd(me, device_handle, guid, 0, verbose);
	if (rc) {
		free(me);
		ERRPRINT(handle, "Cannot init mei, rc = %d\n", rc);
		status = errno2status_init(rc);
		goto End;
	}
	handle->handle = me;
	status = TEE_SUCCESS;

End:
	return status;
}

TEESTATUS TEEAPI TeeConnect(IN OUT PTEEHANDLE handle)
{
	struct mei *me = to_mei(handle);
	TEESTATUS  status;
	int        rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me) {
		ERRPRINT(handle, "One of the parameters was illegal");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	rc = mei_connect(me);
	if (rc) {
		ERRPRINT(handle, "Cannot establish a handle to the Intel MEI driver\n");
		status = errno2status(rc);
		goto End;
	}

	handle->maxMsgLen = me->buf_size;
	handle->protcolVer = me->prot_ver;

	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);
	return status;
}

TEESTATUS TEEAPI TeeRead(IN PTEEHANDLE handle, IN OUT void *buffer, IN size_t bufferSize,
			 OUT OPTIONAL size_t *pNumOfBytesRead, IN OPTIONAL uint32_t timeout)
{
	struct mei *me = to_mei(handle);
	TEESTATUS status;
	ssize_t rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !buffer || !bufferSize) {
		ERRPRINT(handle, "One of the parameters was illegal");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (me->state != MEI_CL_STATE_CONNECTED) {
		ERRPRINT(handle, "The client is not connected\n");
		status = TEE_DISCONNECTED;
		goto End;
	}

	DBGPRINT(handle, "call read length = %zd\n", bufferSize);

	if (timeout && (rc = __mei_select(me, true, timeout))) {
		status = errno2status(rc);
		ERRPRINT(handle, "select failed with status %zd %s\n",
				rc, strerror(-rc));
		goto End;
	}

	rc = mei_recv_msg(me, buffer, bufferSize);
	if (rc < 0) {
		status = errno2status(rc);
		ERRPRINT(handle, "read failed with status %zd %s\n",
				rc, strerror(-rc));
		goto End;
	}

	status = TEE_SUCCESS;
	DBGPRINT(handle, "read succeeded with result %zd\n", rc);
	if (pNumOfBytesRead)
		*pNumOfBytesRead = rc;

End:
	FUNC_EXIT(handle, status);
	return status;
}

TEESTATUS TEEAPI TeeWrite(IN PTEEHANDLE handle, IN const void *buffer, IN size_t bufferSize,
			  OUT OPTIONAL size_t *numberOfBytesWritten, IN OPTIONAL uint32_t timeout)
{
	struct mei *me  =  to_mei(handle);
	TEESTATUS status;
	ssize_t rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !buffer || !bufferSize) {
		ERRPRINT(handle, "One of the parameters was illegal");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (me->state != MEI_CL_STATE_CONNECTED) {
		ERRPRINT(handle, "The client is not connected\n");
		status = TEE_DISCONNECTED;
		goto End;
	}

	DBGPRINT(handle, "call write length = %zd\n", bufferSize);

	if (timeout && (rc = __mei_select(me, false, timeout))) {
		status = errno2status(rc);
		ERRPRINT(handle, "select failed with status %zd %s\n",
				rc, strerror(-rc));
		goto End;
	}

	rc  = mei_send_msg(me, buffer, bufferSize);
	if (rc < 0) {
		status = errno2status(rc);
		ERRPRINT(handle, "write failed with status %zd %s\n", rc, strerror(-rc));
		goto End;
	}

	if (numberOfBytesWritten)
		*numberOfBytesWritten = rc;

	status = TEE_SUCCESS;
End:
	FUNC_EXIT(handle, status);
	return status;
}

TEESTATUS TEEAPI TeeFWStatus(IN PTEEHANDLE handle,
			     IN uint32_t fwStatusNum, OUT uint32_t *fwStatus)
{
	struct mei *me = to_mei(handle);
	TEESTATUS status;
        uint32_t fwsts;
	int rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !fwStatus) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal");
		goto End;
	}
	if (fwStatusNum > MAX_FW_STATUS_NUM) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "fwStatusNum should be 0..5");
		goto End;
	}

	rc  = mei_fwstatus(me, fwStatusNum, &fwsts);
	if (rc < 0) {
		status = errno2status(rc);
		ERRPRINT(handle, "fw status failed with status %d %s\n", rc, strerror(-rc));
		goto End;
	}

	*fwStatus = fwsts;
	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);
	return status;
}

void TEEAPI TeeDisconnect(PTEEHANDLE handle)
{
	struct mei *me  =  to_mei(handle);

	if (!handle) {
		return;
	}

	FUNC_ENTRY(handle);
	if (me) {
		mei_free(me);
		handle->handle = NULL;
	}

	FUNC_EXIT(handle, TEE_SUCCESS);
}

TEE_DEVICE_HANDLE TEEAPI TeeGetDeviceHandle(IN PTEEHANDLE handle)
{
	struct mei *me = to_mei(handle);

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	if (!me) {
		ERRPRINT(handle, "One of the parameters was illegal");
		return TEE_INVALID_DEVICE_HANDLE;
	}
	
	return me->fd;
}

TEESTATUS TEEAPI GetDriverVersion(IN PTEEHANDLE handle, IN OUT teeDriverVersion_t *driverVersion)
{
	struct mei *me = to_mei(handle);
	TEESTATUS status;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !driverVersion) {
		ERRPRINT(handle, "One of the parameters was illegal");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}
	
	status = TEE_NOTSUPPORTED;
End:
	FUNC_EXIT(handle, status);
	return status;
}

uint32_t TEEAPI TeeSetLogLevel(IN PTEEHANDLE handle, IN uint32_t log_level)
{
	struct mei *me = to_mei(handle);
	uint32_t prev_log_level = TEE_LOG_LEVEL_ERROR;

	if (!handle) {
		return prev_log_level;
	}

	FUNC_ENTRY(handle);

	if (!me) {
		ERRPRINT(handle, "Illegal handle\n");
		goto End;
	}

	prev_log_level = handle->log_level;
	handle->log_level = (log_level > TEE_LOG_LEVEL_VERBOSE) ? TEE_LOG_LEVEL_VERBOSE : log_level;

	mei_set_log_level(me, handle->log_level);

End:
	FUNC_EXIT(handle, prev_log_level);
	return prev_log_level;
}

uint32_t TEEAPI TeeGetLogLevel(IN const PTEEHANDLE handle)
{
	uint32_t prev_log_level = TEE_LOG_LEVEL_ERROR;

	if (!handle) {
		return prev_log_level;
	}

	FUNC_ENTRY(handle);

	prev_log_level = handle->log_level;

	FUNC_EXIT(handle, prev_log_level);

	return prev_log_level;
}
