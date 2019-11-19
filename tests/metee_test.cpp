/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#include <vector>
#include <chrono>
#include <thread>
#include <climits>
#include "metee_test.h"

DEFINE_GUID(GUID_NON_EXISTS_CLIENT,
	0x85eb8fa6, 0xbdd, 0x4d01, 0xbe, 0xc4, 0xa5, 0x97, 0x43, 0x4e, 0xd7, 0x62);


void TEEAPI CompletionRoutine(TEESTATUS status, size_t numberOfBytesTransfered)
{
	std::cout << "operation ended with status " << status << ". Num bytes transferred " << numberOfBytesTransfered << std::endl;
}
#define SUCCESS TEE_SUCCESS

std::string GetErrorString(unsigned long LastError);

//Print Expected and Ectual in Hex. 

// Retrieve the system error message for the last-error code
#ifdef WIN32
std::string GetErrorString(unsigned long LastError)
{

	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ARGUMENT_ARRAY |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		L"%0",
		LastError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpMsgBuf,
		0, NULL);

	std::string ErrorMessage((LPCSTR)lpMsgBuf);
	ErrorMessage.erase(ErrorMessage.find('\n')); //error string from system comes with a new line charcter. 
	return ErrorMessage;
}
#else
std::string GetErrorString(unsigned long LastError)
{
	return strerror(LastError);
}
#endif // WIN32

/*
Send GetVersion Command to HCI / MKHI
1) Open Connection to MKHI
2) Send GetVersion Req Command
3) Recevie GetVersion Resp Command
4) Check for Valid Resp
5) Close Connection
*/
TEST_F(MeTeeTEST, PROD_MKHI_SimpleGetVersion)
{
	TEEHANDLE Handle;
	size_t NumberOfBytes = 0;
	Handle.handle = NULL;
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048

	ASSERT_EQ(SUCCESS, TeeInit(&Handle, &GUID_DEVINTERFACE_MKHI, NULL));
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));


	MaxResponse.resize(Handle.maxMsgLen*sizeof(char));
	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));
	ASSERT_EQ(sizeof(GEN_GET_FW_VERSION), NumberOfBytes);

	ASSERT_EQ(SUCCESS, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 0));
	pResponseMessage = (GEN_GET_FW_VERSION_ACK*)(&MaxResponse[0]);

	ASSERT_EQ(SUCCESS, pResponseMessage->Header.Fields.Result);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);
		
	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Wait for timeout on recv data without send
1) Open Connection to MKHI
2) Recevie timeout on GetVersion Resp Command
3) Close Connection
*/
TEST_F(MeTeeTEST, PROD_MKHI_TimeoutGetVersion)
{
	TEEHANDLE Handle;
	size_t NumberOfBytes = 0;
	Handle.handle = NULL;
	std::vector <char> MaxResponse;

	ASSERT_EQ(SUCCESS, TeeInit(&Handle, &GUID_DEVINTERFACE_MKHI, NULL));
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	MaxResponse.resize(Handle.maxMsgLen*sizeof(char));

	EXPECT_EQ(TEE_TIMEOUT, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 1000));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Obtain FW status
1) Recevie FW status
2) Check for Valid Resp
*/
TEST_F(MeTeeTEST, PROD_MKHI_GetFWStatus)
{
	TEEHANDLE Handle;
	uint32_t fwStatusNum;
	uint32_t fwStatus;

	Handle.handle = NULL;

	ASSERT_EQ(SUCCESS, TeeInit(&Handle, &GUID_DEVINTERFACE_MKHI, NULL));
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	//FWSTS1
	fwStatusNum = 0;
	ASSERT_EQ(SUCCESS, TeeFWStatus(&Handle, fwStatusNum, &fwStatus));
	EXPECT_NE(0, fwStatus);

	//FWSTS2
	fwStatusNum = 1;
	ASSERT_EQ(SUCCESS, TeeFWStatus(&Handle, fwStatusNum, &fwStatus));
	EXPECT_NE(0, fwStatus);

	//Invalid input
	fwStatusNum = 6;
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeFWStatus(&Handle, fwStatusNum, &fwStatus));
	fwStatusNum = UINT_MAX;
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeFWStatus(&Handle, fwStatusNum, &fwStatus));
	fwStatusNum = 1;
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeFWStatus(NULL, fwStatusNum, &fwStatus));
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeFWStatus(&Handle, fwStatusNum, NULL));
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeFWStatus(NULL, fwStatusNum, NULL));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

#if 0
/*
We need to add the follow flow :
1)      Connect to a client(MKHI)
2)      Send Valid Write Command(async using TeeWrite2)
3)      Send Valid Write Command(async using TeeWrite2)
4)      Send Valid Write Command(async using TeeWrite2)
5)      Call IOCancel() to cancel the pending Writes
6)      Call Disconnect()

All should pass.
*/
TEST_F(MeTeeTEST, PROD_MKHI_3WritesAndCancle)
{
	TEEHANDLE Handle;
	size_t NumberOfBytes = 0;
	Handle.handle = NULL;
	
	GTEST_ASSERT_EQ(SUCCESS, TeeInit(&Handle, &GUID_DEVINTERFACE_MKHI, NULL));
	EXPECT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));
	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));
	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));

	GTEST_ASSERT_EQ(SUCCESS, TeeCancel(&Handle));
	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

}

/*
Send GetVersion Command to HCI/MKHI
1) Open Connection to MKHI
2) Send 2 read requests asyncronicly
4) Send 1 write requests
5) Expect return of read
6) Send second write request
5) Expect return of second read
5) Close Connection
*/
TEST_F(MeTeeTEST, MKHI_TwoReadsThenTwoWrites)
{
	vector<char> response1;
	vector<char> response2;
	size_t ReadNumberOfBytes1 = 0;
	size_t ReadNumberOfBytes2 = 0;
	size_t WriteNumberOfBytes = 0;
	DWORD err;

	GEN_GET_FW_VERSION_ACK* pResponseMessage = NULL; //max length for this client is 2048
	TEEHANDLE Handle;
	Handle.handle = NULL;

	ASSERT_EQ(SUCCESS, TeeInit(&Handle, &GUID_DEVINTERFACE_MKHI, NULL)); ///as default
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	response1.resize(Handle.maxMsgLen*sizeof(char));
	response2.resize(Handle.maxMsgLen*sizeof(char));

	ASSERT_EQ(SUCCESS, TeeRead(&Handle, &response1[0], (ULONG)Handle.maxMsgLen, &ReadNumberOfBytes1, 0));
	ASSERT_EQ(SUCCESS, TeeRead(&Handle, &response2[0], (ULONG)Handle.maxMsgLen, &ReadNumberOfBytes2, 0));

	

	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &WriteNumberOfBytes, 0));
	EXPECT_EQ(sizeof(GEN_GET_FW_VERSION), WriteNumberOfBytes);
		
	pResponseMessage = (GEN_GET_FW_VERSION_ACK*)(&response1[0]);

	EXPECT_EQ(SUCCESS, pResponseMessage->Header.Fields.Result);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);

#if _DEBUG
	printf("FWVersion.CodeMajor is %d \n", pResponseMessage->Data.FWVersion.CodeMajor);
	printf("FWVersion.CodeMinor is %d \n", pResponseMessage->Data.FWVersion.CodeMinor);
	printf("FWVersion.CodeHotFix is %d \n", pResponseMessage->Data.FWVersion.CodeHotFix);
	printf("FWVersion.CodeBuildNo is %d \n", pResponseMessage->Data.FWVersion.CodeBuildNo);
#endif

	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &WriteNumberOfBytes, 0));
	ASSERT_EQ(sizeof(GEN_GET_FW_VERSION), WriteNumberOfBytes);

	ASSERT_EQ(err, (WAIT_OBJECT_0));

	pResponseMessage = (GEN_GET_FW_VERSION_ACK*)(&response2[0]);

	EXPECT_EQ(SUCCESS, pResponseMessage->Header.Fields.Result);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);

#if _DEBUG
	printf("FWVersion.CodeMajor is %d \n", pResponseMessage->Data.FWVersion.CodeMajor);
	printf("FWVersion.CodeMinor is %d \n", pResponseMessage->Data.FWVersion.CodeMinor);
	printf("FWVersion.CodeHotFix is %d \n", pResponseMessage->Data.FWVersion.CodeHotFix);
	printf("FWVersion.CodeBuildNo is %d \n", pResponseMessage->Data.FWVersion.CodeBuildNo);
#endif

}
#endif
TEST_F(MeTeeNTEST, PROD_N_TestConnectToNullUuid)
{
	TEEHANDLE						handle = TEEHANDLE_ZERO;
		
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeInit(&handle, NULL, NULL));
}

TEST_F(MeTeeNTEST, PROD_N_TestConnectToNonExistsUuid)
{
	TEEHANDLE						handle = TEEHANDLE_ZERO;
		
	ASSERT_EQ(TEE_SUCCESS, TeeInit(&handle, &GUID_NON_EXISTS_CLIENT, NULL));

	ASSERT_EQ(TEE_CLIENT_NOT_FOUND, TeeConnect(&handle));
}

TEST_F(MeTeeNTEST, PROD_N_TestGetDriverVersion)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	teeDriverVersion_t ver = {0, 0, 0, 0};

	ASSERT_EQ(TEE_SUCCESS, TeeInit(&handle, &GUID_NON_EXISTS_CLIENT, NULL));

#ifdef WIN32
	ASSERT_EQ(TEE_SUCCESS, GetDriverVersion(&handle, &ver));

	EXPECT_NE(ver.major, 0);
	EXPECT_NE(ver.minor, 0);
	//hotfix may be 0. such as 99.13.0.x
	EXPECT_NE(ver.build, 0);
#else // WIN32
	ASSERT_EQ(TEE_NOTSUPPORTED, GetDriverVersion(&handle, &ver));
#endif // WIN32
}

TEST_F(MeTeeNTEST, PROD_N_TestGetDriverVersion_NullParam)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;

	ASSERT_EQ(TEE_SUCCESS, TeeInit(&handle, &GUID_NON_EXISTS_CLIENT, NULL));

	ASSERT_EQ(TEE_INVALID_PARAMETER, GetDriverVersion(&handle, NULL));
}

class MeTeeDataNTEST : public ::testing::Test {
public:
	MeTeeDataNTEST() {
		// initialization code here
	}

	void SetUp() {
#ifdef _DEBUG
		printf("Enter ProdTests SetUp\n");
#endif
		_handle.handle = NULL;
		ASSERT_EQ(SUCCESS, TeeInit(&_handle, &GUID_DEVINTERFACE_MKHI, NULL));
		ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&_handle));
		ASSERT_EQ(SUCCESS, TeeConnect(&_handle));
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
	TEEHANDLE						_handle;
	GEN_GET_FW_VERSION MkhiRequest;
};

TEST_F(MeTeeDataNTEST, PROD_N_TestFWUNullBufferWrite)
{
	size_t numOfBytes = 0;
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeWrite(&_handle, NULL, 1024, &numOfBytes, 0));
}

TEST_F(MeTeeDataNTEST, PROD_N_TestFWUZeroBufferSizeWrite)
{
	size_t numOfBytes = 0;
	std::vector<unsigned char> buf(1024);

	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeWrite(&_handle, &buf[0], 0, &numOfBytes, 0));
}

TEST_F(MeTeeDataNTEST, PROD_N_TestFWUBiggerThenMtuWrite)
{
	size_t				numOfBytes = 0;

	std::vector<unsigned char> buf(_handle.maxMsgLen + 10);

	ASSERT_EQ(TEE_INTERNAL_ERROR, TeeWrite(&_handle, &buf[0], buf.size(), &numOfBytes, 0));
}

#ifdef WIN32
TEST_F(MeTeeDataNTEST, PROD_N_TestSmallBufferRead)
{
	size_t WriteNumberOfBytes = 0;
	size_t NumberOfBytes = 0;
	std::vector<char> MaxResponse;
	const size_t Len = 1;

	MaxResponse.resize(Len);

	ASSERT_EQ(SUCCESS, TeeWrite(&_handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &WriteNumberOfBytes, 0));
	ASSERT_EQ(sizeof(GEN_GET_FW_VERSION), WriteNumberOfBytes);

	ASSERT_EQ(TEE_INSUFFICIENT_BUFFER, TeeRead(&_handle, &MaxResponse[0], Len, &NumberOfBytes, 0));
}
#endif // WIN32
