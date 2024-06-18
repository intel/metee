/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/UefiBootServicesTableLib.h>

#include "metee.h"
#include "metee_efi.h"
#include "heci_efi.h"

#include "helpers.h"

#define BIOS_FIXED_HOST_ADDR 0
#define POLICY_CONNECTED_HOST_ADDR 1

#define HECI_HBM_MSG_ADDR 0x00

#define NON_BLOCKING 0
#define BLOCKING 1

#define HECI_TIMEOUT_STALL_SEC (40 * 1000 * 1000) ///< 40 seconds

#pragma pack(1)

typedef struct _HBM_CLIENT_PROP_MSG
{
	UINT8 Command;
	UINT8 Address;
	UINT8 Resv[2];
} HBM_CLIENT_PROP_MSG;

typedef struct _HBM_CLIENT_PROP_MSG_REPLY
{
	UINT8 CmdReply;
	UINT8 Address;
	UINT8 Status;
	UINT8 Resv;
	GUID ProtocolName;
	UINT8 ProtocolVersion;
	UINT8 MaximumConnections;
	UINT8 FixedAddress;
	UINT8 SingleRecvBuffer;
	UINT32 MaxMessageLength;
} HBM_CLIENT_PROP_MSG_REPLY;

#pragma pack()

static EFI_STATUS
heciReadMsg(
		IN struct METEE_EFI_IMPL *Handle,
		IN UINT32 Blocking,
		IN UINT32 *MessageBody,
		IN UINT32 MessageBodyLenBytes,
		OUT UINT32 *BytesRead)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	UINT32 length = MessageBodyLenBytes;

	FUNC_ENTRY(Handle->TeeHandle);

	status = Handle->HeciProtocol->ReadMsg(Handle->HeciDevice, Blocking, MessageBody, &length);
	if (EFI_ERROR(status))
	{
		*BytesRead = 0;
	}
	else
	{
		*BytesRead = length;
	}

	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

static EFI_STATUS
heciSendMsg(
		IN struct METEE_EFI_IMPL *Handle,
		IN UINT32 *Message,
		IN UINT32 Length,
		IN UINT8 HostAddress,
		IN UINT8 MeAddress)
{
	EFI_STATUS status = EFI_UNSUPPORTED;

	FUNC_ENTRY(Handle->TeeHandle);

	status = Handle->HeciProtocol->SendMsg(Handle->HeciDevice, Message, Length, HostAddress, MeAddress);

	FUNC_EXIT(Handle->TeeHandle, status);

	return status;
}

static EFI_STATUS
heciReset(
		IN struct METEE_EFI_IMPL *Handle)
{
	EFI_STATUS status = EFI_UNSUPPORTED;

	FUNC_ENTRY(Handle->TeeHandle);

	status = Handle->HeciProtocol->ResetHeci(Handle->HeciDevice);

	FUNC_EXIT(Handle->TeeHandle, status);

	return status;
}

EFI_STATUS
HeciUninitialize(
		IN struct METEE_EFI_IMPL *Handle)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HBM_CLIENT_DISCONNECT_REQUEST disconnectMsg;
	HBM_CLIENT_DISCONNECT_RESPONSE disconnectMsgReply;
	UINT32 msgReplyLen = 0;
	HECI_CLIENT_CONNECTION *client = &Handle->HeciClientConnection;

	FUNC_ENTRY(Handle->TeeHandle);

	DBGPRINT(Handle->TeeHandle, "####Heci Uninitialize####\n");

	if (client->properties.IsFixed)
	{
		DBGPRINT(Handle->TeeHandle, "######CONNECTION CLOSED SUCCESSFULLY######: Fixed client\n");
		status = EFI_SUCCESS;
		goto End;
	}

	SetMem(&disconnectMsg, sizeof(disconnectMsg), 0x0);
	SetMem(&disconnectMsgReply, sizeof(disconnectMsgReply), 0x0);

	disconnectMsg.Command = CLIENT_DISCONNECT_REQ_CMD;
	disconnectMsg.MEAddress = client->properties.Address;
	disconnectMsg.HostAddress = BIOS_FIXED_HOST_ADDR + 1;

	DBGPRINT(Handle->TeeHandle, "####Prior to Heci-SendMsg: Command: %02X, MEAddress: %02X, HostAddress: %X\n", disconnectMsg.Command, disconnectMsg.MEAddress, disconnectMsg.HostAddress);
	status = heciSendMsg(Handle, (UINT32 *)&disconnectMsg, (UINT32)sizeof(disconnectMsg), (UINT8)BIOS_FIXED_HOST_ADDR, (UINT8)HECI_HBM_MSG_ADDR);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "####Failed to send HBM_CLIENT_DISCONNECT_REQUEST. Status: %d\n", status);
		goto End;
	}

	status = heciReadMsg(Handle, BLOCKING, (UINT32 *)&disconnectMsgReply, sizeof(disconnectMsgReply), &msgReplyLen);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "####HeciUninitialize failed with ReadMsg, Status: %d.\n", status);
		goto End;
	}
	DBGPRINT(Handle->TeeHandle, "#### disconnectMsgReply Command %02X , Status: %02X.\n", disconnectMsgReply.Command, disconnectMsgReply.Status);
	if (CLIENT_DISCONNECT_RES_CMD != disconnectMsgReply.Command) {
		DBGPRINT(Handle->TeeHandle, "####Unexpected disconnect message reply command\n");
		status = EFI_PROTOCOL_ERROR;
		goto End;
	}
	if (0 != disconnectMsgReply.Status) {
		 DBGPRINT(Handle->TeeHandle, "####Unexpected disconnect message reply status\n");
		status = EFI_PROTOCOL_ERROR;
		goto End;
	}

	DBGPRINT(Handle->TeeHandle, "####SUCCESSFULLY DISCONNECT FROM INTERFACE####\n");
	status = EFI_SUCCESS;
End:
	client->initialized = FALSE;
	FreePool(client);
	client = NULL;
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

static EFI_STATUS
heciFwToHostFlowControl(
		IN struct METEE_EFI_IMPL *Handle,
		IN HECI_CLIENT_CONNECTION *client)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HBM_FLOW_CONTROL flowCtrlMsg;
	UINT32 msgLen = 0;

	FUNC_ENTRY(Handle->TeeHandle);

	DBGPRINT(Handle->TeeHandle, "####FW to Host flow control####\n");

	if (NULL == client)
	{
		status = EFI_INVALID_PARAMETER;
		goto End;
	}

	if (1 == client->properties.IsFixed)
	{
		DBGPRINT(Handle->TeeHandle, "####Fixed: Exiting FW to Host flow control####\n");
		status = EFI_SUCCESS;
		goto End;
	}

	DBGPRINT(Handle->TeeHandle, "####First get flow control from FW\n");

	SetMem(&flowCtrlMsg, msgLen, 0x0);

	status = heciReadMsg(Handle, BLOCKING, (UINT32 *)&flowCtrlMsg, sizeof(HBM_FLOW_CONTROL), &msgLen);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "#####Flow control: wait for FW failed with status: %d.\n", status);
		goto End;
	}
 
End:
	DBGPRINT(Handle->TeeHandle, "####Exiting FW to Host flow control####\n");
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

static EFI_STATUS
heciHostToSecFlowControl(
		IN struct METEE_EFI_IMPL *Handle,
		IN HECI_CLIENT_CONNECTION *client,
		IN UINT8 secAddress)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HBM_FLOW_CONTROL flowCtrlMsg;

	FUNC_ENTRY(Handle->TeeHandle);

	DBGPRINT(Handle->TeeHandle, "####Host to Sec flow control####\n");
	if (NULL == client)
	{
		status = EFI_INVALID_PARAMETER;
		goto End;
	}

	if (1 == client->properties.IsFixed)
	{
		DBGPRINT(Handle->TeeHandle, "####Fixed: Exiting Host to Sec flow control####\n");
		status = EFI_SUCCESS;
		goto End;
	}

	// Send flow control to FW

	SetMem(&flowCtrlMsg, sizeof(flowCtrlMsg), 0x0);
	flowCtrlMsg.Command = FLOW_CONTROL_CMD;
	flowCtrlMsg.HostAddress = BIOS_FIXED_HOST_ADDR + 1;
	flowCtrlMsg.MEAddress = secAddress;

	status = heciSendMsg(Handle, (UINT32 *)&flowCtrlMsg, sizeof(flowCtrlMsg), BIOS_FIXED_HOST_ADDR, HECI_HBM_MSG_ADDR);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "#####Flow control: send to FW failed with status: %d.\n", status);
		goto End;
	}

End:
	DBGPRINT(Handle->TeeHandle, "####Exiting Host to FW flow control####\n");
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

EFI_STATUS
HeciConnectClient(
		IN struct METEE_EFI_IMPL *Handle)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HBM_HOST_ENUMERATION_REQUEST enumMsg;
	HBM_HOST_ENUMERATION_RESPONSE enumMsgReply;
	HBM_CLIENT_PROP_MSG propMsg;
	HBM_CLIENT_PROP_MSG_REPLY propMsgReply;
	HBM_CLIENT_CONNECT_REQUEST connectMsg;
	HBM_CLIENT_CONNECT_RESPONSE connectMsgReply;
	UINT32 msgReplyLen = 0;
	HECI_CLIENT_CONNECTION *client = &Handle->HeciClientConnection;

	FUNC_ENTRY(Handle->TeeHandle);

	if (client->initialized)
	{
		HeciUninitialize(Handle);
	}

	client->properties.IsFixed = 0;
	client->properties.Address = 0;

	status = heciReset(Handle);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "Failed to send HECI reset. Status: %d\n", status); 
		status = EFI_DEVICE_ERROR;
		goto End;
	}

	SetMem(&enumMsg, sizeof(enumMsg), 0x0);
	SetMem(&enumMsgReply, sizeof(enumMsgReply), 0x0);
	enumMsg.Command = HOST_ENUM_REQ_CMD;

	for (UINT32 i = 0; i < MAX_CONNECT_RETRIES; ++i)
	{
		DBGPRINT(Handle->TeeHandle, "HECI connect client attempt %u.\n", i);
		status = heciSendMsg(Handle, (UINT32 *)&enumMsg, sizeof(enumMsg), BIOS_FIXED_HOST_ADDR, HECI_HBM_MSG_ADDR);
		if (EFI_ERROR(status))
		{
			DBGPRINT(Handle->TeeHandle, "Failed to send HBM_HOST_ENUMERATION_REQUEST.\n");
			goto End;
		}

		status = heciReadMsg(Handle, BLOCKING, (UINT32 *)&enumMsgReply, sizeof(HBM_HOST_ENUMERATION_RESPONSE), &msgReplyLen);
		if (EFI_ERROR(status))
		{
			DBGPRINT(Handle->TeeHandle, "Failed to read HBM_HOST_ENUMERATION_REQUEST. Status: %d\n", status);
			if (EFI_TIMEOUT == status)
			{
				continue;
			}
			goto End;
		}

		break; // efiStatus is EFI_SUCCESS
	}

	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "Failed to read HBM_HOST_ENUMERATION_REQUEST. Status: %d.\n", status);
		goto End;
	}

	if (enumMsgReply.Command != HOST_ENUM_RES_CMD)
	{
		DBGPRINT(Handle->TeeHandle, "HBM enum got invalid reply: %u\n", enumMsgReply.Command);
		status = EFI_PROTOCOL_ERROR;
		goto End;
	}

#define TEE_CLIENTS_IS_CLIENT_EXIST(bitmap, inx) ((bitmap[inx/8] & (1 << (inx % 8))) != 0)
#define TEE_MAX_FW_CLIENTS 256

	SetMem(&propMsg, sizeof(propMsg), 0x0);
	propMsg.Command = HOST_CLIENT_PROPERTIES_REQ_CMD;

	for (UINT8 enumIdx = 1; enumIdx < TEE_MAX_FW_CLIENTS; ++enumIdx)
	{
		if (!TEE_CLIENTS_IS_CLIENT_EXIST(enumMsgReply.ValidAddresses, enumIdx)) {
			continue;
		}
		propMsg.Address = enumIdx;

		status = heciSendMsg(Handle, (UINT32 *)&propMsg, sizeof(propMsg), BIOS_FIXED_HOST_ADDR, HECI_HBM_MSG_ADDR);
		if (EFI_ERROR(status))
		{
			DBGPRINT(Handle->TeeHandle, "Failed to send HBM_CLIENT_PROP_MSG. Status: %d\n", status);
			goto End;
		}

		SetMem(&propMsgReply, sizeof(HBM_CLIENT_PROP_MSG_REPLY), 0x0);
		status = heciReadMsg(Handle, BLOCKING, (UINT32 *)&propMsgReply, sizeof(HBM_CLIENT_PROP_MSG_REPLY), &msgReplyLen);
		if (EFI_ERROR(status))
		{
			DBGPRINT(Handle->TeeHandle, "heciReadMsg failed to read HBM_CLIENT_PROP_MSG response. Status: %d\n", status);
			goto End;
		}

		if (propMsgReply.CmdReply != HOST_CLIENT_PROPERTIES_RES_CMD)
		{
			DBGPRINT(Handle->TeeHandle, "Not HBM_CLIENT_PROP_MSG response command: %d\n", propMsgReply.CmdReply);
			goto End;
		}

		if (propMsgReply.Status != 0)
		{
			DBGPRINT(Handle->TeeHandle, "propMsgReply.Status: %d\n", propMsgReply.Status);
			continue;
		}

		if (CompareMem(&Handle->ClientGuid, &propMsgReply.ProtocolName, sizeof(GUID)) == 0)
		{
			DBGPRINT(Handle->TeeHandle,
							 "####Match:%u - Guid:%08x-%04x-%04x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x.\n",
							 enumIdx,
							 propMsgReply.ProtocolName.Data1,
							 propMsgReply.ProtocolName.Data2,
							 propMsgReply.ProtocolName.Data3,
							 propMsgReply.ProtocolName.Data4[0],
							 propMsgReply.ProtocolName.Data4[1],
							 propMsgReply.ProtocolName.Data4[2],
							 propMsgReply.ProtocolName.Data4[3],
							 propMsgReply.ProtocolName.Data4[4],
							 propMsgReply.ProtocolName.Data4[5],
							 propMsgReply.ProtocolName.Data4[6],
							 propMsgReply.ProtocolName.Data4[7]);

			client->properties.Address = propMsgReply.Address;
			client->properties.MaxMessageLength = propMsgReply.MaxMessageLength;
			break;
		}
	}

	if (client->properties.Address == 0)
	{
		DBGPRINT(Handle->TeeHandle, "Failed to retrieve FW address.\n"); 
		status = EFI_NOT_FOUND;
		goto End;
	}

	DBGPRINT(Handle->TeeHandle, "####Connection property#####\n");
	DBGPRINT(Handle->TeeHandle,
					 "Address: %u, ProtocolVersion: %u, MaximumConnections: %u, FixedAddress: %u, SingleRecvBuffer: %u, MaxMessageLength: %u.\n",
					 propMsgReply.Address,
					 propMsgReply.ProtocolVersion,
					 propMsgReply.MaximumConnections,
					 propMsgReply.FixedAddress,
					 propMsgReply.SingleRecvBuffer,
					 propMsgReply.MaxMessageLength);

	// copy the Property Message Reply results to client->properties
	client->properties.ProtocolName = propMsgReply.ProtocolName;
	client->properties.ProtocolVersion = propMsgReply.ProtocolVersion;
	client->properties.MaxNumberOfConnections = propMsgReply.MaximumConnections;
	client->properties.Address = propMsgReply.Address;
	client->properties.SingleReceiveBuffer = propMsgReply.SingleRecvBuffer;
	client->properties.MaxMessageLength = propMsgReply.MaxMessageLength;

	if (propMsgReply.FixedAddress != 0)
	{
		client->properties.Address = propMsgReply.FixedAddress;
		client->properties.IsFixed = 1;
		client->initialized = TRUE;
		DBGPRINT(Handle->TeeHandle, "######CONNECTION SETUP SUCCESSFULLY######\n");
		status = EFI_SUCCESS;
		goto End;
	}

	// Now try to connect to the heci interface.

	SetMem(&connectMsg, sizeof(connectMsg), 0x0);
	SetMem(&connectMsgReply, sizeof(connectMsgReply), 0x0);

	connectMsg.Command = CLIENT_CONNECT_REQ_CMD;
	connectMsg.MEAddress = client->properties.Address;
	connectMsg.HostAddress = BIOS_FIXED_HOST_ADDR + 1;

	DBGPRINT(Handle->TeeHandle, "ConnectMsg: Before SendMsg: HeciDevice = %d\n", Handle->HeciDevice);
	status = heciSendMsg(Handle, (UINT32 *)&connectMsg, sizeof(connectMsg), BIOS_FIXED_HOST_ADDR, HECI_HBM_MSG_ADDR);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "Connect Send failed with status: %d.\n", status);
		goto End;
	}

	status = heciReadMsg(Handle, BLOCKING, (UINT32 *)&connectMsgReply, sizeof(HBM_CLIENT_CONNECT_RESPONSE), &msgReplyLen);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "Connect Recv failed with status: %d.\n", status);
		goto End;
	}

	if (connectMsgReply.Command != CLIENT_CONNECT_RES_CMD)
	{
		DBGPRINT(Handle->TeeHandle, "Connect got invalid reply: %u\n", connectMsgReply.Command);
		goto End;
	}

	DBGPRINT(Handle->TeeHandle, "Connect message reply status: %d\n", connectMsgReply.Command);

	if ((CCS_SUCCESS == connectMsgReply.Status) || (CCS_ALREADY_STARTED == connectMsgReply.Status))
	{
		DBGPRINT(Handle->TeeHandle, "######CONNECTION SETUP SUCCESSFULLY######\n");
	}
	else
	{
		DBGPRINT(Handle->TeeHandle, "####Failed to setup connection. Aborted with connectMsgReply.Status: %u.\n", connectMsgReply.Status);
		status = EFI_PROTOCOL_ERROR;
		goto End;
	}

	status = heciFwToHostFlowControl(Handle, client);
	if (EFI_ERROR(status))
	{
		goto End;
	}

	client->initialized = TRUE;
End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

EFI_STATUS
HeciSendMessage(
		IN struct METEE_EFI_IMPL *Handle,
		IN const UINT8 *buffer,
		IN UINT32 bufferLength,
		OUT UINT32 *BytesWritten,
		IN UINT32 timeout)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	UINT8 secAddress = 0;
	UINT8 hostAddress = 0;
	HECI_CLIENT_CONNECTION *client = &Handle->HeciClientConnection;

	FUNC_ENTRY(Handle->TeeHandle);

	if ((NULL == buffer) || (0 == bufferLength) || (BytesWritten == NULL))
	{
		status = EFI_INVALID_PARAMETER;
		goto End;
	}

	if (bufferLength > client->properties.MaxMessageLength)
	{
		status = EFI_BUFFER_TOO_SMALL;
		goto End;
	}

	secAddress = client->properties.Address;
	status = heciHostToSecFlowControl(Handle, client, secAddress);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "heciHostToSecFlowControl Failed. Status: %d", status);
		goto End;
	}

	// Fixed Address expects HostAddress to be 0 while Connected Address requires a 1.
	hostAddress = (client->properties.IsFixed) ? BIOS_FIXED_HOST_ADDR : POLICY_CONNECTED_HOST_ADDR;

	DBGPRINT(Handle->TeeHandle, "HeciDevice: %d, bufferLength: %d, hostAddress: %X, secAddress: %X\n", Handle->HeciDevice, bufferLength, hostAddress, secAddress);
	status = heciSendMsg(Handle, (UINT32*)buffer, bufferLength, hostAddress, secAddress);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "SendMessage: failed to send message. Status: %d.", status);
		goto End;
	}
	*BytesWritten = bufferLength;
	status = EFI_SUCCESS;
End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

EFI_STATUS
HeciReceiveMessage(
		IN struct METEE_EFI_IMPL *Handle,
		OUT UINT8 *Buffer,
		IN UINT32 BufferSize,
		OUT UINT32 *NumOfBytesRead,
		IN UINT32 timeout)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HECI_CLIENT_CONNECTION *client = &Handle->HeciClientConnection;
	UINT32 bytes_read = 0;

	UINT8 messageId = 0;

	FUNC_ENTRY(Handle->TeeHandle);

	if ((Buffer == NULL) || (0 == BufferSize) || (NumOfBytesRead == NULL))
	{
		status = EFI_INVALID_PARAMETER;
		goto End;
	}

	if (BufferSize > client->properties.MaxMessageLength)
	{
		status = EFI_INVALID_PARAMETER;
		goto End;
	}

	DBGPRINT(Handle->TeeHandle, "\nprintout client properties\n");
	DBGPRINT(Handle->TeeHandle,
			"properties.guid:%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
			client->properties.ProtocolName.Data1,
			client->properties.ProtocolName.Data2,
			client->properties.ProtocolName.Data3,
			client->properties.ProtocolName.Data4[0],
			client->properties.ProtocolName.Data4[1],
			client->properties.ProtocolName.Data4[2],
			client->properties.ProtocolName.Data4[3],
			client->properties.ProtocolName.Data4[4],
			client->properties.ProtocolName.Data4[5],
			client->properties.ProtocolName.Data4[6],
			client->properties.ProtocolName.Data4[7]);
	DBGPRINT(Handle->TeeHandle, "properties.ProtocolVersion: %d\n", client->properties.ProtocolVersion);
	DBGPRINT(Handle->TeeHandle, "properties.MaxNumberOfConnections: %d\n", client->properties.MaxNumberOfConnections);
	DBGPRINT(Handle->TeeHandle, "properties.Address: %d\n", client->properties.Address);
	DBGPRINT(Handle->TeeHandle, "properties.SingleReceiveBuffer: %x\n", client->properties.SingleReceiveBuffer);
	DBGPRINT(Handle->TeeHandle, "properties.MaxMessageLength: %d\n", client->properties.MaxMessageLength);
	DBGPRINT(Handle->TeeHandle, "properties.IsFixed: %d\n", client->properties.IsFixed);
	DBGPRINT(Handle->TeeHandle, "handle: %x\n", client->handle);
	DBGPRINT(Handle->TeeHandle, "initialized: %d\n", client->initialized);

	SetMem(Buffer, BufferSize, 0x0);
	status = heciReadMsg(Handle, BLOCKING, (UINT32 *)Buffer, BufferSize, &bytes_read);
	if (EFI_ERROR(status))
	{
		DBGPRINT(Handle->TeeHandle, "\nReadMsg: first reply read failed. bytesRead: %d. Status: %d\n", bytes_read, status);
		goto End;
	}

	// Currently the EFI HECI wrapper does not seem to differentiate between client responses
	// directed to the client connection, and flow control responses directed to the host driver.
	// Because of this limitation, we have to filter out flow control messages. This creates a
	// limitation on the HECI client responses. We have to be careful not to expect a message with
	// size HECI_FLOW_CTRL_MSG_LEN and response type FLOW_CONTROL_CMD.
	messageId = Buffer[0];
	if ((HECI_FLOW_CTRL_MSG_LEN == bytes_read) && (FLOW_CONTROL_CMD == messageId))
	{
		// The first reply message we get is flow control from FW.
		SetMem(Buffer, BufferSize, 0x0);
		status = heciReadMsg(Handle, BLOCKING, (UINT32 *)Buffer, BufferSize, &bytes_read);
		if (EFI_ERROR(status))
		{
			DBGPRINT(Handle->TeeHandle, "heciReadMsg. Status: %d\n");
			if (EFI_TIMEOUT == status)
			{
				DBGPRINT(Handle->TeeHandle, "Detected EFI_TIMEOUT.\n");
				gBS->Stall(HECI_TIMEOUT_STALL_SEC);

				SetMem(Buffer, BufferSize, 0x0);
				status = heciReadMsg(Handle, BLOCKING, (UINT32 *)Buffer, BufferSize, &bytes_read);
			}

			if (EFI_ERROR(status))
			{
				DBGPRINT(Handle->TeeHandle, "Retry failed. Status: %d\n", status);
				goto End;
			}
		}

		*NumOfBytesRead = bytes_read;

		status = EFI_SUCCESS;
		goto End;
	}

	*NumOfBytesRead = bytes_read;

	// Continue to recv flow control message
#define HECI1_DEVICE 0
	if (HECI1_DEVICE == Handle->HeciDevice)
	{
		DBGPRINT(Handle->TeeHandle, "HeciDevice: %d, IsFixed: %d\n", Handle->HeciDevice, client->properties.IsFixed);
		status = heciFwToHostFlowControl(Handle, client);
		DBGPRINT(Handle->TeeHandle, "heciFwToHostFlowControl. Status: %d\n");
		goto End;
	}

	status = EFI_SUCCESS;
End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}
