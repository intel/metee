/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#pragma pack(1)
#define GEN_GET_MKHI_VERSION_CMD        0x01
#define GEN_GET_FW_VERSION_CMD          0x02

// Typedef for GroupID
typedef enum
{
	MKHI_CBM_GROUP_ID = 0,
	MKHI_PM_GROUP_ID,
	MKHI_PWD_GROUP_ID,
	MKHI_FWCAPS_GROUP_ID,
	MKHI_APP_GROUP_ID,      // Reserved (no longer used).
	MKHI_FWUPDATE_GROUP_ID, // This is for manufacturing downgrade
	MKHI_FIRMWARE_UPDATE_GROUP_ID,
	MKHI_BIST_GROUP_ID,
	MKHI_MDES_GROUP_ID,
	MKHI_ME_DBG_GROUP_ID,
	MKHI_MAX_GROUP_ID,
	MKHI_GEN_GROUP_ID = 0xFF
}MKHI_GROUP_ID;
//MKHI host message header. This header is part of HECI message sent from MEBx via
//Host Configuration Interface (HCI). ME Configuration Manager or Power Configuration
//Manager also include this header with appropriate fields set as part of the 
//response message to the HCI.
typedef union _MKHI_MESSAGE_HEADER
{
	uint32_t     Data;
	struct
	{
		uint32_t  GroupId : 8;
		uint32_t  Command : 7;
		uint32_t  IsResponse : 1;
		uint32_t  Reserved : 8;
		uint32_t  Result : 8;
	}Fields;
}MKHI_MESSAGE_HEADER;
static_assert(sizeof(MKHI_MESSAGE_HEADER) == 4, "MKHI header should be 4 bytes exactly!");

typedef struct _GEN_GET_FW_VERSION
{
	MKHI_MESSAGE_HEADER  Header;
}GEN_GET_FW_VERSION;

typedef struct _FW_VERSION
{
	uint32_t CodeMinor : 16;
	uint32_t CodeMajor : 16;
	uint32_t CodeBuildNo : 16;
	uint32_t CodeHotFix : 16;
	uint32_t NFTPMinor : 16;
	uint32_t NFTPMajor : 16;
	uint32_t NFTPBuildNo : 16;
	uint32_t NFTPHotFix : 16;
}FW_VERSION;

typedef struct _GET_FW_VERSION_ACK_DATA
{
	FW_VERSION  FWVersion;
}GET_FW_VERSION_ACK_DATA;

typedef struct _GEN_GET_FW_VERSION_ACK
{
	MKHI_MESSAGE_HEADER      Header;
	GET_FW_VERSION_ACK_DATA  Data;
}GEN_GET_FW_VERSION_ACK;
DEFINE_GUID(GUID_DEVINTERFACE_MKHI, 0x8e6a6715, 0x9abc, 0x4043,
	0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f);
#pragma pack()