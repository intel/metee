/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2020 Intel Corporation
 */
#include <memory.h>
#include <string>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include <gmock/gmock.h>

#include "metee.h"
#include "helpers.h"
#ifdef WIN32
#include <windows.h>
#else
#define ERROR_INVALID_HANDLE -ENOTTY
#define ERROR_NOT_FOUND -ENODEV
#define INVALID_HANDLE_VALUE ((void*)0)
#endif // WIN32
#include "MKHI.h"

std::string GetErrorString(unsigned long LastError);

//Print Expected and Ectual in Hex.
namespace testing {
	namespace internal {
		inline void PrintTo(unsigned long n, ::std::ostream* os) {
			(*os) << "0x" << std::hex << n << " " << (GetErrorString(n));
		}
	}
}

#ifdef _WIN32
inline TEESTATUS TestTeeInitGUID(PTEEHANDLE handle, const GUID *guid, const GUID *device)
{
	if (device != NULL)
		return TeeInitGUID(handle, guid, device);
	else
		return TeeInit(handle, guid, NULL);
}
#else /* _WIN32 */
inline TEESTATUS TestTeeInitGUID(PTEEHANDLE handle, const GUID *guid, const GUID *device)
{
	return TeeInit(handle, guid, NULL);
}
#endif /* _WIN32 */

struct MeTeeTESTParams {
	const char *name;
	const GUID *device;
	const GUID *client;
};

class MeTeeTEST : public ::testing::TestWithParam<struct MeTeeTESTParams>{
public:
	MeTeeTEST() {
		// initialization code here
	}

	void SetUp() {
#ifdef _DEBUG
		printf("Enter ProdTests SetUp\n");
#endif
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;
#ifdef _DEBUG
		printf("Exit ProdTests SetUp\n");
#endif
	}

	void TearDown() {
#ifdef _DEBUG
		printf("Enter ProdTests TearDown\n");
#endif

		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Is it helping?
#ifdef _DEBUG
		printf("Exit ProdTests TearDown\n");
#endif
	}

	~MeTeeTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	GEN_GET_FW_VERSION MkhiRequest;
};

class MeTeeNTEST : public ::testing::TestWithParam<struct MeTeeTESTParams> {
public:
	MeTeeNTEST() {
		// initialization code here
	}

	void SetUp() {
#ifdef _DEBUG
		printf("Enter ProdTests SetUp\n");
#endif
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;
#ifdef _DEBUG
		printf("Exit ProdTests SetUp\n");
#endif
	}

	void TearDown() {
#ifdef _DEBUG
		printf("Enter ProdTests TearDown\n");
#endif

		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Is it helping?
#ifdef _DEBUG
		printf("Exit ProdTests TearDown\n");
#endif
	}

	~MeTeeNTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	GEN_GET_FW_VERSION MkhiRequest;
};

class MeTeeFDTEST : public ::testing::TestWithParam<struct MeTeeTESTParams> {
public:
	MeTeeFDTEST() : deviceHandle(TEE_INVALID_DEVICE_HANDLE) {
		// initialization code here
	}

	void SetUp() {
		OpenMEI();
		if (deviceHandle == TEE_INVALID_DEVICE_HANDLE)
			GTEST_SKIP();
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;
	}

	void TearDown() {
		CloseMEI();
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Is it helping?
	}

	~MeTeeFDTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	GEN_GET_FW_VERSION MkhiRequest;
private:
	void OpenMEI();
	void CloseMEI();
public:
	TEE_DEVICE_HANDLE deviceHandle;
};

class MeTeeDataNTEST : public ::testing::TestWithParam<struct MeTeeTESTParams> {
public:
	MeTeeDataNTEST() {
		// initialization code here
		__tee_init_handle(&_handle);
	}

	void SetUp() {
		struct MeTeeTESTParams intf = GetParam();
		TEESTATUS status;

#ifdef _DEBUG
		printf("Enter ProdTests SetUp\n");
#endif
		_handle.handle = NULL;

		status = TestTeeInitGUID(&_handle, intf.client, intf.device);
		if (status == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		ASSERT_EQ(TEE_SUCCESS, status);
		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&_handle));
		ASSERT_EQ(TEE_SUCCESS, TeeConnect(&_handle));
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;
#ifdef _DEBUG
		printf("Exit ProdTests SetUp\n");
#endif
	}

	void TearDown() {
#ifdef _DEBUG
		printf("Enter ProdTests TearDown\n");
#endif
		TeeDisconnect(&_handle);
		//Is this helping?
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
#ifdef _DEBUG
		printf("Exit ProdTests TearDown\n");
#endif
	}

	~MeTeeDataNTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	TEEHANDLE _handle;
	GEN_GET_FW_VERSION MkhiRequest;
};
