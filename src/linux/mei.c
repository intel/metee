/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2013 - 2019 Intel Corporation. All rights reserved.
 *
 * Intel Management Engine Interface (Intel MEI) Library
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/mei.h>

#include "libmei.h"

/*****************************************************************************
 * Intel Management Engine Interface
 *****************************************************************************/
#ifdef ANDROID
#define LOG_TAG "libmei"
#include <cutils/log.h>
#define mei_msg(_me, fmt, ARGS...) ALOGV_IF(_me->verbose, fmt, ##ARGS)
#define mei_err(_me, fmt, ARGS...) ALOGE(fmt, ##ARGS)
static inline void __dump_buffer(const char *buf)
{
	ALOGV("%s\n", buf);
}

#else /* ! ANDROID */
#define mei_msg(_me, fmt, ARGS...) do {         \
	if (_me->verbose)                       \
		fprintf(stderr, "me: " fmt, ##ARGS);	\
} while (0)

#define mei_err(_me, fmt, ARGS...) do {         \
	fprintf(stderr, "me: error: " fmt, ##ARGS); \
} while (0)
static inline void __dump_buffer(const char *buf)
{
	fprintf(stderr, "%s\n", buf);;
}
#endif /* ANDROID */

static void dump_hex_buffer(const unsigned char *buf, size_t len)
{
	const size_t pbufsz = 16 * 3;
	char pbuf[pbufsz];
	int j = 0;

	while (len-- > 0) {
		snprintf(&pbuf[j], pbufsz - j, "%02X ", *buf++);
		j += 3;
		if (j == 16 * 3) {
			__dump_buffer(pbuf);
			j = 0;
		}
	}
	if (j)
		__dump_buffer(pbuf);
}

static void mei_dump_hex_buffer(struct mei *me,
				const unsigned char *buf, size_t len)
{
	if (!me->verbose)
		return;

	dump_hex_buffer(buf, len);
}

void mei_deinit(struct mei *me)
{
	if (!me)
		return;

	if (me->fd != -1)
		close(me->fd);
	me->fd = -1;
	me->buf_size = 0;
	me->prot_ver = 0;
	me->state = MEI_CL_STATE_ZERO;
	me->last_err = 0;
}

static inline int __mei_errno_to_state(struct mei *me)
{
	switch(me->last_err) {
	case 0:         return me->state;
	case ENOTTY:    return MEI_CL_STATE_NOT_PRESENT;
	case EBUSY:     return MEI_CL_STATE_DISCONNECTED;
	case ENODEV:    return MEI_CL_STATE_DISCONNECTED;
	case EOPNOTSUPP: return me->state;
	default:        return MEI_CL_STATE_ERROR;
	}
}

int mei_get_fd(struct mei *me)
{
	if (!me)
		return -EINVAL;
	return me->fd;
}

static int __mei_set_nonblock(struct mei *me)
{
	int flags;
	int rc;
	flags = fcntl(me->fd, F_GETFL, 0);
	if (flags == -1) {
		me->last_err = errno;
		return -me->last_err;
	}
	rc = fcntl(me->fd, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		me->last_err = errno;
		return -me->last_err;
	}
	return 0;
}

static inline int __mei_open(struct mei *me, const char *devname)
{
	errno = 0;
	me->fd = open(devname, O_RDWR);
	me->last_err = errno;
	return me->fd == -1 ? -me->last_err : me->fd;
}

static inline int __mei_connect(struct mei *me, struct mei_connect_client_data *d)
{
	errno = 0;
	int rc = ioctl(me->fd, IOCTL_MEI_CONNECT_CLIENT, d);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline int __mei_notify_set(struct mei *me, uint32_t *enable)
{
	errno = 0;
	int rc = ioctl(me->fd, IOCTL_MEI_NOTIFY_SET, enable);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline int __mei_notify_get(struct mei *me)
{
	errno = 0;
	uint32_t notification;
	int rc = ioctl(me->fd, IOCTL_MEI_NOTIFY_GET, &notification);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline ssize_t __mei_read(struct mei *me, unsigned char *buf, size_t len)
{
	ssize_t rc;
	errno = 0;
	rc = read(me->fd, buf, len);
	me->last_err = errno;
	return rc <= 0 ? -me->last_err : rc;
}

static inline ssize_t __mei_write(struct mei *me, const unsigned char *buf, size_t len)
{
	ssize_t rc;
	errno = 0;
	rc = write(me->fd, buf, len);
	me->last_err = errno;
	return rc <= 0 ? -me->last_err : rc;
}

int mei_init(struct mei *me, const char *device, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose)
{
	int rc;

	if (!me || !device || !guid)
		return -EINVAL;

	/* if me is uninitialized it will close wrong file descriptor */
	me->fd = -1;
	mei_deinit(me);

	me->verbose = verbose;

	mei_msg(me, "API version %u.%u\n",
		mei_get_api_version() >> 16 & 0xFF,
		mei_get_api_version() >> 8 & 0xFF);

	rc = __mei_open(me, device);
	if (rc < 0) {
		mei_err(me, "Cannot establish a handle to the Intel MEI driver %.20s [%d]:%s\n",
			device, rc, strerror(-rc));
		return rc;
	}

	mei_msg(me, "Opened %.20s: fd = %d\n", device, me->fd);

	memcpy(&me->guid, guid, sizeof(*guid));
	me->prot_ver = req_protocol_version;

	me->state = MEI_CL_STATE_INTIALIZED;

	return 0;
}

struct mei *mei_alloc(const char *device, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose)
{
	struct mei *me;

	if (!device || !guid)
		return NULL;

	me = malloc(sizeof(struct mei));
	if (!me)
		return NULL;

	if (mei_init(me, device, guid, req_protocol_version, verbose)) {
		free(me);
		return NULL;
	}
	return me;
}

void mei_free(struct mei *me)
{
	if (!me)
		return;
	mei_deinit(me);
	free(me);
}

int mei_set_nonblock(struct mei *me)
{
	if (!me)
		return -EINVAL;
	return __mei_set_nonblock(me);
}

int mei_connect(struct mei *me)
{
	struct mei_client *cl;
	struct mei_connect_client_data data;
	int rc;

	if (!me)
		return -EINVAL;

	if (me->state != MEI_CL_STATE_INTIALIZED &&
	    me->state != MEI_CL_STATE_DISCONNECTED) {
		mei_err(me, "client state [%d]\n", me->state);
		return -EINVAL;
	}

	memset(&data, 0, sizeof(data));
	memcpy(&data.in_client_uuid, &me->guid, sizeof(me->guid));

	rc = __mei_connect(me, &data);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "Cannot connect to client [%d]:%s\n", rc, strerror(-rc));
		return rc;
	}

	cl = &data.out_client_properties;
	mei_msg(me, "max_message_length %d\n", cl->max_msg_length);
	mei_msg(me, "protocol_version %d\n", cl->protocol_version);

	if ((me->prot_ver > 0) && (cl->protocol_version < me->prot_ver)) {
		mei_err(me, "Intel MEI protocol version not supported\n");
		me->state =  MEI_CL_STATE_VERSION_MISMATCH;
		rc = -EINVAL;
	} else {
		me->buf_size = cl->max_msg_length;
		me->prot_ver = cl->protocol_version;
		me->state =  MEI_CL_STATE_CONNECTED;
	}

	return rc ;
}

ssize_t mei_recv_msg(struct mei *me, unsigned char *buffer, size_t len)
{
	ssize_t rc;

	if (!me || !buffer)
		return -EINVAL;

	mei_msg(me, "call read length = %zd\n", len);

	rc = __mei_read(me, buffer, len);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "read failed with status [%zd]:%s\n", rc, strerror(-rc));
		goto out;
	}
	mei_msg(me, "read succeeded with result %zd\n", rc);
	mei_dump_hex_buffer(me, buffer, rc);
out:
	return rc;
}

ssize_t mei_send_msg(struct mei *me, const unsigned char *buffer, size_t len)
{
	ssize_t rc;

	if (!me || !buffer)
		return -EINVAL;

	mei_msg(me, "call write length = %zd\n", len);
	mei_dump_hex_buffer(me, buffer, len);

	rc  = __mei_write(me, buffer, len);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "write failed with status [%zd]:%s\n",
			rc, strerror(-rc));
		return rc;
	}

	return rc;
}

int mei_notification_request(struct mei *me, bool enable)
{
	uint32_t _enable;
	int rc;

	if (!me)
		return -EINVAL;

	if (me->state != MEI_CL_STATE_CONNECTED) {
		mei_err(me, "client is not connected [%d]\n", me->state);
		return -EINVAL;
	}

	_enable = enable;
	rc = __mei_notify_set(me, &_enable);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "Cannot %s notification for client [%d]:%s\n",
			enable ? "enable" : "disable", rc, strerror(-rc));
		return rc;
	}

	me->notify_en = enable;

	return 0;
}

int mei_notification_get(struct mei *me)
{
	int rc;

	if (!me)
		return -EINVAL;

	if (me->state != MEI_CL_STATE_CONNECTED) {
		mei_err(me, "client is not connected [%d]\n", me->state);
		return -EINVAL;
	}
	if (!me->notify_en)
		return -ENOTSUP;

	rc = __mei_notify_get(me);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "Cannot get notification for client [%d]:%s\n",
			rc, strerror(-rc));
		return rc;
	}

	return 0;
}

unsigned int mei_get_api_version(void)
{
	return LIBMEI_API_VERSION;
}
