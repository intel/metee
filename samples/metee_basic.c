/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024-2026 Intel Corporation
 */
#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#define metee_basic_print(...) printf(__VA_ARGS__)
#define metee_basic_err(...) fprintf(stderr, __VA_ARGS__)
#define metee_basic_malloc(size) malloc(size)
#define metee_basic_free(ptr) free(ptr)
#define metee_basic_sleep(ms) Sleep(ms)
#elif __linux__
#include <unistd.h>
#define metee_basic_print(...) printf(__VA_ARGS__)
#define metee_basic_err(...) fprintf(stderr, __VA_ARGS__)
#define metee_basic_malloc(size) malloc(size)
#define metee_basic_free(ptr) free(ptr)
#define metee_basic_sleep(ms) usleep((ms)*1000)
#elif defined(EFI)
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#define metee_basic_print(...) AsciiPrint(__VA_ARGS__)
#define metee_basic_err(...) AsciiPrint(__VA_ARGS__)
#define metee_basic_malloc(size) AllocatePool(size)
#define metee_basic_free(ptr) FreePool(ptr)
#define metee_basic_sleep(ms) gBS->Stall((ms)*1000)
#endif

#include <metee.h>

DEFINE_GUID(MEI_MKHIF, 0x8e6a6715, 0x9abc, 0x4043,
    0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0xf);

#define MKHI_TIMEOUT 10000
#define CONNECT_RETRIES 3

#pragma pack(1)
struct mkhi_msg_hdr {
	union {
		uint32_t data;
		struct {
			uint32_t  GroupId : 8;
			uint32_t  Command : 7;
			uint32_t  IsResponse : 1;
			uint32_t  Reserved : 8;
			uint32_t  Result : 8;
		};
	};
};

struct mkhi_fwver_req {
	struct mkhi_msg_hdr header;
};

struct mkhi_fw_version_block {
	uint16_t minor;
	uint16_t major;
	uint16_t buildNo;
	uint16_t hotFix;
};

struct mkhi_fw_version {
	struct mkhi_fw_version_block code;
	struct mkhi_fw_version_block NFTP;
	struct mkhi_fw_version_block FITC;
};

struct mkhi_fwver_rsp {
	struct mkhi_msg_hdr header;
	struct mkhi_fw_version version;
};
#pragma pack()

int main(int argc, char* argv[])
{
	TEEHANDLE handle;
	TEESTATUS status;
	struct tee_device_address addr = {
		.type = TEE_DEVICE_TYPE_NONE,
		.data.path = NULL
	};

	int retry = CONNECT_RETRIES;
	size_t written = 0;
	struct mkhi_fwver_req req;
	uint8_t *read_buf = NULL;
	struct mkhi_fwver_rsp* rsp;
	char kind[32];
	size_t kind_size = sizeof(kind);

	status = TeeInitFull(&handle, &MEI_MKHIF, addr, TEE_LOG_LEVEL_VERBOSE, NULL);
	if (!TEE_IS_SUCCESS(status)) {
		metee_basic_err("TeeInitFull failed with status = %u\n", status);
		return 1;
	}

	status = TeeGetKind(&handle, kind, &kind_size);
	if (!TEE_IS_SUCCESS(status)) {
		metee_basic_err("TeeGetKind failed with status = %u\n", status);
	} else {
		metee_basic_print("Tee device kind is %s\n", kind);
	}
	
	while (retry--) {
		status = TeeConnect(&handle);
		if (status != TEE_BUSY &&
			status != TEE_UNABLE_TO_COMPLETE_OPERATION) /* windows return this error on busy */
			break;
		metee_basic_err("Client is busy, retrying\n");
		metee_basic_sleep(2000);
	}
	switch (status) {
	case TEE_SUCCESS:
		break;
	case TEE_CLIENT_NOT_FOUND:
		metee_basic_err("TeeConnect failed with status = %u (Client not found)\n", status);
		goto out;
	default:
		metee_basic_err("TeeConnect failed with status = %u\n", status);
		goto out;
	}

	if (TeeGetMaxMsgLen(&handle) == 0)
	{
		metee_basic_err("Client reported zero MTU. Aborting.\n");
		goto out;
	}

	/* Write */
	req.header.data = 0; /* Reset */
	req.header.GroupId = 0xFF; /* MKHI */
	req.header.Command = 0x02; /* GET FW Version */
	req.header.IsResponse = 0;
	req.header.Reserved = 0;

	status = TeeWrite(&handle, &req, sizeof(req), &written, MKHI_TIMEOUT);
	if (!TEE_IS_SUCCESS(status)) {
		metee_basic_err("TeeWrite failed with status = %u\n", status);
		goto out;
	}
	if (written != sizeof(req)) {
		metee_basic_err("TeeWrite failed written = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	/* Read */
	read_buf = (uint8_t*)metee_basic_malloc(TeeGetMaxMsgLen(&handle));
	if (!read_buf) {
		metee_basic_err("malloc failed\n");
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	status = TeeRead(&handle, read_buf, TeeGetMaxMsgLen(&handle), &written, MKHI_TIMEOUT);
	if (!TEE_IS_SUCCESS(status)) {
		metee_basic_err("TeeRead failed with status = %u\n", status);
		goto out;
	}

	rsp = (struct mkhi_fwver_rsp*)read_buf;

	if (written < sizeof(struct mkhi_msg_hdr)) {
		metee_basic_err("Returned less than header = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	if (written < sizeof(struct mkhi_fwver_rsp)) {
		metee_basic_err("Returned less than response = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	if (rsp->header.Result) {
		metee_basic_err("Result = %u\n", rsp->header.Result);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	metee_basic_print("Version: %u.%u.%u.%u\n",
		rsp->version.code.major, rsp->version.code.minor, rsp->version.code.hotFix, rsp->version.code.buildNo);

out:
	TeeDisconnect(&handle);
	if (read_buf)
		metee_basic_free(read_buf);
	return TEE_IS_SUCCESS(status) ? 0 : 1;
}
