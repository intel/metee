/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#ifndef __AMTHI_H
#define __AMTHI_H

#include <windows.h>

static const UINT32 BIOS_VERSION_LEN = 65;
static const UINT32 VERSIONS_NUMBER  = 50;
static const UINT32 UNICODE_STRING_LEN = 20;

typedef unsigned int PT_STATUS;
typedef unsigned int AMT_STATUS;


#pragma pack(1)
typedef struct _AMT_UNICODE_STRING 
{
	UINT16  Length;
	UINT8   String[UNICODE_STRING_LEN];
} AMT_UNICODE_STRING;

typedef struct _AMT_VERSION_TYPE 
{
	AMT_UNICODE_STRING   Description;
	AMT_UNICODE_STRING   Version;
}AMT_VERSION_TYPE;

typedef struct _PTHI_VERSION 
{
	UINT8   MajorNumber;
	UINT8   MinorNumber;
} PTHI_VERSION;

typedef struct _CODE_VERSIONS 
{
	UINT8   BiosVersion[BIOS_VERSION_LEN];
	UINT32  VersionsCount;
	AMT_VERSION_TYPE Versions[VERSIONS_NUMBER];
} CODE_VERSIONS;

typedef struct _COMMAND_FMT
{
	union
	{
		UINT32  val;
		struct
		{
			UINT32   Operation   : 23;
			UINT32   IsResponse  : 1;
			UINT32   Class       : 8;
		} fields;
	} cmd;

} COMMAND_FMT;

typedef struct _AMT_ANSI_STRING
{
	UINT16	Length;
	CHAR*	Buffer;
}AMT_ANSI_STRING;


typedef struct _PTHI_MESSAGE_HEADER
{
	PTHI_VERSION Version;
	UINT16       Reserved;
	COMMAND_FMT  Command;
	UINT32       Length;

} PTHI_MESSAGE_HEADER;

typedef struct _CFG_GET_CODE_VERSIONS_RESPONSE
{
	PTHI_MESSAGE_HEADER Header;
	AMT_STATUS   Status;
	CODE_VERSIONS CodeVersions;
} CFG_GET_CODE_VERSIONS_RESPONSE;

const UINT32 CODE_VERSIONS_REQUEST     = 0x0400001A;
const UINT32 CODE_VERSIONS_RESPONSE    = 0x0480001A;

const PTHI_MESSAGE_HEADER GET_CODE_VERSION_HEADER =
{
	{1,1},0,{CODE_VERSIONS_REQUEST},0
};



typedef struct
{
    UINT8                              Command;
    UINT8                              ByteCount;
    UINT8                              SubCommand;
    UINT8                              VersionNumber;
}  AMT_GetMngMacAddress_Request;

typedef struct
{
    UINT8                              Command;
    UINT8                              ByteCount;
    UINT8                              SubCommand;
    UINT8                              VersionNumber;
    AMT_STATUS                         Status;
    UINT8	                           Address[6];  // returned upon success only
}  AMT_GetMngMacAddress_Response;
#pragma pack()

#endif