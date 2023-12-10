/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2023 Intel Corporation
 */
#include <stdio.h>

#include <metee.h>

DEFINE_GUID(GUID_DEVINTERFACE_NULL, 0x00000000, 0x0000, 0x0000,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

int main(int argc, char* argv[])
{
	TEEHANDLE handle;
	TEESTATUS status;
	uint32_t trc_val = 0;
	struct tee_device_address addr = {
	.type = TEE_DEVICE_TYPE_NONE,
	.data.path = NULL
	};

	status = TeeInitFull(&handle, &GUID_DEVINTERFACE_NULL, addr, TEE_LOG_LEVEL_VERBOSE, NULL);
	if (!TEE_IS_SUCCESS(status)) {
		fprintf(stderr, "init failed with status = %u\n", status);
		return 1;
	}

	status = TeeGetTRC(&handle, &trc_val);
	if (!TEE_IS_SUCCESS(status)) {
		fprintf(stderr, "TeeGetTRC failed with status = %u\n", status);
	}
	else {
		fprintf(stdout, "TRC = 0x%08X\n", trc_val);
	}

	TeeDisconnect(&handle);
	return 0;
}