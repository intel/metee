/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2020-2024 Intel Corporation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <metee.h>

#ifndef BIT
#define BIT(n) 1 << (n)
#endif /* BIT */

#include <stdbool.h>
#ifdef _WIN32
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif /* _WIN32 */

#pragma pack(1)
struct gsc_fwu_heci_header {
	uint8_t command_id;
	uint8_t is_response :1;
	uint8_t reserved    :7;
	uint8_t reserved2[2];
};

struct gsc_fwu_heci_version_req {
	struct gsc_fwu_heci_header header;
	uint32_t                   partition;
};

struct gsc_fwu_heci_response {
	struct gsc_fwu_heci_header header;
	uint32_t                   status;
	uint32_t                   reserved;
};

struct gsc_fwu_heci_version_resp {
	struct gsc_fwu_heci_response response;
	uint32_t                     partition;
	uint32_t                     version_length;
	uint8_t                      version[];
};

struct gsc_fwu_external_version {
	char     project[4];
	uint16_t hotfix;
	uint16_t build;
};
#pragma pack()

#define GSC_FWU_HECI_COMMAND_ID_GET_IP_VERSION 6

enum gsc_fwu_heci_payload_type {
	gsc_fwu_heci_payload_type_invalid    = 0, /**< lower sentinel */
	gsc_fwu_heci_payload_type_gfx_fw     = 1, /**< graphics firmware */
	gsc_fwu_heci_payload_type_oprom_data = 2, /**< oprom data partition */
	gsc_fwu_heci_payload_type_oprom_code = 3, /**< oprom code partition */
};

/** GSC firmware update status SUCCESS */
#define GSC_FWU_STATUS_SUCCESS                        0x0
/** GSC firmware update status size error */
#define GSC_FWU_STATUS_SIZE_ERROR                     0x5
/** GSC Update oprom section does not exists error */
#define GSC_FWU_STATUS_INVALID_COMMAND                0x8D
/** GSC firmware update status invalid param error */
#define GSC_FWU_STATUS_INVALID_PARAMS                 0x85
/** GSC firmware update general failure */
#define  GSC_FWU_STATUS_FAILURE                       0x9E

#define MKHI_READ_TIMEOUT 10000

const char *mkhi_status(uint32_t status)
{
#define MKH_STATUS(_x) case _x: return #_x
	switch(status) {
		MKH_STATUS(GSC_FWU_STATUS_SUCCESS);
		MKH_STATUS(GSC_FWU_STATUS_SIZE_ERROR);
		MKH_STATUS(GSC_FWU_STATUS_INVALID_COMMAND);
		MKH_STATUS(GSC_FWU_STATUS_INVALID_PARAMS);
		MKH_STATUS(GSC_FWU_STATUS_FAILURE);
	default:
		fprintf(stderr, "unknown 0x%08X\n", status);
		return "unknown";
	}
#undef MKH_STATUS
}

DEFINE_GUID(GUID_DEVINTERFACE_HECI_GSC_CHILD,
            0x5315db55, 0xe7c7, 0x4e67,
            0xb3, 0x96, 0x80, 0xa, 0x75, 0xdd, 0x6f, 0xe4);
DEFINE_GUID(GUID_METEE_FWU, 0x87d90ca5, 0x3495, 0x4559,
            0x81, 0x05, 0x3f, 0xbf, 0xa3, 0x7b, 0x8b, 0x79);


struct mk_host_if {
	TEEHANDLE mei_cl;
	bool initialized;
	bool reconnect;
	bool verbose;
};

static bool mk_host_if_connect(struct mk_host_if *acmd)
{
	acmd->initialized = (TeeConnect(&acmd->mei_cl) == 0);
	return acmd->initialized;
}

static bool mk_host_if_init(struct mk_host_if *acmd, const GUID *guid,
                            bool reconnect, bool verbose)
{
	acmd->reconnect = reconnect;
	acmd->verbose = verbose;
#ifdef WIN32
	TeeInitGUID(&acmd->mei_cl, guid, &GUID_DEVINTERFACE_HECI_GSC_CHILD);
#else
	TeeInit(&acmd->mei_cl, guid, NULL);
#endif /* WIN32 */
	return mk_host_if_connect(acmd);
}

static void mk_host_if_deinit(struct mk_host_if *acmd)
{
	TeeDisconnect(&acmd->mei_cl);
}

static uint32_t mkhi_verify_response_header(struct gsc_fwu_heci_header *msg, struct gsc_fwu_heci_header *resp)
{
	bool match = true;
	if (msg->command_id != resp->command_id) {
		printf("Mismatch Command; Req-Command = %d , Resp-Command = %d\n",
		       msg->command_id, resp->command_id);
		match = false;
	}
	if (resp->is_response != 1) {
		printf("Wrong IsResponse; Resp-IsResponse = %d\n", resp->is_response);
		match = false;
	}

	return match ? GSC_FWU_STATUS_SUCCESS : GSC_FWU_STATUS_FAILURE;
}

static uint32_t mk_host_if_call(struct mk_host_if *acmd,
                                const unsigned char *command, ssize_t command_sz,
                                uint8_t **read_buf, uint32_t rcmd,
                                unsigned int expected_sz)
{
	uint32_t in_buf_sz;
	size_t out_buf_sz;
	size_t written;
	TEESTATUS status;
	struct gsc_fwu_heci_response *msg_hdr;
	int count = 0;

	in_buf_sz = TeeGetMaxMsgLen(&acmd->mei_cl);
	if (in_buf_sz == 0)
	{
		if (acmd->verbose)
			fprintf(stderr, "mkhif: client reproted zero MTU.\n");
		return GSC_FWU_STATUS_FAILURE;
	}
	*read_buf = (uint8_t *)malloc(in_buf_sz);
	if (*read_buf == NULL)
		return GSC_FWU_STATUS_FAILURE;
	memset(*read_buf, 0, in_buf_sz);
	msg_hdr = (struct gsc_fwu_heci_response *)*read_buf;

	while (count++ < 2) {
		status = TeeWrite(&acmd->mei_cl, command, command_sz, &written, 0);
		if (status || written != command_sz) {
			if(!acmd->reconnect || !mk_host_if_connect(acmd))
				return GSC_FWU_STATUS_FAILURE;
			continue;
		}
		break;
	}

	status = TeeRead(&acmd->mei_cl, *read_buf, in_buf_sz, &out_buf_sz, MKHI_READ_TIMEOUT);
	if (status)
		return GSC_FWU_STATUS_FAILURE;

	status = msg_hdr->status;
	if (acmd->verbose)
		fprintf(stderr, "mkhif: message header read status = %d\n", status);

	status = mkhi_verify_response_header((struct gsc_fwu_heci_header *)command, &msg_hdr->header);
	if (status != GSC_FWU_STATUS_SUCCESS)
		return status;

	if (expected_sz && expected_sz != out_buf_sz) {
		return GSC_FWU_STATUS_FAILURE;
	}
	return GSC_FWU_STATUS_SUCCESS;
}

static void printf_if_fw_version(struct gsc_fwu_external_version *version)
{
	printf("Firmware Version %c%c%c%c.%d.%d\n",
	       version->project[0], version->project[1],
	       version->project[2], version->project[3],
	       version->hotfix, version->build);
}


static uint32_t mk_host_if_fw_version(struct mk_host_if *cmd,
                                      struct gsc_fwu_external_version *version)
{
	struct gsc_fwu_heci_version_req req;
	struct gsc_fwu_heci_version_resp *response = NULL;
	uint32_t status;
	size_t size = sizeof(struct gsc_fwu_heci_version_resp) + sizeof(struct gsc_fwu_external_version);

	req.header.command_id = GSC_FWU_HECI_COMMAND_ID_GET_IP_VERSION; /* GET FW Version */
	req.header.is_response = 0;
	req.header.reserved = 0;
	req.header.reserved2[0] = 0;
	req.header.reserved2[1] = 0;
	req.partition = gsc_fwu_heci_payload_type_gfx_fw;

	status = mk_host_if_call(cmd,
	                         (const unsigned char *)&req, sizeof(req),
	                         (uint8_t **)&response,
	                         0, (unsigned int)size);

	if (status != GSC_FWU_STATUS_SUCCESS) {
		return status;
	}
	memcpy(version, response->version, sizeof(struct gsc_fwu_external_version));

	return status;
}

static uint32_t mk_host_if_fw_version_req(struct mk_host_if *acmd)
{
	struct gsc_fwu_heci_version_req req;
	unsigned char *buf = (unsigned char *)&req;
	size_t written;
	TEESTATUS status;

	req.header.command_id = GSC_FWU_HECI_COMMAND_ID_GET_IP_VERSION; /* GET FW Version */
	req.header.is_response = 0;
	req.header.reserved = 0;
	req.header.reserved2[0] = 0;
	req.header.reserved2[1] = 0;
	req.partition = gsc_fwu_heci_payload_type_gfx_fw;

	status = TeeWrite(&acmd->mei_cl, buf, sizeof(req), &written, 0);
	if (status || written != sizeof(req))
		return GSC_FWU_STATUS_FAILURE;

	return GSC_FWU_STATUS_SUCCESS;
}

static uint32_t mk_host_if_fw_version_resp(struct mk_host_if *acmd)
{
	size_t recvd;
	TEESTATUS status;
	unsigned char rec_msg[100];
	struct gsc_fwu_heci_version_resp *response = (struct gsc_fwu_heci_version_resp *)rec_msg;
	size_t size = sizeof(struct gsc_fwu_heci_version_resp) + sizeof(struct gsc_fwu_external_version);

	status = TeeRead(&acmd->mei_cl, rec_msg, size, &recvd, MKHI_READ_TIMEOUT);
	if (status || recvd <= 0)
		return GSC_FWU_STATUS_FAILURE;
	printf_if_fw_version((struct gsc_fwu_external_version *)response->version);
	return GSC_FWU_STATUS_SUCCESS;
}

static void usage(const char *p)
{
	fprintf(stderr, "Usage: %s [-hv] [-e <l> ] [-i <n> ] [-b M.m.f.b] [-r] [-s <seq>] [-k <n>]\n", p);
	fprintf(stderr, "        -h                help\n");
	fprintf(stderr, "        -v                verbose\n");
	fprintf(stderr, "        -i <n>            iterate n times\n");
	fprintf(stderr, "        -r                reconnect if failed to write\n");
	fprintf(stderr, "        -k <n>            timeout between iterations in microseconds (default: 0)\n");
}

int main(int argc, char *argv[])
{

	struct mk_host_if acmd;

	/* Version Data */
	struct gsc_fwu_external_version version;

	unsigned int i, iterations = 1;
	const GUID *guid = &GUID_METEE_FWU;
	char *sequence = NULL;

	bool verbose = false;
	int ret = 0;
	bool reconnect = false;
	unsigned long iter_timeout = 0;

#ifdef _WIN32
	verbose = true;
	reconnect = true;
#else
	size_t echo_size;
	extern char *optarg;
	int opt;

	while ((opt = getopt(argc, argv, "hv:i:s:rk:")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		case 'i':
			ret = sscanf(optarg,"%u", &iterations);
			if (ret != 1) {
				usage(argv[0]);
				exit(1);
			}
			break;
		case 's':
			sequence = optarg;
			break;
		case 'r':
			reconnect = true;
			break;
		case 'k':
			ret = sscanf(optarg,"%lu", &iter_timeout);
			if (ret != 1) {
				usage(argv[0]);
				exit(1);
			}
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
#endif /* _WIN32 */


	if (!mk_host_if_init(&acmd, guid, reconnect, verbose)) {
		ret = 1;
		goto out;
	}

	if (sequence) {
		for (i = 0; i < strlen(sequence); i++) {
			if (sequence[i] == 's')
				mk_host_if_fw_version_req(&acmd);
			else if (sequence[i] == 'r')
				mk_host_if_fw_version_resp(&acmd);
		}
	}

	for (i = 0; i < iterations ; i++) {
		if (iter_timeout && i > 0) {
			printf("Sleeping for %lu microseconds ...\n", iter_timeout);
#ifdef _WIN32
			Sleep(iter_timeout/1000);
#else
			usleep(iter_timeout);
#endif /* _WIN32_ */
		}
		printf("Running version test %d...\n", i);
		memset(&version, 0, sizeof(version));
		ret = mk_host_if_fw_version(&acmd, &version);
		if (ret != GSC_FWU_STATUS_SUCCESS)
			goto out;

		printf_if_fw_version(&version);

		if (ret != GSC_FWU_STATUS_SUCCESS)
			goto out;
	}

out:
	mk_host_if_deinit(&acmd);
	printf("STATUS %s\n", mkhi_status(ret));
	return ret;
}
