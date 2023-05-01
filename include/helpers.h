/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2021 Intel Corporation
 */
#ifndef __HELPERS_H
#define __HELPERS_H

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "metee.h"

	#if _DEBUG
		#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_VERBOSE
	#else
		#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_QUIET
	#endif

	#define MALLOC(X)   HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, X)
	#define FREE(X)     {if(X) { HeapFree(GetProcessHeap(), 0, X); X = NULL ; } }

	#define DEBUG_MSG_LEN 1024

	void DebugPrint(const char* args, ...);

	#define ErrorPrint(fmt, ...) DebugPrint(fmt, __VA_ARGS__)
	#define IS_HANDLE_INVALID(h) (NULL == h || 0 == h->handle || INVALID_HANDLE_VALUE == h->handle)
	#define INIT_STATUS TEE_INTERNAL_ERROR
#else
	#ifdef ANDROID
		// For debugging
		//#define LOG_NDEBUG 0
		#define LOG_TAG "metee"
		#include <cutils/log.h>
		#define DebugPrint(fmt, ...) ALOGV_IF(true, fmt, ##__VA_ARGS__)
		#define ErrorPrint(fmt, ...) ALOGE_IF(true, fmt, ##__VA_ARGS__)

		#ifdef LOG_NDEBUG
			#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_VERBOSE
		#else
			#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_QUIET
		#endif
	#else /* LINUX */
		#ifdef SYSLOG
			#include <syslog.h>
			#define DebugPrint(fmt, ...) syslog(LOG_DEBUG, fmt, ##__VA_ARGS__)
			#define ErrorPrint(fmt, ...) syslog(LOG_ERR, fmt, ##__VA_ARGS__)
		#else
			#include <stdlib.h>
			#define DebugPrint(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
			#define ErrorPrint(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
		#endif /* SYSLOG */

		#ifdef DEBUG
			#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_VERBOSE
		#else
			#define TEE_DEFAULT_LOG_LEVEL TEE_LOG_LEVEL_QUIET
		#endif

	#endif /* ANDROID */

	#define MALLOC(X)   malloc(X)
	#define FREE(X)     { if(X) { free(X); X = NULL ; } }

	#define IS_HANDLE_INVALID(h) (NULL == h || 0 == h->handle || -1 == h->handle)
	#define INIT_STATUS -EPERM
#endif /* _WIN32 */


#define DBGPRINT(h, _x_, ...) \
	if (h && h->log_level >= TEE_LOG_LEVEL_VERBOSE) \
		DebugPrint("TEELIB: (%s:%s():%d) " _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);

#define ERRPRINT(h, _x_, ...) \
	if (h && h->log_level >= TEE_LOG_LEVEL_ERROR) \
		ErrorPrint("TEELIB: (%s:%s():%d) " _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);

#define FUNC_ENTRY(h)         ERRPRINT(h, "Entry\n")
#define FUNC_EXIT(h, status)  ERRPRINT(h, "Exit with status: %d\n", status)

static inline void __tee_init_handle(PTEEHANDLE handle) { memset(handle, 0, sizeof(TEEHANDLE));}

#endif /* __HELPERS_H */
