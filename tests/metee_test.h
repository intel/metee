/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#include <memory.h>
#include <string>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"

#include "metee.h"
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
class MeTeeTEST : public ::testing::Test {
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

class MeTeeNTEST : public ::testing::Test {
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






