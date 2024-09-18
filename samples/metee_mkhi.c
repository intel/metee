/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2012-2024 Intel Corporation
 */
#include <stdarg.h>
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

struct mkhi_msg_hdr {

	union {
		uint32_t data;
		struct {
			uint32_t  GroupId     :8;
			uint32_t  Command     :7;
			uint32_t  IsResponse  :1;
			uint32_t  Reserved    :8;
			uint32_t  Result      :8;
		};
	};
};

struct mk_host_if_msg {
	struct mkhi_msg_hdr header;
	uint8_t data[0];
};

struct _firmware_version {
	uint16_t minor;
	uint16_t major;
	uint16_t buildNo;
	uint16_t hotFix;
};
struct mei_firmware_version {
	struct _firmware_version code;
	struct _firmware_version NFTP;
	struct _firmware_version FITC;
};

enum mkhi_group_id {
	MKHI_CBM_GROUP_ID = 0,
	MKHI_PM_GROUP_ID,
	MKHI_PWD_GROUP_ID,
	MKHI_FWCAPS_GROUP_ID,
	MKHI_APP_GROUP_ID,      /* Reserved (no longer used). */
	MKHI_FWUPDATE_GROUP_ID, /* This is for manufacturing downgrade */
	MKHI_FIRMWARE_UPDATE_GROUP_ID,
	MKHI_BIST_GROUP_ID,
	MKHI_MDES_GROUP_ID,
	MKHI_ME_DBG_GROUP_ID,
	MKHI_MAX_GROUP_ID,
	MKHI_GEN_GROUP_ID = 0xFF
};



#define MKHI_STATUS_SUCCESS                0x0
#define MKHI_STATUS_INTERNAL_ERROR         0x1
#define MKHI_STATUS_NOT_READY              0x2
#define MKHI_STATUS_INVALID_AMT_MODE       0x3
#define MKHI_STATUS_INVALID_MESSAGE_LENGTH 0x4

#define MKHI_STATUS_HOST_IF_EMPTY_RESPONSE  0x4000
#define MKHI_STATUS_SDK_RESOURCES      0x1004

#define MKHI_TEST_ECHO_GROUP_ID		MKHI_APP_GROUP_ID
#define TEST_ECHO_CMD				0x01

#define MKHI_READ_TIMEOUT 10000

const char *mkhi_status(uint32_t status)
{
#define MKH_STATUS(_x) case _x: return #_x
	switch(status) {
	MKH_STATUS(MKHI_STATUS_SUCCESS);
	MKH_STATUS(MKHI_STATUS_INTERNAL_ERROR);
	MKH_STATUS(MKHI_STATUS_NOT_READY);
	MKH_STATUS(MKHI_STATUS_INVALID_AMT_MODE);
	MKH_STATUS(MKHI_STATUS_INVALID_MESSAGE_LENGTH);
	MKH_STATUS(MKHI_STATUS_HOST_IF_EMPTY_RESPONSE);
	MKH_STATUS(MKHI_STATUS_SDK_RESOURCES);
	default:
		fprintf(stderr, "unknown 0x%08X\n", status);
		return "unknown";
	}
#undef MKH_STATUS
}

struct mkhi_test_msg_hdr {
	union {
		uint32_t data;
		struct {
			uint32_t  GroupId     :8;
			uint32_t  Command     :7;
			uint32_t  IsResponse  :1;
			uint32_t  Size        :16;
		};
	};
};

struct mkhi_test_msg {
	struct mkhi_test_msg_hdr header;
	unsigned char data[0];
};

static size_t mkhi_test_msg_size(struct mkhi_test_msg *msg)
{
	return  msg->header.Size + sizeof(msg->header);
}

static struct mkhi_test_msg *mkhi_test_msg_alloc(size_t size)
{
	size_t cnt =  size / 4 + 1 + 1; /* size + possible alignment + header */
	struct mkhi_test_msg * _ptr = (struct mkhi_test_msg *)calloc(cnt, sizeof(uint32_t));
	if (_ptr)
		_ptr->header.Size = (uint32_t)size;
	return _ptr;
}

#define MAX_TEST_MESSAGE_SIZE_BYTES  1000
#define MAX_TEST_MESSAGE_SIZE_DWORDS  MAX_TEST_MESSAGE_SIZE_BYTES/sizeof(uint32_t)

DEFINE_GUID(MEI_MKHIF, 0x8e6a6715, 0x9abc,0x4043,
                                  0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0xf);

DEFINE_GUID(MEI_MKHIF_TEST, 0x22222222, 0x9abc, 0x4043,
				  0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0xf);


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

void mk_host_if_log(bool is_error, const char* fmt, ...)
{
#define DEBUG_MSG_LEN 1024
	char msg[DEBUG_MSG_LEN + 1];
	va_list varl;
	va_start(varl, fmt);
	vsprintf(msg, fmt, varl);
	va_end(varl);

	fprintf((is_error) ? stderr : stdout, "LIB: %s", msg);
}

static bool mk_host_if_init(struct mk_host_if *acmd, const GUID *guid,
                            bool reconnect, bool verbose)
{
	int status;
	uint32_t log_level, original_log_level;
	struct tee_device_address addr = {
		.type = TEE_DEVICE_TYPE_NONE,
		.data.path = NULL
	};

	acmd->reconnect = reconnect;
	acmd->verbose = verbose;
	status = TeeInitFull(&acmd->mei_cl, guid, addr,
		(verbose) ? TEE_LOG_LEVEL_VERBOSE : TEE_LOG_LEVEL_ERROR, mk_host_if_log);
	if (!TEE_IS_SUCCESS(status)) {
		fprintf(stderr, "init failed with status = %d\n", status);
		return false;
	}
	original_log_level = TeeGetLogLevel(&acmd->mei_cl);
	printf("Original log level: %u\n", original_log_level);
	log_level = TeeSetLogLevel(&acmd->mei_cl, TEE_LOG_LEVEL_ERROR);
	printf("Original log level: %u\n", log_level);
	log_level = TeeGetLogLevel(&acmd->mei_cl);
	printf("New log level: %u\n", log_level);
	TeeSetLogLevel(&acmd->mei_cl, original_log_level);
	printf("Original log level: %u\n", original_log_level);
	return mk_host_if_connect(acmd);
}

static void mk_host_if_deinit(struct mk_host_if *acmd)
{
	TeeDisconnect(&acmd->mei_cl);
}

static uint32_t mkhi_verify_response_header(struct mkhi_msg_hdr *msg, struct mkhi_msg_hdr *resp)
{
	bool match = true;
	if (msg->Command != resp->Command) {
		printf("Mismatch Command; Req-Command = %d , Resp-Command = %d\n",
			msg->Command, resp->Command);
		match = false;
	}
	if (msg->GroupId != resp->GroupId) {
		printf("Mismatch GroupId; Req-GroupId = %d , Resp-GroupId = %d\n",
			msg->GroupId, resp->GroupId);
                match = false;
	}
	if (resp->IsResponse != 1) {
		printf("Wrong IsResponse; Resp-IsResponse = %d\n", resp->IsResponse);
		match = false;
	}

	return match ? MKHI_STATUS_SUCCESS :  MKHI_STATUS_INTERNAL_ERROR;
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
	struct mk_host_if_msg *msg_hdr;
	int count = 0;

	in_buf_sz = TeeGetMaxMsgLen(&acmd->mei_cl);
	if (in_buf_sz == 0)
	{
		if (acmd->verbose)
			fprintf(stderr, "mkhif: client reproted zero MTU.\n");
		return MKHI_STATUS_INTERNAL_ERROR;
	}
	*read_buf = (uint8_t *)malloc(in_buf_sz);
	if (*read_buf == NULL)
		return MKHI_STATUS_SDK_RESOURCES;
	memset(*read_buf, 0, in_buf_sz);
	msg_hdr = (struct mk_host_if_msg *)*read_buf;

	while (count++ < 2) {
		status = TeeWrite(&acmd->mei_cl, command, command_sz, &written, 0);
		if (status || written != command_sz) {
			if(!acmd->reconnect || !mk_host_if_connect(acmd))
				return MKHI_STATUS_INTERNAL_ERROR;
			continue;
		}
		break;
	}

	status = TeeRead(&acmd->mei_cl, *read_buf, in_buf_sz, &out_buf_sz, MKHI_READ_TIMEOUT);
	if (status)
		return MKHI_STATUS_HOST_IF_EMPTY_RESPONSE;

	status = msg_hdr->header.Result;
	if (acmd->verbose)
		fprintf(stderr, "mkhif: message header read status = %d\n", status);
#if 0
	if (status != MKHI_STATUS_SUCCESS)
		return status;
#endif

	status = mkhi_verify_response_header((struct mkhi_msg_hdr *)command, &msg_hdr->header);
	if (status != MKHI_STATUS_SUCCESS)
		return status;

	if (expected_sz && expected_sz != out_buf_sz) {
		return MKHI_STATUS_INTERNAL_ERROR;
	}
	return MKHI_STATUS_SUCCESS;
}

static uint32_t mk_host_if_fw_component_version_validate(const char *component,
	struct _firmware_version *version, struct _firmware_version *expected)
{
	uint32_t match = 0;
	if (!component)
		return (uint32_t)-1;

	if (version->major != expected->major) {
		printf("Wrong %s Major %d != %d\n", component,
			version->major, expected->major);
		match |= BIT(1);
	}
	if (version->minor != expected->minor) {
		printf("Wrong %s Minor %d != %d\n", component,
			version->minor, expected->minor);
		match |= BIT(2);
	}
	if (version->hotFix != expected->hotFix) {
		printf("Wrong %s HotFix %d != %d\n", component,
			version->hotFix, expected->hotFix);
		match |= BIT(3);
	}
	if (version->buildNo != expected->buildNo) {
		printf("Wrong %s BuildNo %d != %d\n", component,
			version->buildNo, expected->buildNo);
		match |= BIT(4);
	}
	return match;
}
static uint32_t mk_host_if_fw_version_validate(struct mei_firmware_version *version, struct mei_firmware_version *expected)
{
	uint32_t match = 0;

	match |= mk_host_if_fw_component_version_validate("code", &version->code, &expected->code);
	match |= mk_host_if_fw_component_version_validate("NFTP", &version->NFTP, &expected->NFTP);
	/* Do not validate FITC
	 * match |= mk_host_if_fw_component_version_validate("FITC", &version->FITC, &expected->FITC);
	 */
	return match == 0 ? MKHI_STATUS_SUCCESS :  MKHI_STATUS_INTERNAL_ERROR;
}

static void printf_if_fw_version(struct mei_firmware_version *version)
{
#define print_ver_component(COMP) \
		printf("ME "# COMP " Firmware Version %d.%d.%d.%d\n",	\
		version->COMP.major, version->COMP.minor,		\
		version->COMP.hotFix, version->COMP.buildNo);

	print_ver_component(code);
	print_ver_component(NFTP);
	print_ver_component(FITC);

#undef print_ver_component
}


static uint32_t mk_host_if_fw_version(struct mk_host_if *cmd,
                                      struct mei_firmware_version *version)
{
	struct mkhi_msg_hdr hdr;
	struct mk_host_if_msg *response = NULL;
	uint32_t status;
	size_t size = sizeof(struct mkhi_msg_hdr) + sizeof(struct mei_firmware_version);

	hdr.data = 0; /* Reset */
	hdr.GroupId = 0xFF; /* MKHI */
	hdr.Command = 0x02; /* GET FW Version */
	hdr.IsResponse = 0;
	hdr.Reserved = 0;

	status = mk_host_if_call(cmd,
	                         (const unsigned char *)&hdr, sizeof(hdr),
	                         (uint8_t **)&response,
	                         0, (unsigned int)size);

	if (status != MKHI_STATUS_SUCCESS) {
		return status;
	}
	memcpy(version, response->data, sizeof(struct mei_firmware_version));

	return status;
}

static uint32_t mk_host_if_fw_version_req(struct mk_host_if *acmd)
{
	struct mkhi_msg_hdr hdr;
	unsigned char *buf = (unsigned char *)&hdr;
	size_t written;
	TEESTATUS status;

	hdr.data = 0; /* Reset */
	hdr.GroupId = 0xFF; /* MKHI */
	hdr.Command = 0x02; /* GET FW Version */
	hdr.IsResponse = 0;
	hdr.Reserved = 0;

	status = TeeWrite(&acmd->mei_cl, buf, sizeof(hdr), &written, 0);
	if (status || written != sizeof(hdr))
		return MKHI_STATUS_INTERNAL_ERROR;

	return MKHI_STATUS_SUCCESS;
}

static uint32_t mk_host_if_fw_version_resp(struct mk_host_if *acmd)
{
	size_t recvd;
	TEESTATUS status;
	unsigned char rec_msg[100];
	size_t size = sizeof(struct mkhi_msg_hdr) + sizeof(struct mei_firmware_version);

	status = TeeRead(&acmd->mei_cl, rec_msg, size, &recvd, MKHI_READ_TIMEOUT);
	if (status || recvd <= 0)
		return MKHI_STATUS_HOST_IF_EMPTY_RESPONSE;
	printf_if_fw_version((struct mei_firmware_version *)rec_msg);
	return MKHI_STATUS_SUCCESS;
}

static uint32_t mk_host_if_echo_validate(uint8_t *request, uint8_t *response, size_t len)
{

	unsigned int i;
	for (i = 0; i < len; i++)
		if (request[i] != response[i])
			break;

	if (i < len) {
		printf("Mismatch Echo [%d] request=0x%08X:response=0x%08X\n",
			i, request[i], response[i]);
		return MKHI_STATUS_INTERNAL_ERROR;
	}
	return MKHI_STATUS_SUCCESS;
}

static uint32_t mk_host_if_echo(struct mk_host_if *cmd, struct mkhi_test_msg *msg)
{
	struct mkhi_test_msg *response = NULL;
	size_t echo_len = mkhi_test_msg_size(msg);
	uint32_t status;

	msg->header.GroupId = MKHI_TEST_ECHO_GROUP_ID;
	msg->header.Command = TEST_ECHO_CMD;
	msg->header.IsResponse = 0;

	status = mk_host_if_call(cmd,
			(const unsigned char *)msg, (unsigned int)echo_len,
			(uint8_t **)&response,
			0, (unsigned int)echo_len);

	if (status != MKHI_STATUS_SUCCESS)
		return status;

	if (response->header.Size != msg->header.Size) {
		printf("Mismatch Echo size request=%d:response=%d\n",
			msg->header.Size, response->header.Size);
		return MKHI_STATUS_INVALID_MESSAGE_LENGTH;
	}

	status = mk_host_if_echo_validate(msg->data, response->data,
					  msg->header.Size);
	return status;
}

static void usage(const char *p)
{
	fprintf(stderr, "Usage: %s [-hv] [-e <l> ] [-i <n> ] [-b M.m.f.b] [-r] [-s <seq>] [-k <n>]\n", p);
	fprintf(stderr, "        -h                help\n");
	fprintf(stderr, "        -v                verbose\n");
	fprintf(stderr, "        -b [M.m.f.b]      expect fw version M.m.f.b\n");
	fprintf(stderr, "        -t                check for test fw\n");
	fprintf(stderr, "        -e <l>            run echo command of l (less then 1024) bytes (only with test fw)\n");
	fprintf(stderr, "        -i <n>            iterate n times\n");
	fprintf(stderr, "        -r                reconnect if failed to write\n");
	fprintf(stderr, "        -k <n>            timeout between iterations in microseconds (default: 0)\n");
}
int main(int argc, char *argv[])
{

	struct mk_host_if acmd;

	/* Version Data */
	struct mei_firmware_version version;
	struct mei_firmware_version *_expected = NULL;
	

	struct mkhi_test_msg *echo_msg = NULL;
	unsigned int i, iterations = 1;
	const GUID *guid = &MEI_MKHIF;
	char *sequence = NULL;

	bool verbose = false;
	int ret = 0;
	bool reconnect = false;
	unsigned long iter_timeout = 0;

#ifdef _WIN32
	verbose = true;
	reconnect = true;
#else
	struct mei_firmware_version expected;
	size_t echo_size;
	extern char *optarg;
	int opt;

	while ((opt = getopt(argc, argv, "hvb:e::i:ts:rk:")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		case 't':
			guid = &MEI_MKHIF_TEST;

			expected.code.major = 1;
			expected.code.minor = 2;
			expected.code.buildNo = 3;
			expected.code.hotFix = 4;

			expected.NFTP.minor = 5;
			expected.NFTP.major = 6;
			expected.NFTP.buildNo = 7;
			expected.NFTP.hotFix = 8;

			expected.FITC.minor = 9;
			expected.FITC.major = 10;
			expected.FITC.buildNo = 11;
			expected.FITC.hotFix = 12;

			_expected = &expected;
			break;
		case 'b':
			_expected = &expected;
			ret = sscanf(optarg, "%hd.%hd.%hd.%hd",
				&expected.code.major, &expected.code.minor,
				&expected.code.hotFix, &expected.code.buildNo);
			if (ret != 4) {
				usage(argv[0]);
				exit(1);
			}

			memcpy(&expected.NFTP, &expected.code,  sizeof(expected.NFTP));
			/* No need to check for FITC */

			break;
		case 'e':
			ret = sscanf(optarg,"%zu", &echo_size);
			if (ret != 1) {
				usage(argv[0]);
				exit(1);
			}
			if (echo_size > 1024) {
				fprintf(stderr, "echo size is limited to 1024\n");
				usage(argv[0]);
				exit(1);
			}
			echo_msg = mkhi_test_msg_alloc(echo_size);
			if (!echo_msg) {
				fprintf(stderr, "cannot allocate memory\n");
				exit(1);
			}

			for (i = 0; i < echo_size; i++) {
				echo_msg->data[i] = i;
			}
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
		memset(&version, 0, sizeof(struct mei_firmware_version));
		ret = mk_host_if_fw_version(&acmd, &version);
		if (ret != MKHI_STATUS_SUCCESS)
			goto out;

		if (_expected)
			ret = mk_host_if_fw_version_validate(&version, _expected);

		printf_if_fw_version(&version);

		if (ret != MKHI_STATUS_SUCCESS)
			goto out;
	}

	if (echo_msg) {
		printf("Running echo test size=%d...\n", echo_msg->header.Size);
		ret = mk_host_if_echo(&acmd, echo_msg);
	}

out:
	free(echo_msg);
	mk_host_if_deinit(&acmd);
	printf("STATUS %s\n", mkhi_status(ret));
	return ret;
}
