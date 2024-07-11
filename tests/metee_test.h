/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2024 Intel Corporation
 */
#include <memory.h>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <climits>
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
#include <sys/resource.h>
#endif // WIN32
#include "MKHI.h"

std::string GetErrorString(unsigned long LastError);

TEESTATUS ConnectRetry(PTEEHANDLE handle);

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

class MeTeeOpenTEST : public ::testing::TestWithParam<struct MeTeeTESTParams> {
public:
	MeTeeOpenTEST() {
		// initialization code here
		__tee_init_handle(&_handle);
	}

	void SetUp() {
		struct MeTeeTESTParams intf = GetParam();
		TEESTATUS status;

		_handle.handle = NULL;

		status = TestTeeInitGUID(&_handle, intf.client, intf.device);
		if (status == TEE_DEVICE_NOT_FOUND)
			GTEST_SKIP();
		ASSERT_EQ(TEE_SUCCESS, status);
		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&_handle));
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;
	}

	void TearDown() {
		TeeDisconnect(&_handle);
		//Is this helping?
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	~MeTeeOpenTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	TEEHANDLE _handle;
	GEN_GET_FW_VERSION MkhiRequest;
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
		ASSERT_EQ(TEE_SUCCESS, ConnectRetry(&_handle));
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

class MeTee1000OpenTEST : public ::testing::TestWithParam<struct MeTeeTESTParams>{
public:
	MeTee1000OpenTEST() {
		// initialization code here
	}

	void SetUp() {
#ifdef _DEBUG
		printf("Enter MeTee1000OpenTEST SetUp\n");
#endif
		MkhiRequest.Header.Fields.Command = GEN_GET_FW_VERSION_CMD;
		MkhiRequest.Header.Fields.GroupId = MKHI_GEN_GROUP_ID;
		MkhiRequest.Header.Fields.IsResponse = 0;

#ifdef __linux__
		struct rlimit limit;

		limit.rlim_cur = 2048;
		limit.rlim_max = 2048;
		errno = 0;
		if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
			FAIL() << "setrlimit() failed with errno=" << errno;
#endif // __linux__

		for (int g = 0; g <= 199; g++) {
			v.push_back(std::thread([g]() {
				int base = g * 10;
				int i = 0;
				std::ofstream file0;
				std::string name0 = "example_" + std::to_string(base) + ".txt";
				std::ofstream file1;
				std::string name1 = "example_" + std::to_string(base + 1) + ".txt";
				std::ofstream file2;
				std::string name2 = "example_" + std::to_string(base + 2) + ".txt";
				std::ofstream file3;
				std::string name3 = "example_" + std::to_string(base + 3) + ".txt";
				std::ofstream file4;
				std::string name4 = "example_" + std::to_string(base + 4) + ".txt";
				std::ofstream file5;
				std::string name5 = "example_" + std::to_string(base + 5) + ".txt";
				std::ofstream file6;
				std::string name6 = "example_" + std::to_string(base + 6) + ".txt";
				std::ofstream file7;
				std::string name7 = "example_" + std::to_string(base + 7) + ".txt";
				std::ofstream file8;
				std::string name8 = "example_" + std::to_string(base + 8) + ".txt";
				std::ofstream file9;
				std::string name9 = "example_" + std::to_string(base + 9) + ".txt";
				file0.open(name0);
				file1.open(name1);
				file2.open(name2);
				file3.open(name3);
				file4.open(name4);
				file5.open(name5);
				file6.open(name6);
				file7.open(name7);
				file8.open(name8);
				file9.open(name9);
				while (i++ < 5) {
					file0 << "Writing to " << name0 << std::endl;
					file1 << "Writing to " << name1 << std::endl;
					file2 << "Writing to " << name2 << std::endl;
					file3 << "Writing to " << name3 << std::endl;
					file4 << "Writing to " << name4 << std::endl;
					file5 << "Writing to " << name5 << std::endl;
					file6 << "Writing to " << name6 << std::endl;
					file7 << "Writing to " << name7 << std::endl;
					file8 << "Writing to " << name8 << std::endl;
					file9 << "Writing to " << name9 << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
				file0.close();
				file1.close();
				file2.close();
				file3.close();
				file4.close();
				file5.close();
				file6.close();
				file7.close();
				file8.close();
				file9.close();
				remove(name0.c_str());
				remove(name1.c_str());
				remove(name2.c_str());
				remove(name3.c_str());
				remove(name4.c_str());
				remove(name5.c_str());
				remove(name6.c_str());
				remove(name7.c_str());
				remove(name8.c_str());
				remove(name9.c_str());
			}));
		}
		std::this_thread::sleep_for(std::chrono::seconds(5));
#ifdef _DEBUG
		printf("Exit MeTee1000OpenTEST SetUp\n");
#endif
	}

	void TearDown() {
#ifdef _DEBUG
		printf("Enter MeTee1000OpenTEST TearDown\n");
#endif

		for (std::vector<std::thread>::iterator it = v.begin(); it != v.end(); it++)
			it->std::thread::join();

		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Is it helping?
#ifdef _DEBUG
		printf("Exit MeTee1000OpenTEST TearDown\n");
#endif
	}

	~MeTee1000OpenTEST() {
		// cleanup any pending stuff, but no exceptions allowed
	}
	GEN_GET_FW_VERSION MkhiRequest;
	std::vector<std::thread> v;
};
