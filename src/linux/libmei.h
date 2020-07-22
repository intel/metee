/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2013 - 2020 Intel Corporation. All rights reserved.
 *
 * Intel Management Engine Interface (Intel MEI) Library
 */
/*! \file libmei.h
 *  \brief mei library API
 */
#ifndef __LIBMEI_H__
#define __LIBMEI_H__

#include <linux/uuid.h>
#include <linux/mei.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif /*  __cplusplus */

/*! Library API version encode helper
 */
#define MEI_ENCODE_VERSION(major, minor)   ((major) << 16 | (minor) << 8)

/*! Library API version
 */
#define LIBMEI_API_VERSION MEI_ENCODE_VERSION(1, 4)

/*! Get current supported library API version
 *
 * \return version value
 */
unsigned int mei_get_api_version(void);

/*! ME client connection state
 */
enum mei_cl_state {
	MEI_CL_STATE_ZERO = 0,          /**< reserved */
	MEI_CL_STATE_INTIALIZED = 1,    /**< client is initialized (typo) */
	MEI_CL_STATE_INITIALIZED = 1,   /**< client is initialized */
	MEI_CL_STATE_CONNECTED,         /**< client is connected */
	MEI_CL_STATE_DISCONNECTED,      /**< client is disconnected */
	MEI_CL_STATE_NOT_PRESENT,       /**< client with GUID is not present in the system */
	MEI_CL_STATE_VERSION_MISMATCH,  /**< client version not supported */
	MEI_CL_STATE_ERROR,             /**< client is in error state */
};

/*! Structure to store connection data
 */
struct mei {
	uuid_le guid;           /**< client UUID */
	unsigned int buf_size;  /**< maximum buffer size supported by client*/
	unsigned char prot_ver; /**< protocol version */
	int fd;                 /**< connection file descriptor */
	int state;              /**< client connection state */
	int last_err;           /**< saved errno */
	bool notify_en;         /**< notification is enabled */
	bool verbose;           /**< verbose execution */
	bool close_on_exit;     /**< close handle on deinit */
	char *device;           /**< device name */
};

/*! Default name of mei device
 */
#define MEI_DEFAULT_DEVICE_NAME "mei0"

/*! Default path to mei device
 */
#define MEI_DEFAULT_DEVICE_PREFIX "/dev/"
#define MEI_DEFAULT_DEVICE (MEI_DEFAULT_DEVICE_PREFIX MEI_DEFAULT_DEVICE_NAME)

/*! Allocate and initialize me handle structure
 *
 *  \param device device path, set MEI_DEFAULT_DEVICE to use default
 *  \param guid UUID/GUID of associated mei client
 *  \param req_protocol_version minimal required protocol version, 0 for any
 *  \param verbose print verbose output to console
 *  \return me handle to the mei device. All subsequent calls to the lib's functions
 *         must be with this handle. NULL on failure.
 */
struct mei *mei_alloc(const char *device, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose);

/*! Allocate and initialize me handle structure
 *
 *  \param fd open file descriptor of MEI device
 *  \param guid UUID/GUID of associated mei client
 *  \param req_protocol_version minimal required protocol version, 0 for any
 *  \param verbose print verbose output to console
 *  \return me handle to the mei device. All subsequent calls to the lib's functions
 *         must be with this handle. NULL on failure.
 */
struct mei *mei_alloc_fd(int fd, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose);

/*! Free me handle structure
 *
 *  \param me The mei handle
 */
void mei_free(struct mei *me);

/*! Initializes a mei connection
 *
 *  \param me A handle to the mei device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param device device path, set MEI_DEFAULT_DEVICE to use default
 *  \param guid UUID/GUID of associated mei client
 *  \param req_protocol_version minimal required protocol version, 0 for any
 *  \param verbose print verbose output to a console
 *  \return 0 if successful, otherwise error code
 */
int mei_init(struct mei *me, const char *device, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose);

/*! Initializes a mei connection
 *
 *  \param me A handle to the mei device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param fd open file descriptor of MEI device
 *  \param guid UUID/GUID of associated mei client
 *  \param req_protocol_version minimal required protocol version, 0 for any
 *  \param verbose print verbose output to a console
 *  \return 0 if successful, otherwise error code
 */
int mei_init_fd(struct mei *me, int fd, const uuid_le *guid,
		unsigned char req_protocol_version, bool verbose);


/*! Closes the session to me driver
 *  Make sure that you call this function as soon as you are done with the device,
 *  as other clients might be blocked until the session is closed.
 *
 *  \param me The mei handle
 */
void mei_deinit(struct mei *me);

/*! Open mei device and starts a session with an mei client
 *  If the application requested specific minimal protocol version
 *  and client doesn't support that version
 *  the handle state will be set to MEI_CL_STATE_VERSION_MISMATCH
 *  but connection will be established
 *
 *  \param me The mei handle
 *  \return 0 if successful, otherwise error code
 */
int mei_connect(struct mei *me);


/*! Setup mei connection to non block
 *
 *  \param me The mei handle
 *  \return 0 if successful, otherwise error code
 */
int mei_set_nonblock(struct mei *me);

/*! return file descriptor to opened handle
 *
 *  \param me The mei handle
 *  \return file descriptor or error
 */
int mei_get_fd(struct mei *me);

/*! Read data from the mei device.
 *
 *  \param me The mei handle
 *  \param buffer A pointer to a buffer that receives the data read from the mei device.
 *  \param len The number of bytes to be read.
 *  \return number of byte read if successful, otherwise error code
 */
ssize_t mei_recv_msg(struct mei *me, unsigned char *buffer, size_t len);

/*! Writes the specified buffer to the mei device.
 *
 *  \param me The mei handle
 *  \param buffer A pointer to the buffer containing the data to be written to the mei device.
 *  \param len The number of bytes to be written.
 *  \return number of bytes written if successful, otherwise error code
 */
ssize_t mei_send_msg(struct mei *me, const unsigned char *buffer, size_t len);

/*! Request to Enable or Disable Event Notification
 *
 *  \param me The mei handle
 *  \param enable A boolean to enable or disable event notification
 *  \return 0 if successful, otherwise error code
 */
int mei_notification_request(struct mei *me, bool enable);

/*! Acknowledge an event and enable further notification
 *  notification events are signaled as priority events (POLLPRI) on select/poll
 *
 *  \param me The mei handle
 *  \return 0 if successful, otherwise error code. -ENOTSUPP is returned
 *  in case the event notification was not enabled
 */
int mei_notification_get(struct mei *me);

/*! Obtains FW status of device
 *
 *  \param me The mei handle
 *  \param fwsts_num The FW status register number (0-5).
 *  \param fwsts FW status to fill
 *  \return 0 if successful, otherwise error code
 */
int mei_fwstatus(struct mei *me, uint32_t fwsts_num, uint32_t *fwsts);


#ifdef __cplusplus
}
#endif /*  __cplusplus */

#endif /* __LIBMEI_H__ */
