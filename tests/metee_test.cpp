/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2024 Intel Corporation
 */
#include <vector>
#include <chrono>
#include <thread>
#include <climits>
#include <fstream>
#include "metee_test.h"
#ifdef WIN32
extern "C" {
#include "public.h"
#include "metee_win.h"
}
#endif // WIN32

DEFINE_GUID(GUID_NON_EXISTS_CLIENT,
	0x85eb8fa6, 0xbdd, 0x4d01, 0xbe, 0xc4, 0xa5, 0x97, 0x43, 0x4e, 0xd7, 0x62);


void TEEAPI CompletionRoutine(TEESTATUS status, size_t numberOfBytesTransfered)
{
	std::cout << "operation ended with status " << status << ". Num bytes transferred " << numberOfBytesTransfered << std::endl;
}
#define SUCCESS TEE_SUCCESS

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
	ErrorMessage.erase(ErrorMessage.find('\n')); //error string from system comes with a new line character. 
	return ErrorMessage;
}
#else
std::string GetErrorString(unsigned long LastError)
{
	return strerror(LastError);
}
#endif // WIN32

// Open MEI with default path
#ifdef WIN32
void MeTeeFDTEST::OpenMEI()
{
	DWORD status;
	char devicePath[MAX_PATH] = {0};
	TEEHANDLE handle;

	__tee_init_handle(&handle);

	deviceHandle = TEE_INVALID_DEVICE_HANDLE;
	status = GetDevicePath(&handle, &GUID_DEVINTERFACE_HECI, devicePath, MAX_PATH);
	if (status)
		return;
	deviceHandle = CreateFileA(devicePath,
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_FLAG_OVERLAPPED,
					NULL);
}
void MeTeeFDTEST::CloseMEI()
{
	CloseHandle(deviceHandle);
}
#else
#define MEI_DEFAULT_DEVICE "/dev/mei0"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
void MeTeeFDTEST::OpenMEI()
{
	deviceHandle = open(MEI_DEFAULT_DEVICE, O_RDWR | O_CLOEXEC);
}
void MeTeeFDTEST::CloseMEI()
{
	close(deviceHandle);
}
#endif // WIN32

/*
Send GetVersion Command to HCI / MKHI
1) Open Connection to MKHI
2) Send GetVersion Req Command
3) Receive GetVersion Resp Command
4) Check for Valid Resp
5) Close Connection
*/
TEST_P(MeTeeTEST, PROD_MKHI_SimpleGetVersion)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
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
Check too big timeouts on read and write
*/
TEST_P(MeTeeTEST, PROD_MKHI_BadTimeout)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	char buf[10];
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));


	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeWrite(&Handle, buf, 10, &NumberOfBytes, (uint32_t)INT_MAX + 1));

	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeRead(&Handle, buf, 10, &NumberOfBytes, (uint32_t)INT_MAX + 1));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Set log level
1) Init metee handle
2) Get log level
3) Change log level
4) Check for valid log levels
*/
TEST_P(MeTeeTEST, PROD_MKHI_SetLogLevel)
{
        TEEHANDLE Handle = TEEHANDLE_ZERO;
        struct MeTeeTESTParams intf = GetParam();
        TEESTATUS status;
	uint32_t orig_log_level, prev_log_level, new_log_level;

        status = TestTeeInitGUID(&Handle, intf.client, intf.device);
        if (status == TEE_DEVICE_NOT_FOUND)
                GTEST_SKIP();
        ASSERT_EQ(SUCCESS, status);
        ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

        orig_log_level = TeeGetLogLevel(&Handle);
	prev_log_level = TeeSetLogLevel(&Handle, TEE_LOG_LEVEL_VERBOSE);
	new_log_level = TeeGetLogLevel(&Handle);

        ASSERT_EQ(orig_log_level, prev_log_level);
	ASSERT_EQ(new_log_level, TEE_LOG_LEVEL_VERBOSE);

	prev_log_level = TeeSetLogLevel(&Handle, orig_log_level);
	ASSERT_EQ(prev_log_level, TEE_LOG_LEVEL_VERBOSE);

	new_log_level = TeeGetLogLevel(&Handle);
	ASSERT_EQ(orig_log_level, new_log_level);

}

void MeTeeTEST_Log(bool is_error, const char* fmt, ...)
{
	EXPECT_EQ(is_error, true);
	EXPECT_STREQ(fmt, "TEELIB: (%s:%s():%d) TestTestTest");
}

TEST_P(MeTeeTEST, PROD_MKHI_SetLogCallback)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	uint32_t prev_log_level;
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	prev_log_level = TeeSetLogLevel(&Handle, TEE_LOG_LEVEL_ERROR);
	status = TeeSetLogCallback(&Handle, MeTeeTEST_Log);
	EXPECT_EQ(SUCCESS, status);
	ERRPRINT(&Handle, "TestTestTest");

	status = TeeSetLogCallback(&Handle, NULL);
	EXPECT_EQ(SUCCESS, status);
	ERRPRINT(&Handle, "NotReal");
	TeeSetLogLevel(&Handle, prev_log_level);

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}
/*
* Blocking read from side thread cancelled by disconnect from the main thread
*/
TEST_P(MeTeeTEST, PROD_MKHI_InterruptRead)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;
	std::thread thr;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	thr = std::thread([Handle]() {
		std::vector <char> MaxResponse;
		size_t NumberOfBytes = 0;
		MaxResponse.resize(Handle.maxMsgLen * sizeof(char));
		EXPECT_EQ(TEE_UNABLE_TO_COMPLETE_OPERATION, TeeRead((PTEEHANDLE) & Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 0));
	});
	thr.detach();
	std::this_thread::sleep_for(std::chrono::seconds(1));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Send GetVersion Command to MKHI with timeout and fd > 1024
1) Open 2000 file descriptors
2) Open Connection to MKHI
3) Send GetVersion Req Command
4) Receive GetVersion Resp Command
5) Check for Valid Resp
6) Close Connection
*/
TEST_P(MeTee1000OpenTEST, PROD_MKHI_1000HandlesGetVersion)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));


	MaxResponse.resize(Handle.maxMsgLen*sizeof(char));
	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 1000));
	ASSERT_EQ(sizeof(GEN_GET_FW_VERSION), NumberOfBytes);

	ASSERT_EQ(SUCCESS, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 1000));
	pResponseMessage = (GEN_GET_FW_VERSION_ACK*)(&MaxResponse[0]);

	ASSERT_EQ(SUCCESS, pResponseMessage->Header.Fields.Result);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
	EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Send stress of GetVersion Command to HCI / MKHI
1) Open Connection to MKHI
2) Send GetVersion Req Command
3) Receive GetVersion Resp Command
4) Check for Valid Resp
5) Close Connection
*/
TEST_P(MeTeeTEST, PROD_MKHI_SimpleGetVersionStress)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	MaxResponse.resize(Handle.maxMsgLen * sizeof(char));
	for (unsigned int i = 0; i < 1000; i++) {
		ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));
		ASSERT_EQ(sizeof(GEN_GET_FW_VERSION), NumberOfBytes);

		ASSERT_EQ(SUCCESS, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 0));
		pResponseMessage = (GEN_GET_FW_VERSION_ACK*)(&MaxResponse[0]);

		ASSERT_EQ(SUCCESS, pResponseMessage->Header.Fields.Result);
		EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeMajor);
		EXPECT_NE(0, pResponseMessage->Data.FWVersion.CodeBuildNo);
	}
	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

#ifndef WIN32
/*
Send pending write stress
1) Connect to a client(MKHI)
2) Send stress of valid Write Command (async using TeeWrite)
3) Call Disconnect()
*/
TEST_P(MeTeeTEST, PROD_MKHI_PendingWriteStress)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	for (unsigned int i = 0; i < 51; i++)
		EXPECT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 1000));
	for (unsigned int i = 0; i < 2; i++)
		EXPECT_EQ(TEE_TIMEOUT, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 1000));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}
#endif // not WIN32

TEST_P(MeTeeTEST, PROD_MKHI_SimpleGetVersionNULLReturn)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));


	MaxResponse.resize(Handle.maxMsgLen*sizeof(char));
	ASSERT_EQ(SUCCESS, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), NULL, 0));

	ASSERT_EQ(SUCCESS, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, NULL, 0));
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
2) Receive timeout on GetVersion Resp Command
3) Close Connection
*/
TEST_P(MeTeeTEST, PROD_MKHI_TimeoutGetVersion)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));

	MaxResponse.resize(Handle.maxMsgLen*sizeof(char));

	EXPECT_EQ(TEE_TIMEOUT, TeeRead(&Handle, &MaxResponse[0], Handle.maxMsgLen, &NumberOfBytes, 1000));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

/*
Obtain FW status
1) Receive FW status
2) Check for Valid Resp
*/
TEST_P(MeTeeTEST, PROD_MKHI_GetFWStatus)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	uint32_t fwStatusNum;
	uint32_t fwStatus;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);
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

/*
 * GetTRC API
 * 1) Receive TRC
 * 2) Check for bad input
*/
TEST_P(MeTeeTEST, PROD_MKHI_GetTRC)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	uint32_t trcVal;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	EXPECT_THAT(TeeGetTRC(&Handle, &trcVal),
		    testing::AnyOf(testing::Eq(TEE_SUCCESS), testing::Eq(TEE_NOTSUPPORTED)));

	//Invalid input
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeGetTRC(NULL, &trcVal));
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeGetTRC(&Handle, NULL));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

TEST_P(MeTeeTEST, PROD_MKHI_DoubleConnect)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
	ASSERT_EQ(SUCCESS, TeeConnect(&Handle));
	ASSERT_EQ(TEE_INTERNAL_ERROR, TeeConnect(&Handle));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

TEST_P(MeTeeTEST, PROD_MKHI_WriteReadNoConnect)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	TEESTATUS status;

	status = TestTeeInitGUID(&Handle, intf.client, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	ASSERT_EQ(TEE_DISCONNECTED, TeeWrite(&Handle, &MkhiRequest, sizeof(GEN_GET_FW_VERSION), &NumberOfBytes, 0));

	MaxResponse.resize(1);
	ASSERT_EQ(TEE_DISCONNECTED, TeeRead(&Handle, &MaxResponse[0], 1, &NumberOfBytes, 0));

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}

TEST_P(MeTeeNTEST, PROD_N_TestConnectToNullUuid)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();

	ASSERT_EQ(TEE_INVALID_PARAMETER, TestTeeInitGUID(&handle, NULL, intf.device));
}

TEST_P(MeTeeNTEST, PROD_N_TestConnectToNonExistsUuid)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TestTeeInitGUID(&handle, &GUID_NON_EXISTS_CLIENT, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);

	ASSERT_EQ(TEE_CLIENT_NOT_FOUND, TeeConnect(&handle));
}

TEST_P(MeTeeNTEST, PROD_N_TestLongDevicePath)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	const char *longPath = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

	ASSERT_EQ(TEE_DEVICE_NOT_FOUND, TeeInit(&handle, &GUID_NON_EXISTS_CLIENT, longPath));
}

TEST_P(MeTeeNTEST, PROD_N_TestLongClientPath)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;
	const char *longPath = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

	status = TestTeeInitGUID(&handle, (const GUID*)longPath, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);

	ASSERT_EQ(TEE_CLIENT_NOT_FOUND, TeeConnect(&handle));
}

TEST_P(MeTeeNTEST, PROD_N_TestGetDriverVersion)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	teeDriverVersion_t ver = {0, 0, 0, 0};
	TEESTATUS status;

	status = TestTeeInitGUID(&handle, &GUID_NON_EXISTS_CLIENT, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);

#ifdef WIN32
	ASSERT_EQ(TEE_SUCCESS, GetDriverVersion(&handle, &ver));

	EXPECT_NE(ver.major, 0);
	// All other may be zero
#else // WIN32
	ASSERT_EQ(TEE_NOTSUPPORTED, GetDriverVersion(&handle, &ver));
#endif // WIN32
}

TEST_P(MeTeeNTEST, PROD_N_TestGetDriverVersion_NullParam)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TestTeeInitGUID(&handle, &GUID_NON_EXISTS_CLIENT, intf.device);
	if (status == TEE_DEVICE_NOT_FOUND)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, status);

	ASSERT_EQ(TEE_INVALID_PARAMETER, GetDriverVersion(&handle, NULL));
}

#ifdef WIN32
TEST_P(MeTeeNTEST, PROD_N_TestConnectByPath)
{
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;
	char devicePath[MAX_PATH] = {0};
	TEEHANDLE handle;

	__tee_init_handle(&handle);

	status = GetDevicePath(&handle, (intf.device) ? intf.device : &GUID_DEVINTERFACE_HECI, devicePath, MAX_PATH);
	if (status)
		GTEST_SKIP();
	ASSERT_EQ(TEE_SUCCESS, TeeInit(&handle, intf.client, devicePath));
}
#endif // WIN32

TEST_P(MeTeeNTEST, PROD_N_TestConnectByWrongPath)
{
	TEEHANDLE handle = TEEHANDLE_ZERO;

	ASSERT_EQ(TEE_DEVICE_NOT_FOUND, TeeInit(&handle, &GUID_NON_EXISTS_CLIENT, "\\NO_SUCH_DEVICE"));
}

TEST_P(MeTeeDataNTEST, PROD_N_TestFWUNullBufferWrite)
{
	size_t numOfBytes = 0;
	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeWrite(&_handle, NULL, 1024, &numOfBytes, 0));
}

TEST_P(MeTeeDataNTEST, PROD_N_TestFWUZeroBufferSizeWrite)
{
	size_t numOfBytes = 0;
	std::vector<unsigned char> buf(1024);

	ASSERT_EQ(TEE_INVALID_PARAMETER, TeeWrite(&_handle, &buf[0], 0, &numOfBytes, 0));
}

TEST_P(MeTeeDataNTEST, PROD_N_TestFWUBiggerThenMtuWrite)
{
	size_t numOfBytes = 0;

	std::vector<unsigned char> buf(_handle.maxMsgLen + 10);

	ASSERT_EQ(TEE_INTERNAL_ERROR, TeeWrite(&_handle, &buf[0], buf.size(), &numOfBytes, 0));
}

#ifdef WIN32
TEST_P(MeTeeDataNTEST, PROD_N_TestSmallBufferRead)
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

TEST_P(MeTeeFDTEST, PROD_MKHI_SimpleGetVersion)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	size_t NumberOfBytes = 0;
	struct MeTeeTESTParams intf = GetParam();
	std::vector <char> MaxResponse;
	GEN_GET_FW_VERSION_ACK* pResponseMessage; //max length for this client is 2048
	TEESTATUS status;

	status = TeeInitHandle(&Handle, intf.client, deviceHandle);
	ASSERT_EQ(SUCCESS, status);
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

TEST_P(MeTeeFDTEST, PROD_MKHI_GetFWStatus)
{
	TEEHANDLE Handle = TEEHANDLE_ZERO;
	uint32_t fwStatusNum;
	uint32_t fwStatus;
	struct MeTeeTESTParams intf = GetParam();
	TEESTATUS status;

	status = TeeInitHandle(&Handle, intf.client, deviceHandle);
	ASSERT_EQ(TEE_SUCCESS, status);
	ASSERT_NE(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));

	//FWSTS1
	fwStatusNum = 0;
	ASSERT_EQ(SUCCESS, TeeFWStatus(&Handle, fwStatusNum, &fwStatus));
	EXPECT_NE(0, fwStatus);

	TeeDisconnect(&Handle);
	EXPECT_EQ(TEE_INVALID_DEVICE_HANDLE, TeeGetDeviceHandle(&Handle));
}


struct MeTeeTESTParams interfaces[1] = {
	{"PCH", NULL, &GUID_DEVINTERFACE_MKHI}};

INSTANTIATE_TEST_SUITE_P(MeTeeTESTInstance, MeTeeTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTeeTEST::ParamType>& info) {
			return info.param.name;
		});

INSTANTIATE_TEST_SUITE_P(MeTeeNTESTInstance, MeTeeNTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTeeNTEST::ParamType>& info) {
			return info.param.name;
		});

INSTANTIATE_TEST_SUITE_P(MeTeeDataNTESTInstance, MeTeeDataNTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTeeDataNTEST::ParamType>& info) {
			return info.param.name;
		});

INSTANTIATE_TEST_SUITE_P(MeTeeFDTESTInstance, MeTeeFDTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTeeFDTEST::ParamType>& info) {
			return info.param.name;
		});

INSTANTIATE_TEST_SUITE_P(MeTee1000OpenTESTInstance, MeTee1000OpenTEST,
		testing::ValuesIn(interfaces),
		[](const testing::TestParamInfo<MeTee1000OpenTEST::ParamType>& info) {
			return info.param.name;
		});
