/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

#include "metee.h"
#include "helpers.h"

#include "metee_efi.h"
#include "heci_core.h"
#include "pci_utils.h"



#pragma pack(1)

///
/// H_CSR     - Host Control Status Register, default offset 04h
/// ME_CSR_HA - ME Control Status Register (Host Access), default offset 0Ch
///
typedef union
{
	UINT32 Data;
	struct
	{
		UINT32 IntEnable : 1;	   ///< 0 - Host/ME Interrupt Enable
		UINT32 IntStatus : 1;	   ///< 1 - Host/ME Interrupt Status
		UINT32 IntGenerate : 1;	   ///< 2 - Host/ME Interrupt Generate
		UINT32 Ready : 1;		   ///< 3 - Host/ME Ready
		UINT32 Reset : 1;		   ///< 4 - Host/ME Reset
		UINT32 Reserved : 3;	   ///< 7:5 - Reserved
		UINT32 CBReadPointer : 8;  ///< 15:8 - Host/ME Circular Buffer Read Pointer
		UINT32 CBWritePointer : 8; ///< 23:16 - Host/ME Circular Buffer Write Pointer
		UINT32 CBDepth : 8;		   ///< 31:24 - Host/ME Circular Buffer Depth
	} Fields;
} HECI_CONTROL_STATUS_REGISTER;

typedef union
{
	UINT32 Data;
	struct
	{
		/**
		  This is the logical address of the Intel ME client of the message. This address is assigned
		  during ME firmware initialization.
		**/
		UINT32 FwAddress : 8;
		/**
		  This is the logical address of the Host client of the message. This address is assigned
		  when the host client registers itself with the Host MEI driver.
		**/
		UINT32 HostAddress : 8;
		/**
		  This is the message length in bytes, not including the MEI_MESSAGE_HEADER. A value of 0
		  indicates no additional bytes are part of the message.
		**/
		UINT32 Length : 9;
		UINT32 Reserved : 6;
		UINT32 MessageComplete : 1; ///< Indicates the last message of the list
	} Fields;
} HECI_MESSAGE_HEADER;

#pragma pack()


//
// Timeout values based on HPET
//
#define HECI_INIT_TIMEOUT 15000000	///< 15sec timeout in microseconds
#define HECI_READ_TIMEOUT 5000000	///< 5sec timeout in microseconds
#define HECI_SEND_TIMEOUT 5000000	///< 5sec timeout in microseconds
#define HECI_CB_OVERFLOW 0xFFFFFFFF ///< Circular buffer overflow

/**
  Return number of filled slots in HECI circular buffer.

  @param[in] ControlStatusRegister        FW Control Status Register

  @retval FilledSlots      Number of filled slots in circular buffer
  @retval HECI_CB_OVERFLOW Circular buffer overflow has occurred
**/
static UINT32
GetFilledSlots(
	IN HECI_CONTROL_STATUS_REGISTER ControlStatusRegister)
{
	UINT8 FilledSlots;

	FilledSlots = (INT8)ControlStatusRegister.Fields.CBWritePointer - (INT8)ControlStatusRegister.Fields.CBReadPointer;

	return (FilledSlots > (UINT8)ControlStatusRegister.Fields.CBDepth) ? HECI_CB_OVERFLOW : FilledSlots;
}

#define DelayExecutionFlow gBS->Stall

#define TIMER_STEP 1000

/**
  Checks if FW is ready for communication over the HECI interface.

  @param[in] Handle     The HECI Handle to be accessed.
  @param[in] HeciMemBar HECI Memory BAR
  @param[in] Timeout    Timeout value in microseconds

  @retval TRUE   FW is ready
  @retval FALSE  FW is not ready
**/
static BOOLEAN
IsMeReady(
	IN struct METEE_EFI_IMPL *Handle,
	IN UINTN HeciMemBar,
	IN UINT32 Timeout)
{
	HECI_CONTROL_STATUS_REGISTER HeciCsrMeHra;

	while (Timeout > TIMER_STEP)
	{
		HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
		///
		/// Check for reset first and then for FW Ready
		///
		if (HeciCsrMeHra.Fields.Reset)
		{
			return FALSE;
		}
		else if (HeciCsrMeHra.Fields.Ready)
		{
			return TRUE;
		}
		DelayExecutionFlow(TIMER_STEP);
		Timeout -= TIMER_STEP;
	}
	DBGPRINT(Handle->TeeHandle, "HECI Interface ERROR: Timeout due to ME_RDY bit not set after %d seconds\n", Timeout);
	return FALSE;
}

/**
  Function forces a reinit of the heci interface by following the reset heci interface via host algorithm
  in HPS section 4.1.1.1

  @param[in] Handle               The HECI Handle to be accessed.

  @retval EFI_SUCCESS             Interface reset successful
  @retval EFI_TIMEOUT             ME is not ready
**/
EFI_STATUS
EFIAPI
ResetHeciInterface(
	IN struct METEE_EFI_IMPL *Handle)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HECI_CONTROL_STATUS_REGISTER HeciCsrHost;
	HECI_CONTROL_STATUS_REGISTER HeciCsrMeHra;
	UINT32 Timeout = HECI_INIT_TIMEOUT;
	UINT64 HeciMemBar;

	FUNC_ENTRY(Handle->TeeHandle);

	EFIPRINT(Handle->TeeHandle, "Resetting interface\n");

	///
	/// Make sure that HECI device BAR is correct and device is enabled.
	///
	HeciMemBar = CheckAndFixHeciForAccess(Handle);
	if (HeciMemBar == 0)
	{
		EFIPRINT(Handle->TeeHandle, "HeciMemBar = 0\n");
		status = EFI_DEVICE_ERROR;
		goto End;
	}

	///
	/// Enable Reset
	///
	HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	EFIPRINT(Handle->TeeHandle, "Step1 Enable Host Reset : H_CSR = 0x%08X\n", HeciCsrHost.Data);

	if (!HeciCsrHost.Fields.Reset)
	{
		HeciCsrHost.Fields.Reset = 1;
		HeciCsrHost.Fields.IntGenerate = 1;
		MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR, HeciCsrHost.Data);
	}

	///
	/// Make sure that the reset started
	///
	EFIPRINT(Handle->TeeHandle, "Step2 Wait for reset started: H_CSR = 0x%08X\n", HeciCsrHost.Data);

	do
	{
		if (Timeout < TIMER_STEP)
		{
			EFIPRINT(Handle->TeeHandle, "IsHeciTimeout");
			status = EFI_TIMEOUT;
			goto End;
		}
		EFIPRINT(Handle->TeeHandle, "Wait iteration Timeout left %d, TimerStep %d\n", Timeout, TIMER_STEP);
		DelayExecutionFlow(TIMER_STEP);
		Timeout -= TIMER_STEP;
		HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
		EFIPRINT(Handle->TeeHandle, "Value after wait: H_CSR = 0x%08X\n", HeciCsrHost.Data);
	} while (HeciCsrHost.Fields.Ready == 1);

	///
	/// Wait for ME to perform reset
	///
	HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	EFIPRINT(Handle->TeeHandle, "Step3 Wait for ME reset: ME_CSR_HA = 0x%08X\n", HeciCsrMeHra.Data);
	do
	{
		if (Timeout < TIMER_STEP)
		{
			EFIPRINT(Handle->TeeHandle, "Step3  ME has failed to reset Heci: ME_CSR_HA = 0x%08X\n", HeciCsrMeHra.Data);
			status = EFI_TIMEOUT;
			goto End;
		}
		DelayExecutionFlow(TIMER_STEP);
		Timeout -= TIMER_STEP;
		HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	} while (HeciCsrMeHra.Fields.Ready == 0);

	///
	/// Enable host side interface. Host SW writes the value read back to the H_CSR register.
	/// This clears the H_IS and bit in H_CSR to 0
	///
	HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	HeciCsrHost.Fields.Reset = 0;
	HeciCsrHost.Fields.IntGenerate = 1;
	HeciCsrHost.Fields.Ready = 1;
	MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR, HeciCsrHost.Data);
	status = EFI_SUCCESS;
End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

/**
  Function to pull one message packet off the HECI circular buffer up to its capacity.
  Corresponds to HECI HPS (part of) section 4.2.4.
  BIOS does not rely on Interrupt Status bit, since this bit can be set due to several reasons:
	a) FW has finished reading data from H_CSR
	b) FW has finished writing data to ME_CSR
	c) Reset has occurred
  Because of above - additional checks must be conducted in order to prevent misinterpretations.

  @param[in] Handle               The HECI Handle to be accessed.
  @param[in] HeciMemBar           HECI Memory BAR.
  @param[in] Timeout              Timeout value for first Dword to appear in circular buffer.
  @param[out] MessageHeader       Pointer to message header buffer.
  @param[in] MessageData          Pointer to receive buffer.
  @param[in, out] BufferLength    On input is the size of the caller's buffer in bytes.
								  On output is the size of the data copied to the buffer.
  @param[out] PacketSize          Size of the packet in bytes. This might be greater than buffer size.

  @retval EFI_SUCCESS             One message packet read.
  @retval EFI_DEVICE_ERROR        The circular buffer is overflowed or transaction error.
  @retval EFI_NO_RESPONSE         The circular buffer is empty
  @retval EFI_TIMEOUT             Failed to receive a full message on time
  @retval EFI_BUFFER_TOO_SMALL    Message packet is larger than caller's buffer
**/
static EFI_STATUS
HeciPacketRead(
	IN struct METEE_EFI_IMPL *Handle,
	IN UINTN HeciMemBar,
	IN UINT32 Timeout,
	OUT HECI_MESSAGE_HEADER *MessageHeader,
	OUT UINT32 *MessageData,
	IN OUT UINT32 *BufferLength,
	OUT UINT32 *PacketSize)
{
	EFI_STATUS Status;
	UINT32 Index;
	UINT32 FilledSlots;
	UINT32 LengthInDwords;
	UINT32 TempBuffer;
	UINT32 ByteCount;
	HECI_CONTROL_STATUS_REGISTER HeciCsrMeHra;
	HECI_CONTROL_STATUS_REGISTER HeciCsrHost;

	*PacketSize = 0;
	Status = EFI_SUCCESS;

	HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	FilledSlots = GetFilledSlots(HeciCsrMeHra);

	///
	/// For BLOCKING read, wait until data appears in the CB or timeout occurs
	///
	if (Timeout != 0)
	{
		while (FilledSlots == 0)
		{
			if (Timeout < TIMER_STEP)
			{
				return EFI_TIMEOUT;
			}
			DelayExecutionFlow(TIMER_STEP);
			Timeout -= TIMER_STEP;
			HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
			FilledSlots = GetFilledSlots(HeciCsrMeHra);
		}
	}

	///
	/// Check for empty and overflowed CB
	///
	if (FilledSlots == 0)
	{
		*BufferLength = 0;
		return EFI_NO_RESPONSE;
	}
	else if (FilledSlots == HECI_CB_OVERFLOW)
	{
		*BufferLength = 0;
		return EFI_DEVICE_ERROR;
	}

	///
	/// Eat the HECI Message header
	///
	MessageHeader->Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CB_RW);
	///
	/// Compute required message length in DWORDS
	///
	LengthInDwords = ((MessageHeader->Fields.Length + 3) / 4);

	if (LengthInDwords > HeciCsrMeHra.Fields.CBDepth)
	{
		///
		/// Make sure that the message does not overflow the circular buffer.
		///
		*BufferLength = 0;
		return EFI_DEVICE_ERROR;
	}

	///
	/// Wait until whole message appears in circular buffer.
	///
	Timeout = HECI_READ_TIMEOUT;

	do
	{
		HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
		if (Timeout < TIMER_STEP)
		{
			return EFI_TIMEOUT;
		}
		DelayExecutionFlow(TIMER_STEP);
		Timeout -= TIMER_STEP;
	} while (GetFilledSlots(HeciCsrMeHra) < LengthInDwords);

	///
	/// Update status to signal if buffer can hold the message
	///
	if ((MessageHeader->Fields.Length) <= *BufferLength)
	{
		Status = EFI_SUCCESS;
	}
	else
	{
		Status = EFI_BUFFER_TOO_SMALL;
	}
	///
	/// Copy as much bytes as there is space left in the message buffer.
	/// Excessive bytes will be dismissed.
	///
	ByteCount = 0;
	for (Index = 0; Index < LengthInDwords; Index++)
	{
		TempBuffer = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CB_RW);
		CopyMem(&MessageData[Index], &TempBuffer, MIN(sizeof(TempBuffer), *BufferLength));
		ByteCount += MIN(sizeof(TempBuffer), *BufferLength);
		*BufferLength -= MIN(sizeof(TempBuffer), *BufferLength);
	}
	*BufferLength = ByteCount;
	*PacketSize = MessageHeader->Fields.Length;

	///
	/// Read Handle->Hw.RegisterOffset.ME_CSR_HA. If the ME_RDY bit is 0, then an ME reset occurred during the
	/// transaction and the message should be discarded as bad data may have been retrieved
	/// from the host's circular buffer
	///
	HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	if (HeciCsrMeHra.Fields.Ready == 0)
	{
		*BufferLength = 0;
		*PacketSize = 0;
		return EFI_NOT_READY;
	}

	///
	/// Set Interrupt Generate bit and clear Interrupt Status bit if there are no more messages in the buffer.
	///
	HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	if (GetFilledSlots(HeciCsrMeHra) == 0)
	{
		HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
		HeciCsrHost.Fields.IntGenerate = 1;
		HeciCsrHost.Fields.IntStatus = 1;
		MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR, HeciCsrHost.Data);
		HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	}

	return Status;
}

/**
  Reads a message from FW through HECI.

  @param[in] Handle        		  The HECI Handle to be accessed.
  @param[in] Blocking             Used to determine if the read is BLOCKING or NON_BLOCKING.
  @param[in, out] MessageBody     Pointer to a buffer used to receive a message.
  @param[in, out] Length          Pointer to the length of the buffer on input and the length
								  of the message on return. (in bytes)

  @retval EFI_SUCCESS             One message packet read.
  @retval EFI_DEVICE_ERROR        Failed to initialize HECI
  @retval EFI_NOT_READY           HECI is not ready for communication
  @retval EFI_TIMEOUT             Failed to receive a full message on time
  @retval EFI_NO_RESPONSE         No response from FW
  @retval EFI_BUFFER_TOO_SMALL    The caller's buffer was not large enough
**/
EFI_STATUS
EFIAPI
HeciReceive(
	IN struct METEE_EFI_IMPL *Handle,
	IN UINT32 Blocking,
	IN OUT UINT32 *MessageBody,
	IN OUT UINT32 *Length)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	UINT64 HeciMemBar;
	HECI_MESSAGE_HEADER PacketHeader;
	UINT32 TotalLength;
	UINT32 PacketSize;
	UINT32 PacketBuffer;
	UINT32 ReadTimeout;
	UINT32 InitTimeout;
	UINT32 InputLength;
	BOOLEAN FirstPacket = TRUE;

	InputLength = *Length;

	PacketHeader.Data = 0;
	TotalLength = 0;
	PacketBuffer = InputLength;

	FUNC_ENTRY(Handle->TeeHandle);

	///
	/// Make sure that HECI device BAR is correct and device is enabled.
	///
	HeciMemBar = CheckAndFixHeciForAccess(Handle);
	if (HeciMemBar == 0)
	{
		EFIPRINT(Handle->TeeHandle, "HeciMemBar = 0\n");
		status = EFI_DEVICE_ERROR;
		goto End;
	}

	if (Blocking)
	{
		InitTimeout = HECI_INIT_TIMEOUT;
		ReadTimeout = HECI_READ_TIMEOUT;
	}
	else
	{
		InitTimeout = 0;
		ReadTimeout = 0;
	}

	if (!IsMeReady(Handle, HeciMemBar, InitTimeout))
	{
		//
		// Return as CB will be empty after reset and FW will not put any data
		//
		EFIPRINT(Handle->TeeHandle, "FW Not Ready\n");
		status = EFI_ABORTED;
		goto End;
	}

	//
	// Read until MessageComplete bit is set
	//
	while (!PacketHeader.Fields.MessageComplete)
	{
		status = HeciPacketRead(
			Handle,
			HeciMemBar,
			ReadTimeout,
			&PacketHeader,
			(UINT32 *)&MessageBody[TotalLength / 4],
			&PacketBuffer,
			&PacketSize);

		if (status != EFI_NO_RESPONSE)
		{
			EFIPRINT(Handle->TeeHandle, "Got msg: Status %d, Data %08X\n", status, PacketHeader.Data);
		};
		if (FirstPacket) 
		{
			
			EFIPRINT(Handle->TeeHandle, "HeciReceive: Host Client Id: %d FW Client Id: %d\n", 
				PacketHeader.Fields.HostAddress, PacketHeader.Fields.FwAddress);
			FirstPacket = FALSE;
		}
		///
		/// If timeout occurred we need to reset the interface to clear the data that could possibly come later.
		/// Also buffer overflow and transaction errors will require a reset.
		/// We need to continue read even if buffer too small to clear the data and signal the buffer size.
		///
		if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL)
		{
			if (status != EFI_NO_RESPONSE)
			{
				EFIPRINT(Handle->TeeHandle, "FW Not Ready\n");
				status = EFI_ABORTED;
				goto End;
			}
			*Length = TotalLength;
			if (!(status == EFI_NO_RESPONSE && Blocking == NON_BLOCKING))
			{
				//
				// NO_RESPONSE is expected for NON_BLOCKING ReadMsg.
				//
				EFIPRINT(Handle->TeeHandle, "NO_RESPONSE is expected for NON_BLOCKING ReadMsg.\n");
			}
			goto End;
		}
		///
		/// Track the length of what we have read so far
		///
		TotalLength += PacketSize;
		///
		/// To do message size calculations we are using 3 different variables:
		/// PacketBuffer - on input -> space left in the buffer, on output -> number of bytes written to buffer
		/// Length       - max buffer size
		/// TotalLength  - Total message size -> sum of all bytes in multiple packets
		///
		if (TotalLength < *Length)
		{
			PacketBuffer = *Length - TotalLength;
		}
		else
		{
			PacketBuffer = 0;
		}
		///
		/// If this was a NON-BLOCKING message and it is a multipacket message, we need to change the
		/// parameter to BLOCKING because it will take a non-zero value of time until a new packet appears
		///
		ReadTimeout = HECI_READ_TIMEOUT;
	}
	*Length = TotalLength;
	status = EFI_SUCCESS;

End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}

/**
  Function sends one message packet through the HECI circular buffer
  Corresponds to HECI HPS (part of) section 4.2.3

  @param[in] HeciMemBar           HECI Memory BAR.
  @param[in] MessageHeader        Pointer to the message header.
  @param[in] MessageData          Pointer to the actual message data.

  @retval EFI_SUCCESS             One message packet sent
  @retval EFI_ABORTED             Fatal error has occurred during transmission of the message
  @retval EFI_DEVICE_ERROR        Unrecoverable fatal error has occurred during transmission of the message
  @retval EFI_TIMEOUT             CSME failed to empty the circular buffer
  @retval EFI_NOT_READY           Fatal error has occurred during transmission of the message
**/
static EFI_STATUS
HeciPacketWrite(
	IN struct METEE_EFI_IMPL *Handle,
	IN UINTN HeciMemBar,
	IN HECI_MESSAGE_HEADER *MessageHeader,
	IN UINT32 *MessageData)
{
	UINT32 Timeout = HECI_SEND_TIMEOUT;
	UINT32 Index;
	UINT32 LengthInDwords;
	HECI_CONTROL_STATUS_REGISTER HeciCsrHost;
	HECI_CONTROL_STATUS_REGISTER HeciCsrMeHra;

	///
	/// Compute message length in DWORDS
	///
	LengthInDwords = ((MessageHeader->Fields.Length + 3) / 4);

	///
	/// Wait until there is sufficient room in the circular buffer
	/// Must have room for message and message header
	///
	HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	while ((LengthInDwords + 1) > (HeciCsrHost.Fields.CBDepth - GetFilledSlots(HeciCsrHost)))
	{
		if (Timeout < TIMER_STEP)
		{
			DBGPRINT(Handle->TeeHandle, "EFI_TIMEOUT due to circular buffer never emptied after %d seconds waiting", HECI_SEND_TIMEOUT);
			return EFI_TIMEOUT;
		}
		DelayExecutionFlow(TIMER_STEP);
		Timeout -= TIMER_STEP;
		HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	}
	///
	/// Write Message Header
	///
	MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CB_WW, MessageHeader->Data);

	///
	/// Write Message Body
	///
	for (Index = 0; Index < LengthInDwords; Index++)
	{
		MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CB_WW, MessageData[Index]);
	}
	///
	/// Set Interrupt Generate bit
	///
	HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);
	HeciCsrHost.Fields.IntGenerate = 1;
	MmioWrite32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR, HeciCsrHost.Data);

	///
	/// Test if FW Ready bit is set to 1, if set to 0 a fatal error occurred during
	/// the transmission of this message.
	///
	HeciCsrMeHra.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.ME_CSR_HA);
	if (HeciCsrMeHra.Fields.Ready == 0)
	{
		return EFI_NOT_READY;
	}

	return EFI_SUCCESS;
}

/**
  Function sends one message (of any length) through the HECI circular buffer.

@param[in] Handle        		  The HECI Handle to be accessed.
  @param[in] Message              Pointer to the message data to be sent.
  @param[in] Length               Length of the message in bytes.
  @param[in] HostAddress          The address of the host processor.
  @param[in] FwAddress            Address of the FW subsystem the message is being sent to.

  @retval EFI_SUCCESS             One message packet sent.
  @retval EFI_DEVICE_ERROR        Failed to initialize HECI
  @retval EFI_NOT_READY           HECI is not ready for communication
  @retval EFI_TIMEOUT             FW failed to empty the circular buffer
  @retval EFI_UNSUPPORTED         Current FW mode doesn't support send this message through this HECI
**/
EFI_STATUS
EFIAPI
HeciSend(
	IN struct METEE_EFI_IMPL *Handle,
	IN UINT32 *Message,
	IN UINT32 Length,
	IN UINT8 HostAddress,
	IN UINT8 FwAddress)
{
	EFI_STATUS status = EFI_UNSUPPORTED;
	HECI_MESSAGE_HEADER MessageHeader;
	HECI_CONTROL_STATUS_REGISTER HeciCsrHost;
	UINT32 CircularBufferDepth;
	UINT32 SendLength;
	UINT32 BytesLeft;
	UINT64 HeciMemBar;

	EFIPRINT(Handle->TeeHandle, "HeciSend: Host Client Id: %d FW Client Id: %d\n", HostAddress, FwAddress);

	///
	/// Make sure that HECI device BAR is correct and device is enabled.
	///
	HeciMemBar = CheckAndFixHeciForAccess(Handle);
	if (HeciMemBar == 0)
	{
		EFIPRINT(Handle->TeeHandle, "HeciMemBar = 0\n");
		status = EFI_DEVICE_ERROR;
		goto End;
	}

	///
	/// Make sure that HECI is ready for communication
	///
	if (!IsMeReady(Handle, HeciMemBar, HECI_INIT_TIMEOUT))
	{
		EFIPRINT(Handle->TeeHandle, "FW Not Ready\n");
		status = EFI_ABORTED;
		goto End;
	}
	///
	/// Set up memory mapped registers
	///
	HeciCsrHost.Data = MmioRead32(HeciMemBar + Handle->Hw.RegisterOffset.H_CSR);

	///
	/// Grab Circular Buffer length and convert it from Dword to bytes
	///
	CircularBufferDepth = 4 * HeciCsrHost.Fields.CBDepth;

	///
	/// Prepare message header
	///
	MessageHeader.Data = 0;
	MessageHeader.Fields.FwAddress = FwAddress;
	MessageHeader.Fields.HostAddress = HostAddress;

	BytesLeft = Length;
	///
	/// Break message up into CB-sized packets and loop until completely sent
	///
	while (BytesLeft)
	{
		///
		/// Set the Message Complete bit if this is our last packet in the message.
		/// Needs to be less or equal to CB depth minus one Dword for HECI header.
		///
		if (BytesLeft <= CircularBufferDepth - sizeof(MessageHeader))
		{
			MessageHeader.Fields.MessageComplete = 1;
		}
		///
		/// Calculate length for Message Header:
		/// It will be the smaller value of circular buffer or remaining message.
		///
		SendLength = MIN(CircularBufferDepth - sizeof(MessageHeader), BytesLeft);
		MessageHeader.Fields.Length = SendLength;
		///
		/// Send the current packet
		///
		EFIPRINT(Handle->TeeHandle, "Send msg Data: %08X\n", MessageHeader.Data);

		status = HeciPacketWrite(Handle, HeciMemBar, &MessageHeader, (UINT32 *)((UINTN)Message + (Length - BytesLeft)));
		if (EFI_ERROR(status))
		{
			EFIPRINT(Handle->TeeHandle, "FW Not Ready\n");
			status = EFI_ABORTED;
			goto End;
		}
		///
		/// Update the length information
		///
		BytesLeft -= SendLength;
	}

End:
	FUNC_EXIT(Handle->TeeHandle, status);
	return status;
}
