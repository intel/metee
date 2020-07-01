/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2020 Intel Corporation
 */
/*! \file metee.h
 *  \brief metee library API
 */
#ifndef __METEE_H
#define __METEE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
//! @cond suppress_warnings
#ifdef _WIN32
	#include <Windows.h>
	#include <initguid.h>

	#ifndef METEE_DLL
		#define METEE_DLL_API
	#else /* METEE_DLL */
		#ifdef METEE_DLL_EXPORT
			#define METEE_DLL_API __declspec(dllexport)
		#else
			#define METEE_DLL_API __declspec(dllimport)
		#endif /* METEE_DLL_EXPORT */
	#endif /* METEE_DLL */
	#define TEEAPI METEE_DLL_API __stdcall
	#define TEE_DEVICE_HANDLE HANDLE
	#define TEE_INVALID_DEVICE_HANDLE ((void*)0)
#else /* _WIN32 */
	#include <linux/uuid.h>

	#ifndef METEE_DLL
		#define METEE_DLL_API
	#else /*! METEE_DLL */
		#ifdef METEE_DLL_EXPORT
			#define METEE_DLL_API __attribute__((__visibility__("default")))
		#else
			#define METEE_DLL_API
		#endif /* METEE_DLL_EXPORT */
	#endif /* METEE_DLL */
	#define TEEAPI METEE_DLL_API
	#define GUID uuid_le
	#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		const uuid_le name = UUID_LE(l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
	#define TEE_DEVICE_HANDLE int
	#define TEE_INVALID_DEVICE_HANDLE (-1)
	#ifndef IN
		#define IN
	#endif
	#ifndef OUT
		#define OUT
	#endif
	#ifndef OPTIONAL
		#define OPTIONAL
	#endif
#endif /* _WIN32 */
//! @endcond

/*!
 * Structure to store connection data
 */
typedef struct _TEEHANDLE {

	void    *handle;          /**< Handle to the internal structure */
	size_t  maxMsgLen;        /**< FW Client Max Message Length */
	uint8_t protcolVer;       /**< FW Client Protocol FW */
} TEEHANDLE;

/*!
 * \var typedestruct _TEEHANDLE *PTEEHANDLE
 * \brief A type definition for pointer to TEEHANDLE
 */
typedef TEEHANDLE *PTEEHANDLE;



/** ZERO/NULL device handle */
#define TEEHANDLE_ZERO {0}

typedef uint16_t TEESTATUS; /**< return status for API functions */
/** METEE ERROR BASE */
#define TEE_ERROR_BASE                    0x0000U
/** METEE SUCCESS */
#define TEE_SUCCESS                       (TEE_ERROR_BASE + 0)
/** An internal error occurred in the library */
#define TEE_INTERNAL_ERROR                (TEE_ERROR_BASE + 1)
/** The device is not in the system or is not working */
#define TEE_DEVICE_NOT_FOUND              (TEE_ERROR_BASE + 2)
/** The device is not ready for the  operation */
#define TEE_DEVICE_NOT_READY              (TEE_ERROR_BASE + 3)
/** An invalid parameter was used in the call */
#define TEE_INVALID_PARAMETER             (TEE_ERROR_BASE + 4)
/** It is not possible to complete the operation */
#define TEE_UNABLE_TO_COMPLETE_OPERATION  (TEE_ERROR_BASE + 5)
/** The operation has timed out */
#define TEE_TIMEOUT                       (TEE_ERROR_BASE + 6)
/** The operation is not supported */
#define TEE_NOTSUPPORTED                  (TEE_ERROR_BASE + 7)
/** The ME client is not present in the firmware */
#define TEE_CLIENT_NOT_FOUND              (TEE_ERROR_BASE + 8)
/** The device is busy */
#define TEE_BUSY                          (TEE_ERROR_BASE + 9)
/** The ME client is not connected */
#define TEE_DISCONNECTED                  (TEE_ERROR_BASE + 10)
/** The buffer for read not big enough  */
#define TEE_INSUFFICIENT_BUFFER           (TEE_ERROR_BASE + 11)

/*! Macro for successful operation result check
 */
#define TEE_IS_SUCCESS(Status) (((TEESTATUS)(Status)) == TEE_SUCCESS)

/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device optional device path, set NULL to use default
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeInit(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			 IN OPTIONAL const char *device);

#ifdef _WIN32
/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device optional device class GUID, set NULL to use default
 *         - use the first device from that class
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeInitGUID(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			     IN OPTIONAL const GUID *device);
#endif /* _WIN32 */

/*! Initializes a TEE connection
 *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
 *         must be with this handle
 *  \param guid GUID of the FW client that want to start a session
 *  \param device_handle open file handle
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeInitHandle(IN OUT PTEEHANDLE handle, IN const GUID *guid,
			       IN const TEE_DEVICE_HANDLE device_handle);

/*! Connects to the TEE driver and starts a session
 *  \param handle A handle to the TEE device
 *  \return 0 if successful, otherwise error code
 */
TEESTATUS TEEAPI TeeConnect(OUT PTEEHANDLE handle);

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
			 OUT OPTIONAL size_t *pNumOfBytesRead, IN OPTIONAL uint32_t timeout);

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
			  OUT OPTIONAL size_t *numberOfBytesWritten, IN OPTIONAL uint32_t timeout);

/*! Retrieves specified FW status register.
 *  \param handle The handle of the session.
 *  \param fwStatusNum The FW status register number (0-5).
 *  \param fwStatus The memory to store obtained FW status.
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeFWStatus(IN PTEEHANDLE handle,
			     IN uint32_t fwStatusNum, OUT uint32_t *fwStatus);

/*! Closes the session to TEE driver
 *  Make sure that you call this function as soon as you are done with the device,
 *  as other clients might be blocked until the session is closed.
 *  \param handle The handle of the session to close.
 */
void TEEAPI TeeDisconnect(IN PTEEHANDLE handle);

/*! Returns handle of TEE device
 *  Obtains HECI device handle on Windows and mei device file descriptor on Linux
 *  \param handle The handle of the session.
 */
TEE_DEVICE_HANDLE TEEAPI TeeGetDeviceHandle(IN PTEEHANDLE handle);

/*! Structure to store version data
 */
typedef struct {
	uint16_t major;  /**< Major version number */
	uint16_t minor;  /**< Minor version number */
	uint16_t hotfix; /**< Hotfix version number */
	uint16_t build;  /**< Build version number */
} teeDriverVersion_t;

/*! Obtains version of the TEE device driver
 *  Not implemented on Linux
 *  \param handle The handle of the session.
 *  \param driverVersion Pointer to driver version struct
 */
TEESTATUS TEEAPI GetDriverVersion(IN PTEEHANDLE handle, IN OUT teeDriverVersion_t *driverVersion);

#ifdef __cplusplus
}
#endif

#endif /* __METEE_H */
