/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2024 Intel Corporation
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
#include <stdbool.h>
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

#elif defined(EFI)
	#include <Uefi.h>
	#define TEEAPI 
	#define TEE_DEVICE_HANDLE void *
	#define TEE_INVALID_DEVICE_HANDLE ((void*)-1)	

	// when calling TeeInitFull
	// TEE_DEVICE_TYPE_BDF - HECI device Bus Device Function
	
#else /* _WIN32 */
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

	#ifndef GUID_DEFINED
		#define GUID_DEFINED 1
		typedef struct _GUID {
			uint32_t l;
			uint16_t w1;
			uint16_t w2;
			uint8_t  b[8];
		} GUID;
		#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
			const GUID name \
				= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
	#endif /* GUID_DEFINED */

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

/*! log level
 */
enum tee_log_level {
	TEE_LOG_LEVEL_QUIET = 0,   /**< no log prints */
	TEE_LOG_LEVEL_ERROR = 1,   /**< error log prints */
	TEE_LOG_LEVEL_VERBOSE = 2, /**< verbose log prints */
	TEE_LOG_LEVEL_MAX = 3,     /**< upper sentinel */
};

/*! log callback function format
 */
typedef void(*TeeLogCallback)(bool is_error, const char* fmt, ...);

/*!
 * Structure to store connection data
 */
typedef struct _TEEHANDLE {

	void    *handle;          /**< Handle to the internal structure */
	size_t  maxMsgLen;        /**< FW Client Max Message Length */
	uint8_t protcolVer;       /**< FW Client Protocol FW */
	enum tee_log_level log_level; /**< Log level */
	TeeLogCallback log_callback; /**< Log callback */
} TEEHANDLE;

/*!
 * \var typedestruct _TEEHANDLE *PTEEHANDLE
 * \brief A type definition for pointer to TEEHANDLE
 */
typedef TEEHANDLE *PTEEHANDLE;

/*! Device address passed to the init function
 */
struct tee_device_address {
	/*! Device address type
	 */
	enum {
		TEE_DEVICE_TYPE_NONE = 0, /**< Select first available device */
		TEE_DEVICE_TYPE_PATH = 1, /**< Use device by path (char*) */
		TEE_DEVICE_TYPE_HANDLE = 2, /**< Use device by pre-opend handle */
		TEE_DEVICE_TYPE_GUID = 3, /**< Select first device by GUID (Windows only) */
		TEE_DEVICE_TYPE_BDF = 4, /**< Use BDF to work with HECI, EFI only */
		TEE_DEVICE_TYPE_MAX = 5, /**< upper sentinel */
	} type;

	/*! Device address */
	union {
		const char* path; /** < Path to device */
		const GUID* guid; /** Device GUID (Windows only) */
		TEE_DEVICE_HANDLE handle; /**< Pre-opend handle */
		struct {
			struct {
				uint32_t segment;                     /** HECI device Segment */
				uint32_t bus;                         /** HECI device Bus */
				uint32_t device;                      /** HECI device Device */
				uint32_t function;                    /** HECI device Function */
			} value;
			/*! Specify HW layout: HECI, FWSTS registers locations  */			
			enum  { 
				HECI_DEVICE_KIND_PCH, 
				HECI_DEVICE_KIND_GFX_GSC, 
				HECI_DEVICE_KIND_GFX_CSC, 
			} kind;									/** HECI device kind */
		} bdf;
	} data;
};

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
/** The user don't have permission for this operation  */
#define TEE_PERMISSION_DENIED             (TEE_ERROR_BASE + 12)

/*! Macro for successful operation result check
 */
#define TEE_IS_SUCCESS(Status) (((TEESTATUS)(Status)) == TEE_SUCCESS)

 /*! Initializes a TEE connection
  *  \param handle A handle to the TEE device. All subsequent calls to the lib's functions
  *         must be with this handle
  *  \param guid GUID of the FW client that want to start a session
  *  \param device device address structure
  *  \param log_level log level to set (from enum tee_log_level)
  *  \param log_callback pointer to function to run for log write, set NULL to use built-in function
  *  \return 0 if successful, otherwise error code
  */
TEESTATUS TEEAPI TeeInitFull(IN OUT PTEEHANDLE handle, IN const GUID* guid,
	IN const struct tee_device_address device,
	IN uint32_t log_level, IN OPTIONAL TeeLogCallback log_callback);

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

/*! Retrieves TRC register.
 *  \param handle The handle of the session.
 *  \param trc_val The memory to store obtained TRC value.
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeGetTRC(IN PTEEHANDLE handle, OUT uint32_t* trc_val);

/*! Closes the session to TEE driver
 *  Make sure that you call this function as soon as you are done with the device,
 *  as other clients might be blocked until the session is closed.
 *  \param handle The handle of the session to close.
 */
void TEEAPI TeeDisconnect(IN PTEEHANDLE handle);

/*! Returns handle of TEE device
 *  Obtains HECI device handle on Windows and mei device file descriptor on Linux
 *  \param handle The handle of the session.
 *  \return device handle
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
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI GetDriverVersion(IN PTEEHANDLE handle, IN OUT teeDriverVersion_t *driverVersion);

/*! Set log level
 *
 *  \param handle The handle of the session.
 *  \param log_level log level to set
 *  \return previous log level
 */
uint32_t TEEAPI TeeSetLogLevel(IN PTEEHANDLE handle, IN uint32_t log_level);

/*! Retrieve current log level
 *
 *  \param handle The handle of the session.
 *  \return current log level
 */
uint32_t TEEAPI TeeGetLogLevel(IN const PTEEHANDLE handle);

/*! Set log callback
 *
 *  \param handle The handle of the session.
 *  \param log_callback pointer to function to run for log write, set NULL to use built-in function
 *  \return 0 if successful, otherwise error code.
 */
TEESTATUS TEEAPI TeeSetLogCallback(IN const PTEEHANDLE handle, TeeLogCallback log_callback);

/*! Retrieve client maximum message length (MTU)
 *
 *  \param handle The handle of the session.
 *  \return client maximum message length.
 *          If client never connected, will return zero.
 */
uint32_t TEEAPI TeeGetMaxMsgLen(IN const PTEEHANDLE handle);

/*! Retrieve client protocol version
 *
 *  \param handle The handle of the session.
 *  \return client protocol version.
 *          If client never connected, will return zero.
 */
uint8_t TEEAPI TeeGetProtocolVer(IN const PTEEHANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __METEE_H */
