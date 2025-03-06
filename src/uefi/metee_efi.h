/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef METEE_EFI_H_
#define METEE_EFI_H_


enum METEE_CLIENT_STATE
{
    METEE_CLIENT_STATE_NONE,
    METEE_CLIENT_STATE_CONNECTED,
    METEE_CLIENT_STATE_FAILED
};


#define MAX_CONNECT_RETRIES 3
#define HECI_FLOW_CTRL_MSG_LEN 8


/**
* HECI HW Section
*/

/**
 * HECI addresses and defines
*/


/**
 * register bits - H_CSR
*/
#define H_CBD 0xFF000000  /** Host Circular Buffer Depth */
#define H_CBWP 0x00FF0000 /** Host Circular Buffer Write Pointer */
#define H_CBRP 0x0000FF00 /** Host Circular Buffer Read Pointer */
#define H_RST 0x00000010  /** Host Reset */
#define H_RDY 0x00000008  /** Host Ready */
#define H_IG 0x00000004   /** Host Interrupt Generate */
#define H_IS 0x00000002   /** Host Interrupt Status */
#define H_IE 0x00000001   /** Host Interrupt Enable */

/**
 * register bits - ME_CSR_HA
*/
#define ME_CBD_HRA 0xFF000000  /** ME Circular Buffer Depth Host Read Access */
#define ME_CBWP_HRA 0x00FF0000 /** ME Circular Buffer Write Pointer Host Read Access */
#define ME_CBRP_HRA 0x0000FF00 /** ME Circular Buffer Read Pointer Host Read Access */
#define ME_RST_HRA 0x00000010  /** ME Reset Host Read Access */
#define ME_RDY_HRA 0x00000008  /** ME Ready Host Read Access */
#define ME_IG_HRA 0x00000004   /** ME Interrupt Generate Host Read Access */
#define ME_IS_HRA 0x00000002   /** ME Interrupt Status Host Read Access */
#define ME_IE_HRA 0x00000001   /** ME Interrupt Enable Host Read Access */

/**
 * HECI BUS Interface Section
*/

#define HOST_ENUM_REQ_CMD 0x04
#define HOST_ENUM_RES_CMD 0x84

#define HOST_CLIENT_PROPERTIES_REQ_CMD 0x05
#define HOST_CLIENT_PROPERTIES_RES_CMD 0x85

#define CLIENT_CONNECT_REQ_CMD 0x06
#define CLIENT_CONNECT_RES_CMD 0x86

#define CLIENT_DISCONNECT_REQ_CMD 0x07
#define CLIENT_DISCONNECT_RES_CMD 0x87

#define FLOW_CONTROL_CMD 0x08

#pragma pack(1)

typedef struct _HBM_HOST_ENUMERATION_REQUEST
{
    UINT8 Command;
    UINT8 ClientReqBits;
    UINT8 Reserved[2];
} HBM_HOST_ENUMERATION_REQUEST;

typedef struct _HBM_HOST_ENUMERATION_RESPONSE
{
    UINT8 Command;
    UINT8 Reserved[3];
    UINT8 ValidAddresses[32];
} HBM_HOST_ENUMERATION_RESPONSE;

typedef struct _HBM_CLIENT_CONNECT_REQUEST
{
    UINT8 Command;
    UINT8 MEAddress;
    UINT8 HostAddress;
    UINT8 Reserved;
} HBM_CLIENT_CONNECT_REQUEST;

typedef struct _HBM_CLIENT_CONNECT_RESPONSE
{
    UINT8 Command;
    UINT8 MEAddress;
    UINT8 HostAddress;
    UINT8 Status;
} HBM_CLIENT_CONNECT_RESPONSE;

typedef struct _HBM_CLIENT_DISCONNECT_REQUEST
{
    UINT8 Command;
    UINT8 MEAddress;
    UINT8 HostAddress;
    UINT8 Reserved[1];
} HBM_CLIENT_DISCONNECT_REQUEST;

typedef struct _HBM_CLIENT_DISCONNECT_RESPONSE
{
    UINT8 Command;
    UINT8 MEAddress;
    UINT8 HostAddress;
    UINT8 Status;
    UINT32 Pad;
} HBM_CLIENT_DISCONNECT_RESPONSE;

typedef enum _CLIENT_CONNECT_STATUS
{
    CCS_SUCCESS = 0x00,
    CCS_ALREADY_STARTED = 0x02,
} CLIENT_CONNECT_STATUS;

typedef struct _HBM_FLOW_CONTROL
{
    UINT8 Command;
    UINT8 MEAddress;
    UINT8 HostAddress;
    UINT8 Reserved[5];
} HBM_FLOW_CONTROL;

typedef struct _HECI_CLIENT_PROPERTIES
{
    GUID ProtocolName;
    UINT8 ProtocolVersion;
    UINT8 MaxNumberOfConnections;
    UINT8 Address;
    UINT8 SingleReceiveBuffer;
    UINT32 MaxMessageLength;
    UINT8 FixedAddress;
} HECI_CLIENT_PROPERTIES;

typedef struct _HECI_CLIENT_CONNECTION
{
    HECI_CLIENT_PROPERTIES properties;
    UINT32 ResetGeneration;				/** Should match device Link Reset */
    UINT32 handle;
    BOOLEAN connected;
    UINT8 HostClientId;
} HECI_CLIENT_CONNECTION;

#pragma pack()


#define HECI_FW_STS_COUNT 6

struct HECI_HW_BDF {
    UINT32 Segment;                     /** HECI device Segment */
    UINT32 Bus;                         /** HECI device Bus */
    UINT32 Device;                      /** HECI device Device */
    UINT32 Function;                    /** HECI device Function */
};

struct HECI_HW {
    struct HECI_HW_BDF Bdf;
    struct RegisterOffset {
        UINT32 H_CB_WW;                     /** Host Circular Buffer Write Window */
        UINT32 H_CSR;                       /** Host Control Status */
        UINT32 ME_CB_RW;                    /** ME Circular Buffer Read Window */
        UINT32 ME_CSR_HA;                   /** ME Control Status Host Access */
        UINT32 BaseAddressOffset;           /** HECI registers offset inside device MMIO */                
    } RegisterOffset;
    struct FwStatus {
        UINT32 FW_STS[HECI_FW_STS_COUNT];   /** FW status registers offsets */
        BOOLEAN ResidesInConfigSpace;       /** FW status resides in config space or MMIO */
    } FwStatus;
    UINT32 TrcOffset;                       /** TRC register offset */
};

struct _TEEHANDLE;
struct METEE_EFI_IMPL
{
    struct _TEEHANDLE *TeeHandle;
    GUID ClientGuid;                             /** Client GUID */
    enum METEE_CLIENT_STATE State;               /** The client state */
    HECI_CLIENT_CONNECTION HeciClientConnection; /** HECI EFI Connection information */
    struct HECI_HW Hw;                           /** HECI HW information */ 
    enum HECI_HW_TYPE HwType;                    /** HECI HW type */
};

#define HECI_EFI_PRINT_BDF_STR "[HECI %d:%d:%d:%d] "
#define HECI_EFI_PRINT_BDF_VAL Handle->Hw.Bdf.Segment, Handle->Hw.Bdf.Bus, Handle->Hw.Bdf.Device, Handle->Hw.Bdf.Function

#define EFIPRINT(Handle, fmt, ...) DBGPRINT(Handle, HECI_EFI_PRINT_BDF_STR fmt, HECI_EFI_PRINT_BDF_VAL, ##__VA_ARGS__);

#endif // METEE_EFI_H_
