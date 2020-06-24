/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2021-2025 Intel Corporation
 */
#include "metee_test.h"

DEFINE_GUID(GUID_NON_EXISTS_CLIENT,
	0x85eb8fa6, 0xbdd, 0x4d01, 0xbe, 0xc4, 0xa5, 0x97, 0x43, 0x4e, 0xd7, 0x62);

TEST_P(MeTeePPTEST, PROD_MKHI_SimpleGetVersion)
{
	struct MeTeeTESTParams intf = GetParam();
	std::vector<uint8_t> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage;

	for (int i = 1; i < 2; i++) {
		try {
			intel::security::metee metee(*intf.client);

			ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, metee.device_handle());
			metee.connect();

			ASSERT_EQ(MkhiRequest.size(), metee.write(MkhiRequest, 0));

			MaxResponse = metee.read(0);
			ASSERT_LE(sizeof(GEN_GET_FW_VERSION_ACK), MaxResponse.size());
			pResponseMessage = reinterpret_cast<GEN_GET_FW_VERSION_ACK*>(MaxResponse.data());

			ASSERT_EQ(TEE_SUCCESS, pResponseMessage->Header.Fields.Result);
			EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
			EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);
		}
		catch(const intel::security::metee_exception &ex){
			if (ex.code().value() == TEE_DEVICE_NOT_FOUND)
				GTEST_SKIP();
			FAIL() << "Excepton: " << ex.what();
		}
	}
}

TEST_P(MeTeePPTEST, PROD_N_Kind)
{
	struct MeTeeTESTParams intf = GetParam();

	try {
		intel::security::metee metee(*intf.client);

		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, metee.device_handle());

		ASSERT_GE(metee.kind().length(), 0);
	}
	catch (const intel::security::metee_exception& ex) {
		if (ex.code().value() == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		if (ex.code().value() != TEE_NOTSUPPORTED)
			FAIL() << "Excepton: " << ex.what();
	}
}

TEST_P(MeTeePPTEST, PROD_MKHI_InitFull)
{
	struct MeTeeTESTParams intf = GetParam();
	std::vector<uint8_t> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage;

	try {
		struct tee_device_address device = {tee_device_address::TEE_DEVICE_TYPE_NONE, { NULL }};

		intel::security::metee metee(*intf.client, device, TEE_LOG_LEVEL_VERBOSE, NULL);

		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, metee.device_handle());
		metee.connect();
	}
	catch (const intel::security::metee_exception& ex) {
		if (ex.code().value() == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		FAIL() << "Excepton: " << ex.what();
	}
}

TEST_P(MeTeePPTEST, PROD_N_TestConnectToNonExistsUuid)
{
	struct MeTeeTESTParams intf = GetParam();

	try {
		intel::security::metee metee(GUID_NON_EXISTS_CLIENT);
		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, metee.device_handle());
		metee.connect();
	}
	catch (const intel::security::metee_exception& ex) {
		if (ex.code().value() == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		if (ex.code().value() == TEE_CLIENT_NOT_FOUND)
			return;
		FAIL() << "Excepton: " << ex.what();
	}
	FAIL();
}

TEST_P(MeTeePPTEST, PROD_MKHI_MoveSemantics)
{
	struct MeTeeTESTParams intf = GetParam();
	std::vector<uint8_t> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage;

	intel::security::metee metee2;

	try {
		intel::security::metee metee(*intf.client);
		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, metee.device_handle());
		metee.connect();

		metee2 = std::move(metee);
	}
	catch (const intel::security::metee_exception& ex) {
		if (ex.code().value() == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		FAIL() << "Excepton: " << ex.what();
	}

	ASSERT_EQ(MkhiRequest.size(), metee2.write(MkhiRequest, 0));

	MaxResponse = metee2.read(0);
	ASSERT_LE(sizeof(GEN_GET_FW_VERSION_ACK), MaxResponse.size());
	pResponseMessage = reinterpret_cast<GEN_GET_FW_VERSION_ACK*>(MaxResponse.data());

	ASSERT_EQ(TEE_SUCCESS, pResponseMessage->Header.Fields.Result);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);
}

static struct MeTeeTESTParams interfaces[1] = {
	{"PCH", NULL, &GUID_DEVINTERFACE_MKHI}};

INSTANTIATE_TEST_SUITE_P(MeTeePPTESTInstance, MeTeePPTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTeePPTEST::ParamType>& info) {
			return info.param.name;
		});