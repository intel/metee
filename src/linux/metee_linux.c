/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2024 Intel Corporation
 */
#include <errno.h>
#include <fcntl.h>
#include <libmei.h>
#include <limits.h>
#include <linux/mei.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>

#include "metee.h"
#include "helpers.h"

#define MAX_FW_STATUS_NUM 5
#define CANCEL_PIPES_NUM 2

struct metee_linux_intl {
	struct mei me;
	int cancel_pipe[CANCEL_PIPES_NUM];
};

/* use inline function instead of macro to avoid -Waddress warning in GCC */
static inline struct mei *to_mei(PTEEHANDLE _h) __attribute__((always_inline));
static inline struct mei *to_mei(PTEEHANDLE _h)
{
	return _h ? &((struct metee_linux_intl *)_h->handle)->me : NULL;
}

/* use inline function instead of macro to avoid -Waddress warning in GCC */
static inline struct metee_linux_intl *to_intl(PTEEHANDLE _h) __attribute__((always_inline));
static inline struct metee_linux_intl *to_intl(PTEEHANDLE _h)
{
	return _h ? (struct metee_linux_intl *)_h->handle : NULL;
}

static inline int __mei_select(struct mei *me, int pipe_fd,
			       bool on_read, int timeout)
{
	int rv;
	struct pollfd pfd[2];
	pfd[0].fd = me->fd;
	pfd[0].events = (on_read) ? POLLIN : POLLOUT;
	pfd[1].fd = pipe_fd;
	pfd[1].events = POLLIN;

	errno = 0;
	rv = poll(pfd, 2, timeout);
	if (rv < 0)
		return -errno;
	if (rv == 0)
		return -ETIME;
	if (pfd[1].revents != 0)
		return -ECANCELED;
	return 0;
}

static inline TEESTATUS errno2status(ssize_t err)
{
	switch (err) {
		case 0      : return TEE_SUCCESS;
		case -ENOTTY: return TEE_CLIENT_NOT_FOUND;
		case -EBUSY : return TEE_BUSY;
		case -ENODEV: return TEE_DISCONNECTED;
		case -ETIME : return TEE_TIMEOUT;
		case -EACCES: return TEE_PERMISSION_DENIED;
		case -EOPNOTSUPP: return TEE_NOTSUPPORTED;
		case -ECANCELED: return TEE_UNABLE_TO_COMPLETE_OPERATION;
		default     : return TEE_INTERNAL_ERROR;
	}
}

static inline TEESTATUS errno2status_init(ssize_t err)
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

TEESTATUS TEEAPI TeeInitFull(IN OUT PTEEHANDLE handle, IN const GUID* guid,
			     IN const struct tee_device_address device,
			     IN uint32_t log_level, IN OPTIONAL TeeLogCallback log_callback)
{
	struct metee_linux_intl *intl;
	TEESTATUS  status;
	int rc;
	bool verbose = (log_level == TEE_LOG_LEVEL_VERBOSE);

	if (guid == NULL || handle == NULL) {
		return TEE_INVALID_PARAMETER;
	}

	__tee_init_handle(handle);
	handle->log_level = (log_level >= TEE_LOG_LEVEL_MAX) ? TEE_LOG_LEVEL_VERBOSE : log_level;
	handle->log_callback = log_callback;

	FUNC_ENTRY(handle);

	if (log_level >= TEE_LOG_LEVEL_MAX) {
		ERRPRINT(handle, "LogLevel %u is too big.\n", log_level);
		status = TEE_INVALID_PARAMETER;
		goto End;
	}
	switch (device.type) {
	case TEE_DEVICE_TYPE_NONE:
		if (device.data.path != NULL) {
			ERRPRINT(handle, "Path is not NULL.\n");
			status = TEE_INVALID_PARAMETER;
			goto End;
		}
		break;
	case TEE_DEVICE_TYPE_PATH:
		if (device.data.path == NULL) {
			ERRPRINT(handle, "Path is NULL.\n");
			status = TEE_INVALID_PARAMETER;
			goto End;
		}
		break;
	case TEE_DEVICE_TYPE_HANDLE:
		if (device.data.handle == TEE_INVALID_DEVICE_HANDLE) {
			ERRPRINT(handle, "Handle is invalid.\n");
			status = TEE_INVALID_PARAMETER;
			goto End;
		}
		break;
	case TEE_DEVICE_TYPE_GUID:
	default:
		ERRPRINT(handle, "Wrong device type %u.\n", device.type);
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	intl = malloc(sizeof(struct metee_linux_intl));
	if (!intl) {
		ERRPRINT(handle, "Cannot alloc intl structure\n");
		status = TEE_INTERNAL_ERROR;
		goto End;
	}

	switch (device.type) {
	case TEE_DEVICE_TYPE_NONE:
		rc = mei_init_with_log(&intl->me, MEI_DEFAULT_DEVICE,
			(uuid_le*)guid, 0, verbose, log_callback);
		break;
	case TEE_DEVICE_TYPE_PATH:
		rc = mei_init_with_log(&intl->me, device.data.path,
			(uuid_le*)guid, 0, verbose, log_callback);
		break;
	case TEE_DEVICE_TYPE_HANDLE:
		rc = mei_init_fd(&intl->me, device.data.handle, (uuid_le*)guid, 0, verbose);
		if (!rc) {
			mei_set_log_callback(&intl->me, log_callback);
			mei_set_log_level(&intl->me, verbose);
		}
		break;
	default:
		rc = -EFAULT;
		break;
	}
	if (rc) {
		free(intl);
		ERRPRINT(handle, "Cannot init mei, rc = %d\n", rc);
		status = errno2status_init(rc);
		goto End;
	}
	rc = pipe(intl->cancel_pipe);
	if (rc) {
		mei_deinit(&intl->me);
		free(intl);
		ERRPRINT(handle, "Cannot init mei, rc = %d\n", rc);
		status = errno2status_init(rc);
		goto End;
	}
	handle->handle = intl;

	status = TEE_SUCCESS;

End:
	return status;
}

TEESTATUS TEEAPI TeeInit(IN OUT PTEEHANDLE handle, IN const GUID* guid,
	IN OPTIONAL const char* device)
{
	struct tee_device_address addr;

	addr.type = (device) ? TEE_DEVICE_TYPE_PATH : TEE_DEVICE_TYPE_NONE;
	addr.data.path = device;

	return TeeInitFull(handle, guid, addr, TEE_DEFAULT_LOG_LEVEL, NULL);
}

TEESTATUS TEEAPI TeeInitHandle(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			       IN const TEE_DEVICE_HANDLE device_handle)
{
	struct tee_device_address addr;

	addr.type = TEE_DEVICE_TYPE_HANDLE;
	addr.data.handle = device_handle;

	return TeeInitFull(handle, guid, addr, TEE_DEFAULT_LOG_LEVEL, NULL);
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
		ERRPRINT(handle, "One of the parameters was illegal\n");
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
	struct metee_linux_intl *intl = to_intl(handle);
	int ltimeout;
	TEESTATUS status;
	ssize_t rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !buffer || !bufferSize) {
		ERRPRINT(handle, "One of the parameters was illegal\n");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (timeout > INT_MAX) {
		ERRPRINT(handle, "Timeout is too big %u > %d \n", timeout, INT_MAX);
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (me->state != MEI_CL_STATE_CONNECTED) {
		ERRPRINT(handle, "The client is not connected\n");
		status = TEE_DISCONNECTED;
		goto End;
	}

	DBGPRINT(handle, "call read length = %zd\n", bufferSize);

	ltimeout = (timeout) ? (int)timeout : -1;

	rc = __mei_select(me, intl->cancel_pipe[1], true, ltimeout);
	if (rc) {
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
		*pNumOfBytesRead = (size_t)rc;

End:
	FUNC_EXIT(handle, status);
	return status;
}

TEESTATUS TEEAPI TeeWrite(IN PTEEHANDLE handle, IN const void *buffer, IN size_t bufferSize,
			  OUT OPTIONAL size_t *numberOfBytesWritten, IN OPTIONAL uint32_t timeout)
{
	struct mei *me  =  to_mei(handle);
	struct metee_linux_intl *intl = to_intl(handle);
	int ltimeout;
	TEESTATUS status;
	ssize_t rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !buffer || !bufferSize) {
		ERRPRINT(handle, "One of the parameters was illegal\n");
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (timeout > INT_MAX) {
		ERRPRINT(handle, "Timeout is too big %u > %d \n", timeout, INT_MAX);
		status = TEE_INVALID_PARAMETER;
		goto End;
	}

	if (me->state != MEI_CL_STATE_CONNECTED) {
		ERRPRINT(handle, "The client is not connected\n");
		status = TEE_DISCONNECTED;
		goto End;
	}

	DBGPRINT(handle, "call write length = %zd\n", bufferSize);

	ltimeout = (timeout) ? (int)timeout : -1;

	rc = __mei_select(me, intl->cancel_pipe[1], false, ltimeout);
	if (rc) {
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
		*numberOfBytesWritten = (size_t)rc;

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
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}
	if (fwStatusNum > MAX_FW_STATUS_NUM) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "fwStatusNum should be 0..5\n");
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

TEESTATUS TEEAPI TeeGetTRC(IN PTEEHANDLE handle, OUT uint32_t* trc_val)
{
	struct mei* me = to_mei(handle);
	TEESTATUS status;
	uint32_t trc;
	int rc;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me || !trc_val) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto End;
	}

	rc = mei_gettrc(me, &trc);
	if (rc < 0) {
		status = errno2status(rc);
		ERRPRINT(handle, "TRC get failed with status %d %s\n", rc, strerror(-rc));
		goto End;
	}

	*trc_val = trc;
	status = TEE_SUCCESS;

End:
	FUNC_EXIT(handle, status);
	return status;
}

void TEEAPI TeeDisconnect(PTEEHANDLE handle)
{
	struct metee_linux_intl *intl = to_intl(handle);
	const char buf[] = "X";

	if (!handle) {
		return;
	}

	FUNC_ENTRY(handle);
	if (intl) {
		if (write(intl->cancel_pipe[1], buf, sizeof(buf)) < 0) {
			ERRPRINT(handle, "Pipe write failed\n");
		}
		mei_deinit(&intl->me);
		close(intl->cancel_pipe[0]);
		close(intl->cancel_pipe[1]);
		free(intl);
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
		DBGPRINT(handle, "Internal structure is not initialized\n");
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
		ERRPRINT(handle, "One of the parameters was illegal\n");
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

TEESTATUS TEEAPI TeeSetLogCallback(IN const PTEEHANDLE handle, TeeLogCallback log_callback)
{
	struct mei *me = to_mei(handle);
	TEESTATUS status;

	if (!handle) {
		return TEE_INVALID_PARAMETER;
	}

	FUNC_ENTRY(handle);

	if (!me) {
		status = TEE_INVALID_PARAMETER;
		ERRPRINT(handle, "One of the parameters was illegal\n");
		goto Cleanup;
	}

	handle->log_callback = log_callback;
	mei_set_log_callback(me, log_callback);
	status = TEE_SUCCESS;

Cleanup:
	FUNC_EXIT(handle, status);
	return status;
}

uint32_t TEEAPI TeeGetMaxMsgLen(IN const PTEEHANDLE handle)
{
	if (!handle) {
		return 0;
	}
	return (uint32_t)handle->maxMsgLen;
}

uint8_t TEEAPI TeeGetProtocolVer(IN const PTEEHANDLE handle)
{
	if (!handle) {
		return 0;
	}
	return handle->protcolVer;
}