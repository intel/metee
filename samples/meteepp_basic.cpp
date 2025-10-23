/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2025 Intel Corporation
 */

#include <iostream>
#include <thread>
#include <meteepp.h>

DEFINE_GUID(MEI_MKHIF, 0x8e6a6715, 0x9abc, 0x4043,
	0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f);

static const uint32_t MKHI_TIMEOUT = 10000;
static const int CONNECT_RETRIES = 3;

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
	int retry = CONNECT_RETRIES;

	try {
		intel::security::metee metee(MEI_MKHIF, TEE_LOG_LEVEL_VERBOSE);

		try {
			std::cout << "Device kind is " << metee.kind() << std::endl;
		}
		catch (const intel::security::metee_exception& ex) {
			std::cerr << ex.what() << std::endl;
		}

		while (retry--) {
			try {
				metee.connect();
				break;
			}
			catch (const intel::security::metee_exception& ex) {
				if (ex.code().value() != TEE_BUSY &&
					ex.code().value() != TEE_UNABLE_TO_COMPLETE_OPERATION) /* windows return this error on busy */
					throw;
				std::cerr << "Client is busy, retrying" << std::endl;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		}

		if (metee.max_msg_len() == 0)
		{
			std::cerr << "Client reported zero MTU. Aborting." << std::endl;
			return 1;
		}

		struct mkhi_fwver_req req;
		uint8_t* u = reinterpret_cast<uint8_t*>(&req);
		/* Write */
		req.header.data = 0; /* Reset */
		req.header.GroupId = 0xFF; /* MKHI */
		req.header.Command = 0x02; /* GET FW Version */
		req.header.IsResponse = 0;
		req.header.Reserved = 0;

		size_t written = metee.write(std::vector<uint8_t>(u, u + sizeof req), MKHI_TIMEOUT);
		if (written != sizeof(req)) {
			std::cerr << "Write failed written " << written << std::endl;
			return 1;
		}

		std::vector<uint8_t> res = metee.read(MKHI_TIMEOUT);
		if (res.size() < sizeof(struct mkhi_msg_hdr)) {
			std::cerr << "Returned less than header = " << res.size() << std::endl;
			return 1;
		}

		if (res.size() < sizeof(struct mkhi_fwver_rsp)) {
			std::cerr << "Returned less than response = " << res.size() << std::endl;
			return 1;
		}

		struct mkhi_fwver_rsp* rsp;
		rsp = reinterpret_cast<struct mkhi_fwver_rsp*>(res.data());


		if (rsp->header.Result) {
			std::cerr << "Result = " << rsp->header.Result << std::endl;
			return 1;
		}

		std::cout << "Version: " << rsp->version.code.major << "." << rsp->version.code.minor << "."
			<< rsp->version.code.hotFix << "." << rsp->version.code.buildNo << std::endl;

		return 0;
	}
	catch (const intel::security::metee_exception& ex) {
		std::cerr << ex.what() << std::endl;
		return 1;
	}
}
