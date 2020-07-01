/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#ifndef __HELPERS_H
#define __HELPERS_H

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "metee.h"

	#if _DEBUG
		#define PRINTS_ENABLE
	#endif

	#define MALLOC(X)   HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, X)
	#define FREE(X)     {if(X) { HeapFree(GetProcessHeap(), 0, X); X = NULL ; } }

	#define DEBUG_MSG_LEN 1024

	static void DebugPrint(const char* args, ...)
	{
		char msg[DEBUG_MSG_LEN + 1];
		va_list varl;
		va_start(varl, args);
		vsprintf_s(msg, DEBUG_MSG_LEN, args, varl);
		va_end(varl);

	#ifdef SYSLOG
		OutputDebugStringA(msg);
	#else
		fprintf(stderr, "%s", msg);
	#endif /* SYSLOG */
	}

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
		#if LOG_NDEBUG
			#define PRINTS_ENABLE
		#endif
	#else /* LINUX */
		#ifdef DEBUG
			#define PRINTS_ENABLE
		#endif
		#ifdef SYSLOG
			#include <syslog.h>
			#define DebugPrint(fmt, ...) syslog(LOG_DEBUG, fmt, ##__VA_ARGS__)
			#define ErrorPrint(fmt, ...) syslog(LOG_ERR, fmt, ##__VA_ARGS__)
		#else
			#include <stdlib.h>
			#define DebugPrint(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
			#define ErrorPrint(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
		#endif /* SYSLOG */
	#endif /* ANDROID */

	#define MALLOC(X)   malloc(X)
	#define FREE(X)     { if(X) { free(X); X = NULL ; } }

	#define IS_HANDLE_INVALID(h) (NULL == h || 0 == h->handle || -1 == h->handle)
	#define INIT_STATUS -EPERM
#endif /* _WIN32 */


#ifdef PRINTS_ENABLE
#define DBGPRINT(_x_, ...) \
	DebugPrint("TEELIB: (%s:%s():%d) " _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);
#else
	#define DBGPRINT(_x_, ...)
#endif /* PRINTS_ENABLE */

#ifdef PRINTS_ENABLE
#define ERRPRINT(_x_, ...) \
	ErrorPrint("TEELIB: (%s:%s():%d) " _x_,__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);
#else
	#define ERRPRINT(_x_, ...)
#endif

#define FUNC_ENTRY()         ERRPRINT("Entry\n")
#define FUNC_EXIT(status)    ERRPRINT("Exit with status: %d\n", status)

static inline void __tee_init_handle(PTEEHANDLE handle) { memset(handle, 0, sizeof(TEEHANDLE));}

#endif /* __HELPERS_H */
