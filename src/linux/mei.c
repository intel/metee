/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2013 - 2024 Intel Corporation. All rights reserved.
 *
 * Intel Management Engine Interface (Intel MEI) Library
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/mei.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "libmei.h"

/*****************************************************************************
 * Intel Management Engine Interface
 *****************************************************************************/
#ifdef ANDROID
#define LOG_TAG "libmei"
#include <cutils/log.h>
#define mei_msg(_me, fmt, ARGS...) ALOGV_IF((_me->log_level >= MEI_LOG_LEVEL_VERBOSE), fmt, ##ARGS)
#define mei_err(_me, fmt, ARGS...) ALOGE(fmt, ##ARGS)
#ifdef DEBUG
static inline void __dump_buffer(const char *buf)
{
	ALOGV("%s\n", buf);
}
#endif /* DEBUG */

#else /* ! ANDROID */
#ifdef SYSLOG
	#include <syslog.h>
	#define __mei_msg(fmt, ...) syslog(LOG_DEBUG, fmt, ##__VA_ARGS__)
	#define __mei_err(fmt, ...) syslog(LOG_ERR, fmt, ##__VA_ARGS__)
#else
	#include <stdlib.h>
	#define __mei_msg(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
	#define __mei_err(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#endif /* SYSLOG */

#define mei_msg(_me, fmt, ARGS...) do {                          \
	if ((_me)->log_level >= MEI_LOG_LEVEL_VERBOSE) {         \
		if ((_me)->log_callback)                         \
			(_me)->log_callback(false, fmt, ##ARGS); \
		else                                             \
			__mei_msg(fmt, ##ARGS);                  \
	}                                                        \
} while (0)

#define mei_err(_me, fmt, ARGS...) do {                                       \
	if ((_me)->log_level > MEI_LOG_LEVEL_QUIET) {                         \
		if ((_me)->log_callback)                                      \
			(_me)->log_callback(true, "me: error: " fmt, ##ARGS); \
		else                                                          \
			__mei_err("me: error: " fmt, ##ARGS);                 \
	}                                                                     \
} while (0)

#ifdef DEBUG
static inline void __dump_buffer(const char *buf)
{
	__mei_msg("%s\n", buf);
}
#endif /* DEBUG */
#endif /* ANDROID */

#ifdef DEBUG
static void dump_hex_buffer(const unsigned char *buf, size_t len)
{
#define LINE_LEN 16
#define PBUFSZ (sizeof("00") * LINE_LEN)
	char pbuf[PBUFSZ];

	pbuf[0]	= '\0';

	for (size_t j = 0; (j < PBUFSZ) && (len > 0); len--, j += sizeof("00"))
		snprintf(&pbuf[j], PBUFSZ - j, "%02X ", *buf++);

	__dump_buffer(pbuf);
#undef PBUFSZ
#undef LINE_LEN
}

static void mei_dump_hex_buffer(struct mei *me,
				const unsigned char *buf, size_t len)
{
	if (me->log_level < MEI_LOG_LEVEL_VERBOSE)
		return;

	dump_hex_buffer(buf, len);
}
#else /* DEBUG */
static void mei_dump_hex_buffer(struct mei *me,
				const unsigned char *buf, size_t len)
{
	(void)(me);
	(void)(buf);
	(void)(len);
}
#endif /* DEBUG */

void mei_deinit(struct mei *me)
{
	if (!me)
		return;

	if (me->close_on_exit && me->fd != -1)
		close(me->fd);
	me->fd = -1;
	me->buf_size = 0;
	me->prot_ver = 0;
	me->state = MEI_CL_STATE_ZERO;
	me->last_err = 0;
	free(me->device);
	me->device = NULL;
}

static inline int __mei_errno_to_state(struct mei *me)
{
	switch (me->last_err) {
	case 0: return me->state;
	case ENOTTY: return MEI_CL_STATE_NOT_PRESENT;
	case EBUSY: /* fall through */
	case ENODEV: return MEI_CL_STATE_DISCONNECTED;
	case EOPNOTSUPP: return me->state;
	default: return MEI_CL_STATE_ERROR;
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

	errno = 0;
	flags = fcntl(me->fd, F_GETFL, 0);
	if (flags == -1) {
		me->last_err = errno;
		return -me->last_err;
	}
	errno = 0;
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
	me->fd = open(devname, O_RDWR | O_CLOEXEC);
	me->last_err = errno;
	return me->fd == -1 ? -me->last_err : me->fd;
}

static inline int __mei_connect(struct mei *me, struct mei_connect_client_data *d)
{
	int rc;

	errno = 0;
	rc = ioctl(me->fd, IOCTL_MEI_CONNECT_CLIENT, d);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline int __mei_connect_vtag(struct mei *me,
				     struct mei_connect_client_data_vtag *d)
{
	int rc;

	errno = 0;
	rc = ioctl(me->fd, IOCTL_MEI_CONNECT_CLIENT_VTAG, d);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline int __mei_notify_set(struct mei *me, uint32_t *enable)
{
	int rc;

	errno = 0;
	rc = ioctl(me->fd, IOCTL_MEI_NOTIFY_SET, enable);
	me->last_err = errno;
	return rc == -1 ? -me->last_err : 0;
}

static inline int __mei_notify_get(struct mei *me)
{
	uint32_t notification;
	int rc;

	errno = 0;
	rc = ioctl(me->fd, IOCTL_MEI_NOTIFY_GET, &notification);
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

static inline int __mei_fwsts(struct mei *me, const char *device,
			      uint32_t fwsts_num, uint32_t *fwsts)
{
#define FWSTS_FILENAME_LEN 33
#define FWSTS_LEN 9
#define CONV_BASE 16
	char path[FWSTS_FILENAME_LEN];
	int fd;
	char line[FWSTS_LEN];
	unsigned long cnv;
	ssize_t len;
	off_t count;

	if (snprintf(path, FWSTS_FILENAME_LEN,
		     "/sys/class/mei/%s/fw_status", device) < 0)
		return -EINVAL;
	path[FWSTS_FILENAME_LEN - 1] = '\0';

	errno = 0;
	fd = open(path, O_CLOEXEC, O_RDONLY);
	if (fd == -1) {
		me->last_err = errno;
		return -me->last_err;
	}

	/* safe to cast to off_t: fwsts_num is a small number */
	count = (off_t)fwsts_num * FWSTS_LEN;
	errno = 0;
	len = pread(fd, line, FWSTS_LEN, count);
	if (len == -1) {
		me->last_err = errno;
		close(fd);
		return -me->last_err;
	}

	close(fd);
	if (len < FWSTS_LEN) {
		me->last_err = EPROTO;
		return -me->last_err;
	}

	errno = 0;
	cnv = strtoul(line, NULL, CONV_BASE);
	if (errno) {
		me->last_err = errno;
		return -me->last_err;
	}
	*fwsts = cnv;

	return 0;
#undef FWSTS_FILENAME_LEN
#undef FWSTS_LEN
#undef CONV_BASE
}

static inline int __mei_gettrc(struct mei *me, const char *device, uint32_t *trc_val)
{
#define TRC_FILENAME_LEN 33
#define TRC_LEN 9
#define CONV_BASE 16
	char path[TRC_FILENAME_LEN];
	int fd;
	char line[TRC_LEN];
	unsigned long cnv;
	ssize_t len;

	if (snprintf(path, TRC_FILENAME_LEN,
		     "/sys/class/mei/%s/trc", device) < 0)
		return -EINVAL;
	path[TRC_FILENAME_LEN - 1] = '\0';

	errno = 0;
	fd = open(path, O_CLOEXEC, O_RDONLY);
	if (fd == -1) {
		me->last_err = errno;
		return -me->last_err;
	}

	errno = 0;
	len = pread(fd, line, TRC_LEN, 0);
	if (len == -1) {
		me->last_err = errno;
		close(fd);
		return -me->last_err;
	}

	close(fd);
	if (len < TRC_LEN) {
		me->last_err = EPROTO;
		return -me->last_err;
	}

	errno = 0;
	cnv = strtoul(line, NULL, CONV_BASE);
	if (errno) {
		me->last_err = errno;
		return -me->last_err;
	}
	*trc_val = cnv;

	return 0;
#undef TRC_FILENAME_LEN
#undef TRC_LEN
#undef CONV_BASE
}

int mei_init_with_log(struct mei *me, const char *device, const uuid_le *guid,
		      unsigned char req_protocol_version, bool verbose,
		      mei_log_callback log_callback)
{
	int rc;

	if (!me || !device || !guid)
		return -EINVAL;

	/* if me is uninitialized it will close wrong file descriptor */
	me->fd = -1;
	me->close_on_exit = true;
	me->device = NULL;
	me->log_callback = log_callback;
	mei_deinit(me);

	me->log_level = verbose ? MEI_LOG_LEVEL_VERBOSE : MEI_LOG_LEVEL_ERROR;

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
	me->device = strdup(device);
	if (!me->device) {
		mei_deinit(me);
		return -ENOMEM;
	}

	me->state = MEI_CL_STATE_INITIALIZED;

	return 0;
}

int mei_init(struct mei *me, const char *device, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose)
{
	return mei_init_with_log(me, device, guid, req_protocol_version, verbose, NULL);
}

static int __mei_fd_to_devname(struct mei *me, int fd)
{
	char name[PATH_MAX];
	char proc[PATH_MAX];
	int ret;
	ssize_t sret;

	ret = snprintf(proc, PATH_MAX, "/proc/self/fd/%d", fd);
	if (ret < 0) {
		mei_err(me, "Proc path is bad %d\n", ret);
		return ret;
	}
	if (ret >= PATH_MAX) {
		mei_err(me, "Proc path is too long %d\n", ret);
		return -ENAMETOOLONG;
	}

	errno = 0;
	sret = readlink(proc, name, PATH_MAX);
	if (sret == -1) {
		mei_err(me, "Cannot obtain device name %d\n", errno);
		return -errno;
	}
	if (sret == PATH_MAX) {
		mei_err(me, "Cannot obtain device name, too long\n");
		return -ENAMETOOLONG;
	}
	if (sret < 0 || sret > PATH_MAX) {
		mei_err(me, "Cannot obtain device name %zd\n", sret);
		return -EFAULT;
	}
	name[sret] = '\0';

	me->device = strdup(name);
	if (!me->device)
		return -ENOMEM;

	return 0;
}

int mei_init_fd(struct mei *me, int fd, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose)
{
	int ret;

	if (!me || fd < 0 || !guid)
		return -EINVAL;

	/* if me is uninitialized it will close wrong file descriptor */
	me->close_on_exit = false;
	me->device = NULL;
	mei_deinit(me);
	me->fd = fd;
	me->log_callback = NULL;

	me->log_level = verbose ? MEI_LOG_LEVEL_VERBOSE : MEI_LOG_LEVEL_ERROR;

	mei_msg(me, "API version %u.%u\n",
		mei_get_api_version() >> 16 & 0xFF,
		mei_get_api_version() >> 8 & 0xFF);

	memcpy(&me->guid, guid, sizeof(*guid));
	me->prot_ver = req_protocol_version;

	ret = __mei_fd_to_devname(me, fd);
	if (ret)
		return ret;

	me->state = MEI_CL_STATE_INITIALIZED;

	return ret;

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

struct mei *mei_alloc_fd(const int fd, const uuid_le *guid,
			 unsigned char req_protocol_version, bool verbose)
{
	struct mei *me = NULL;

	if (!guid || fd < 0)
		return NULL;

	me = malloc(sizeof(*me));
	if (!me)
		return NULL;

	if (mei_init_fd(me, fd, guid, req_protocol_version, verbose)) {
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

static int __int_mei_connect(struct mei *me, uint8_t vtag)
{
	struct mei_client *cl;
	struct mei_connect_client_data data;
	struct mei_connect_client_data_vtag data_v;
	int rc;

	if (!me)
		return -EINVAL;

	if (me->state == MEI_CL_STATE_CONNECTED) {
		mei_err(me, "client is connected [%d]\n", me->state);
		return -EINVAL;
	}

	me->vtag = vtag;
	if (me->vtag) {
		memset(&data_v, 0, sizeof(data_v));
		memcpy(&data_v.connect.in_client_uuid, &me->guid,
		       sizeof(me->guid));
		data_v.connect.vtag = me->vtag;

		rc = __mei_connect_vtag(me, &data_v);
		cl = &data_v.out_client_properties;
	} else {
		memset(&data, 0, sizeof(data));
		memcpy(&data.in_client_uuid, &me->guid, sizeof(me->guid));

		rc = __mei_connect(me, &data);
		cl = &data.out_client_properties;
	}
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "Cannot connect to client [%d]:%s\n", rc, strerror(-rc));
		return rc;
	}

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

	return rc;
}

int mei_connect(struct mei *me)
{
	return __int_mei_connect(me, 0);
}

int mei_connect_vtag(struct mei *me, uint8_t vtag)
{
	return __int_mei_connect(me, vtag);
}

ssize_t mei_recv_msg(struct mei *me, unsigned char *buffer, size_t len)
{
	ssize_t rc;

	if (!me || !buffer)
		return -EINVAL;

	mei_msg(me, "call read length = %zu\n", len);

	rc = __mei_read(me, buffer, len);
	if (rc < 0) {
		me->state = __mei_errno_to_state(me);
		mei_err(me, "read failed with status [%zd]:%s\n", rc, strerror(-rc));
		goto out;
	}
	mei_msg(me, "read succeeded with result %zd\n", rc);
	mei_dump_hex_buffer(me, buffer, (size_t)rc);
out:
	return rc;
}

ssize_t mei_send_msg(struct mei *me, const unsigned char *buffer, size_t len)
{
	ssize_t rc;

	if (!me || !buffer)
		return -EINVAL;

	mei_msg(me, "call write length = %zu\n", len);
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

#define MAX_FW_STATUS_NUM 5

int mei_fwstatus(struct mei *me, uint32_t fwsts_num, uint32_t *fwsts)
{
	char *device;
	int rc;

	if (!me || !fwsts)
		return -EINVAL;

	if (fwsts_num > MAX_FW_STATUS_NUM) {
		mei_err(me, "FW status number should be 0..5\n");
		return -EINVAL;
	}

	if (me->device) {
		device = strstr(me->device, MEI_DEFAULT_DEVICE_PREFIX);
		if (!device) {
			mei_err(me, "Device does not start with '%s'\n",
				MEI_DEFAULT_DEVICE_PREFIX);
			return -EINVAL;
		}
		device += strlen(MEI_DEFAULT_DEVICE_PREFIX);
	} else {
		device = MEI_DEFAULT_DEVICE_NAME;
	}
	rc = __mei_fwsts(me, device, fwsts_num, fwsts);
	if (rc < 0) {
		mei_err(me, "Cannot get FW status [%d]:%s\n",
			rc, strerror(-rc));
		return rc;
	}

	return 0;
}

int mei_gettrc(struct mei *me, uint32_t *trc_val)
{
	char *device;
	int rc;

	if (!me || !trc_val)
		return -EINVAL;

	if (me->device) {
		device = strstr(me->device, MEI_DEFAULT_DEVICE_PREFIX);
		if (!device) {
			mei_err(me, "Device does not start with '%s'\n",
				MEI_DEFAULT_DEVICE_PREFIX);
			return -EINVAL;
		}
		device += strlen(MEI_DEFAULT_DEVICE_PREFIX);
	} else {
		device = MEI_DEFAULT_DEVICE_NAME;
	}
	rc = __mei_gettrc(me, device, trc_val);
	if (rc < 0) {
		mei_err(me, "Cannot get TRC value [%d]:%s\n",
			rc, strerror(-rc));
		return rc;
	}

	return 0;
}

unsigned int mei_get_api_version(void)
{
	return LIBMEI_API_VERSION;
}

uint32_t mei_set_log_level(struct mei *me, uint32_t log_level)
{
	uint32_t prev_log_level;

	if (!me)
		return MEI_LOG_LEVEL_ERROR;

	prev_log_level = log_level;
	me->log_level = (log_level > MEI_LOG_LEVEL_VERBOSE) ? MEI_LOG_LEVEL_VERBOSE : log_level;

	return prev_log_level;
}

uint32_t mei_get_log_level(const struct mei *me)
{
	if (!me)
		return MEI_LOG_LEVEL_ERROR;

	return me->log_level;
}

int mei_set_log_callback(struct mei *me, mei_log_callback log_callback)
{
	if (!me)
		return -EINVAL;

	me->log_callback = log_callback;
	mei_msg(me, "New log callback set\n");

	return 0;
}
