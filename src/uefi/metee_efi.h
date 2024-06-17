/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#ifndef METEE_EFI_H_
#define METEE_EFI_H_

typedef struct _HECI_PROTOCOL HECI_PROTOCOL;

/**
  Function sends one message through the HECI circular buffer and waits
  for the corresponding ACK message.

  @param[in, out] *Message        Pointer to the message buffer.
  @param[in] Length               Length of the message in bytes.
  @param[in, out] *RecLength      Length of the message response in bytes.
  @param[in] HostAddress          Address of the sending entity.
  @param[in] MEAddress            Address of the ME entity that should receive the message.

  @exception EFI_SUCCESS          Command succeeded
  @exception EFI_DEVICE_ERROR     HECI Device error, command aborts abnormally
  @exception EFI_TIMEOUT          HECI does not return the buffer before timeout
  @exception EFI_BUFFER_TOO_SMALL Message Buffer is too small for the Acknowledge
  @exception EFI_UNSUPPORTED      Current ME mode doesn't support send message through HECI
**/
typedef EFI_STATUS(EFIAPI *HECI_SENDWACK)(
    IN UINT32 HeciDev,
    IN OUT UINT32 *Message,
    IN OUT UINT32 Length,
    IN OUT UINT32 *RecLength,
    IN UINT8 HostAddress,
    IN UINT8 MEAddress);

/**
  Read the HECI Message from Intel ME with size in Length into
  buffer MessageBody. Set Blocking to BLOCKING and code will
  wait until one message packet is received. When set to
  NON_BLOCKING, if the circular buffer is empty at the time, the
  code will not wait for the message packet.

  @param[in] Blocking             Used to determine if the read is BLOCKING or NON_BLOCKING.
  @param[in] *MessageBody         Pointer to a buffer used to receive a message.
  @param[in, out] *Length         Pointer to the length of the buffer on input and the length of the message on return. (in bytes)

  @retval EFI_SUCCESS             One message packet read.
  @retval EFI_DEVICE_ERROR        Failed to initialize HECI or zero-length message packet read
  @retval EFI_TIMEOUT             HECI is not ready for communication
  @retval EFI_BUFFER_TOO_SMALL    The caller's buffer was not large enough
**/
typedef EFI_STATUS(EFIAPI *HECI_READ_MESSAGE)(
    IN UINT32 HeciDev,
    IN UINT32 Blocking,
    IN UINT32 *MessageBody,
    IN OUT UINT32 *Length);

/**
  Function sends one message (of any length) through the HECI circular buffer.

  @param[in] *Message             Pointer to the message data to be sent.
  @param[in] Length               Length of the message in bytes.
  @param[in] HostAddress          The address of the host processor.
  @param[in] MEAddress            Address of the ME subsystem the message is being sent to.

  @retval EFI_SUCCESS             One message packet sent.
  @retval EFI_DEVICE_ERROR        Failed to initialize HECI
  @retval EFI_TIMEOUT             HECI is not ready for communication
  @exception EFI_UNSUPPORTED      Current ME mode doesn't support send message through HECI
**/
typedef EFI_STATUS(EFIAPI *HECI_SEND_MESSAGE)(
    IN UINT32 HeciDev,
    IN UINT32 *Message,
    IN UINT32 Length,
    IN UINT8 HostAddress,
    IN UINT8 MEAddress);

/**
  Reset the HECI Controller with algorithm defined in the RS -
  Intel(R) Management Engine - Host Embedded Controller
  Interface Hardware Programming Specification (HPS)

  @retval EFI_TIMEOUT             HECI does not return the buffer before timeout
  @retval EFI_SUCCESS             Interface reset
**/
typedef EFI_STATUS(EFIAPI *HECI_RESET)(
    IN UINT32 HeciDev);

/**
  Initialize the HECI Controller with algorithm defined in the
  RS - Intel(R) Management Engine - Host Embedded Controller
  Interface Hardware Programming Specification (HPS).
  Determines if the HECI device is present and, if present,
  initializes it for use by the BIOS.

  @retval EFI_SUCCESS             HECI device is present and initialized
  @retval EFI_TIMEOUT             HECI does not return the buffer before timeout
**/
typedef EFI_STATUS(EFIAPI *HECI_INIT)(
    IN UINT32 HeciDev);

/**
  Re-initialize the HECI Controller with algorithm defined in the RS - Intel(R) Management Engine
  - Host Embedded Controller Interface Hardware Programming Specification (HPS).
  Heci Re-initializes it for Host

  @retval EFI_TIMEOUT             HECI does not return the buffer before timeout
  @retval EFI_STATUS              Status code returned by ResetHeciInterface
**/
typedef EFI_STATUS(EFIAPI *HECI_REINIT)(
    IN UINT32 HeciDev);

/**
  Reset Intel ME and timeout if Ready is not set after Delay timeout

  @param[in] Delay                Timeout value in microseconds

  @retval EFI_TIMEOUT             HECI does not return the buffer before timeout
  @retval EFI_SUCCESS             Me is ready
**/
typedef EFI_STATUS(EFIAPI *HECI_RESET_WAIT)(
    IN UINT32 HeciDev,
    IN UINT32 Delay);

/**
  Get an abstract Intel ME State from Firmware Status Register.
  This is used to control BIOS flow for different Intel ME
  functions.
  The ME status information is obtained by sending HECI messages
  to Intel ME and is used both by the platform code and
  reference code. This will optimize boot time because system
  BIOS only sends each HECI message once. It is recommended to
  send the HECI messages to Intel ME only when ME mode is normal
  (Except for HMRFPO Disable Message) and ME State is NORMAL or
  RECOVERY (Suitable for AT and Kernel Messaging only).

  @param[out] MeStatus            Pointer for abstract status report

  @retval EFI_SUCCESS             MeStatus copied
  @retval EFI_INVALID_PARAMETER   Pointer of MeStatus is invalid
**/
typedef EFI_STATUS(EFIAPI *HECI_GET_ME_STATUS)(
    OUT UINT32 *Status);

/**
  Get an abstract ME operation mode from firmware status
  register. This is used to control BIOS flow for different
  Intel ME functions.

  @param[out] MeMode              Pointer for ME Mode report,
                                  @see MeState.h - Abstract ME Mode definitions.

  @retval EFI_SUCCESS             MeMode copied
  @retval EFI_INVALID_PARAMETER   Pointer of MeMode is invalid
**/
typedef EFI_STATUS(EFIAPI *HECI_GET_ME_MODE)(
    OUT UINT32 *Mode);

/**
  HECI protocol provided for DXE phase
  The interface functions are for sending/receiving HECI messages between host and Intel ME subsystem.
  There is also support to control HECI Initialization and get Intel ME status.
**/
struct _HECI_PROTOCOL
{
    HECI_SENDWACK SendwACK;         /** Send HECI message and wait for respond */
    HECI_READ_MESSAGE ReadMsg;      /** Receive HECI message */
    HECI_SEND_MESSAGE SendMsg;      /** Send HECI message */
    HECI_RESET ResetHeci;           /** Reset HECI device */
    HECI_INIT InitHeci;             /** Initialize HECI device */
    HECI_RESET_WAIT MeResetWait;    /** Intel ME Reset Wait Timer */
    HECI_REINIT ReInitHeci;         /** Re-initialize HECI */
    HECI_GET_ME_STATUS GetMeStatus; /** Get Intel ME Status register */
    HECI_GET_ME_MODE GetMeMode;     /** Get Intel ME mode */
};

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
#define H_CB_WW 0x0   /** Host Circular Buffer Write Window */
#define H_CSR 0x4     /** Host Control Status */
#define ME_CB_RW 0x8  /** ME Circular Buffer Read Window */
#define ME_CSR_HA 0xC /** ME Control Status Host Access */

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
    UINT8 IsFixed;
} HECI_CLIENT_PROPERTIES;

typedef struct _HECI_CLIENT_CONNECTION
{
    HECI_CLIENT_PROPERTIES properties;
    UINT32 handle;
    UINT32 initialized;
    UINT8 has_fc;
} HECI_CLIENT_CONNECTION;

#pragma pack()

struct _TEEHANDLE;

struct METEE_EFI_IMPL
{
    struct _TEEHANDLE *TeeHandle;
    GUID ClientGuid;                             /** Client GUID */
    enum METEE_CLIENT_STATE State;               /** The client state */
    HECI_CLIENT_CONNECTION HeciClientConnection; /** HECI EFI Connection information */
    HECI_PROTOCOL *HeciProtocol;                 /** BIOS HECI Protocol implementation */
    UINT32 HeciDeviceBus;                        /** HECI device Bdf */
    UINT32 HeciDeviceFunction;                   /** HECI device bdF */
};

#endif // METEE_EFI_H_
