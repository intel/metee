/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2024 Intel Corporation
 */
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#endif /* __linux__ */

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

	status = TeeInitFull(&handle, &MEI_MKHIF, addr, TEE_LOG_LEVEL_VERBOSE, NULL);
	if (!TEE_IS_SUCCESS(status)) {
		fprintf(stderr, "TeeInitFull failed with status = %u\n", status);
		return 1;
	}

	while (retry--) {
		status = TeeConnect(&handle);
		if (status != TEE_BUSY &&
			status != TEE_UNABLE_TO_COMPLETE_OPERATION) /* windows return this error on busy */
			break;
		fprintf(stderr, "Client is busy, retrying\n");
#ifdef WIN32
		Sleep(2000);
#else
		sleep(2);
#endif /* WIN32 */
	}
	switch (status) {
	case TEE_SUCCESS:
		break;
	case TEE_CLIENT_NOT_FOUND:
		fprintf(stderr, "TeeConnect failed with status = %u (Client not found)\n", status);
		goto out;
	default:
		fprintf(stderr, "TeeConnect failed with status = %u\n", status);
		goto out;
	}

	if (TeeGetMaxMsgLen(&handle) == 0)
	{
		fprintf(stderr, "Client reported zero MTU. Aborting.\n");
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
		fprintf(stderr, "TeeWrite failed with status = %u\n", status);
		goto out;
	}
	if (written != sizeof(req)) {
		fprintf(stderr, "TeeWrite failed written = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	/* Read */
	read_buf = (uint8_t*)malloc(TeeGetMaxMsgLen(&handle));
	if (!read_buf) {
		fprintf(stderr, "malloc failed\n");
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	status = TeeRead(&handle, read_buf, TeeGetMaxMsgLen(&handle), &written, MKHI_TIMEOUT);
	if (!TEE_IS_SUCCESS(status)) {
		fprintf(stderr, "TeeWrite failed with status = %u\n", status);
		goto out;
	}

	rsp = (struct mkhi_fwver_rsp*)read_buf;

	if (written < sizeof(struct mkhi_msg_hdr)) {
		fprintf(stderr, "Returned less then header = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	if (written < sizeof(struct mkhi_fwver_rsp)) {
		fprintf(stderr, "Returned less then response = %zu\n", written);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	if (rsp->header.Result) {
		fprintf(stderr, "Result = %u\n", rsp->header.Result);
		status = TEE_INTERNAL_ERROR;
		goto out;
	}

	printf("Version: %u.%u.%u.%u\n",
		rsp->version.code.major, rsp->version.code.minor, rsp->version.code.hotFix, rsp->version.code.buildNo);

out:
	TeeDisconnect(&handle);
	if (read_buf)
		free(read_buf);
	return TEE_IS_SUCCESS(status) ? 0 : 1;
}
