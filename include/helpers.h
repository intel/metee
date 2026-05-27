/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2026 Intel Corporation
 */
#ifndef __HELPERS_H
#define __HELPERS_H
#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MSG_LEN 1024
#if DEBUG
	#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_VERBOSE
#else
	#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_QUIET
#endif

#ifdef EFI
	#define DEBUG_PRINT_ME_PREFIX_INTERNAL "TEELIB: (%a:%a():%d) "
	#ifdef METEE_EFI_STDLIB_SUPPORT
		#define DEBUG_PRINT_ME_PREFIX_EXTERNAL "TEELIB: (%s:%s():%d) "
	#else
		#define DEBUG_PRINT_ME_PREFIX_EXTERNAL "TEELIB: (%a:%a():%d) "
	#endif /* METEE_EFI_STDLIB_SUPPORT */
#else /* EFI */
	#define DEBUG_PRINT_ME_PREFIX_INTERNAL "TEELIB: (%s:%s():%d) "
	#define DEBUG_PRINT_ME_PREFIX_EXTERNAL "TEELIB: (%s:%s():%d) "
#endif /* EFI */

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "metee.h"

	#define MALLOC(X)   HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, X)
	#define FREE(X)     {if(X) { HeapFree(GetProcessHeap(), 0, X); X = NULL ; } }

	#define IS_HANDLE_INVALID(h) (NULL == h || 0 == h->handle || INVALID_HANDLE_VALUE == h->handle)
	#define INIT_STATUS TEE_INTERNAL_ERROR
#else
	#define MALLOC(X)   malloc(X)
	#define FREE(X)     { if(X) { free(X); X = NULL ; } }

	#define IS_HANDLE_INVALID(h) (NULL == h || 0 == h->handle || -1 == h->handle)
	#define INIT_STATUS -EPERM
#endif /* _WIN32 */

#if SYSLOG
	#ifdef _WIN32
		void DebugPrintMe(const char* args, ...);
		#define ErrorPrintMe(fmt, ...) DebugPrintMe(fmt, __VA_ARGS__)
	#elif defined(ANDROID)
		#define LOG_TAG "metee"
		#include <android/log_macros.h>
		#define DebugPrintMe(fmt, ...) ALOGV(fmt, ##__VA_ARGS__)
		#define ErrorPrintMe(fmt, ...) ALOGE(fmt, ##__VA_ARGS__)
	#elif defined(EFI)
		#define DebugPrintMe(fmt, ...) AsciiPrint(fmt, ##__VA_ARGS__)
		#define ErrorPrintMe(fmt, ...) AsciiPrint(fmt, ##__VA_ARGS__)
	#else
		#include <syslog.h>
		#define DebugPrintMe(fmt, ...) syslog(LOG_DEBUG, fmt, ##__VA_ARGS__)
		#define ErrorPrintMe(fmt, ...) syslog(LOG_ERR, fmt, ##__VA_ARGS__)
	#endif
#else /* SYSLOG */
	#ifdef EFI
		#define DebugPrintMe(fmt, ...) AsciiPrint(fmt, ##__VA_ARGS__)
		#define ErrorPrintMe(fmt, ...) AsciiPrint(fmt, ##__VA_ARGS__)
	#else
		#include <stdlib.h>
		#define DebugPrintMe(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
		#define ErrorPrintMe(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
	#endif
#endif /* SYSLOG */

#define LEGACY_CALLBACK_SET(h) ((h)->log_callback ? 1 : 0)
#define STANDARD_CALLBACK_SET(h) ((h)->log_callback2 ? 1 : 0)

void CallbackPrintHelper(IN PTEEHANDLE handle, bool is_error, const char* args, ...);

#define DBGPRINT(h, _x_, ...) \
	if ((h) && (h)->log_level >= TEE_LOG_LEVEL_VERBOSE) { \
		if (LEGACY_CALLBACK_SET(h)) \
		    (h)->log_callback(false, DEBUG_PRINT_ME_PREFIX_EXTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
		else if (STANDARD_CALLBACK_SET(h)) \
				CallbackPrintHelper((h), false, DEBUG_PRINT_ME_PREFIX_EXTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
		else \
			DebugPrintMe(DEBUG_PRINT_ME_PREFIX_INTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
	}

#define ERRPRINT(h, _x_, ...) \
	if ((h) && (h)->log_level >= TEE_LOG_LEVEL_ERROR) { \
		if (LEGACY_CALLBACK_SET(h)) \
		    (h)->log_callback(true, DEBUG_PRINT_ME_PREFIX_EXTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
		else if (STANDARD_CALLBACK_SET(h)) \
				CallbackPrintHelper((h), true, DEBUG_PRINT_ME_PREFIX_EXTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
		else \
			ErrorPrintMe(DEBUG_PRINT_ME_PREFIX_INTERNAL _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__); \
	}

#define FUNC_ENTRY(h)         DBGPRINT(h, "Entry\n")
#define FUNC_EXIT(h, status)  DBGPRINT(h, "Exit with status: %d\n", status)

#ifdef EFI
#include <Library/BaseMemoryLib.h>
static inline void __tee_init_handle(PTEEHANDLE handle) { SetMem(handle, sizeof(TEEHANDLE), 0);}
#else /* _WIN32, LINUX */
static inline void __tee_init_handle(PTEEHANDLE handle) { memset(handle, 0, sizeof(TEEHANDLE));}
#endif /* EFI */


#ifdef __cplusplus
}
#endif
#endif /* __HELPERS_H */
