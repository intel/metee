/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2021-2025 Intel Corporation
 */
/*! \file meteepp.h
	\brief metee C++ library API
 */
#ifndef _METEEPP_H_
#define _METEEPP_H_

#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include "metee.h"

namespace intel {
	namespace security {

		/*! \def TEE_ERR_STATE(state)
		 * Internal macro
		 */

		 /*! MeTee error category class */
		static class metee_category_t : public std::error_category {
		public:
			virtual const char* name() const noexcept { return "MeTee"; }
			virtual std::string message(int ev) const {
#define TEE_ERR_STATE(state) case TEE_##state: return #state
				switch (ev) {
					TEE_ERR_STATE(SUCCESS);
					TEE_ERR_STATE(INTERNAL_ERROR);
					TEE_ERR_STATE(DEVICE_NOT_FOUND);
					TEE_ERR_STATE(DEVICE_NOT_READY);
					TEE_ERR_STATE(INVALID_PARAMETER);
					TEE_ERR_STATE(UNABLE_TO_COMPLETE_OPERATION);
					TEE_ERR_STATE(TIMEOUT);
					TEE_ERR_STATE(NOTSUPPORTED);
					TEE_ERR_STATE(CLIENT_NOT_FOUND);
					TEE_ERR_STATE(BUSY);
					TEE_ERR_STATE(DISCONNECTED);
					TEE_ERR_STATE(INSUFFICIENT_BUFFER);
					TEE_ERR_STATE(PERMISSION_DENIED);
				default:
					return std::to_string(ev);
				}
#undef TEE_ERR_STATE
			}
		} metee_category;

		/*! MeTee error exception class class */
		class metee_exception : public std::system_error
		{
		public:
			/*! Constructor
			 *  \param what error string
			 *  \param err error code
			 */
			metee_exception(const std::string& what, int err = TEE_INTERNAL_ERROR) : std::system_error(err, metee_category, what) {}
			/*! Constructor
			 *  \param err error code
			 *  \param cat error category
			 */
			metee_exception(int err, const std::error_category& cat) : std::system_error(err, cat) {}
			/*! Constructor
			 *  \param err error code
			 *  \param cat error category
			 *  \param what error string
			 */
			metee_exception(int err, const std::error_category& cat, const std::string& what)
				: std::system_error(err, cat, what) {}
			/*! Destructor */
			virtual ~metee_exception() noexcept {}
		};

		/*! Dummy client GUID for default constructor */
		DEFINE_GUID(METEE_GUID_ZERO,
			0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

		/*! Main interface class
		 * \brief C++ class to access CSE/CSME/GSC firmware via a mei interface.
		 */
		class metee
		{
		public:
			/*! Default constructor, when connection to specific client is not required */
			metee() : metee(METEE_GUID_ZERO) {}

			/*! Constructor without client GUID, when connection to specific client is not required
			 *  \param log_level log level to set (from enum tee_log_level)
			 *  \param log_callback pointer to function to run for log write (type 2)
			 */
			metee(uint32_t log_level, TeeLogCallback2 log_callback) : metee(METEE_GUID_ZERO, log_level, log_callback) {}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 */
			metee(const GUID& guid)
			{
				TEESTATUS status = TeeInit(&_handle, &guid, nullptr);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Init failed", status);
				}
			}
			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param log_level log level to set (from enum tee_log_level)
			 */
			metee(const GUID& guid, uint32_t log_level) : metee(guid, log_level, static_cast<TeeLogCallback2>(nullptr)) {}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param log_level log level to set (from enum tee_log_level)
			 *  \param log_callback pointer to function to run for log write
			 */
			metee(const GUID& guid, uint32_t log_level, TeeLogCallback log_callback) :
				metee(guid, { tee_device_address::TEE_DEVICE_TYPE_NONE , nullptr }, log_level, log_callback) {}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param log_level log level to set (from enum tee_log_level)
			 *  \param log_callback pointer to function to run for log write (type 2)
			 */
			metee(const GUID& guid, uint32_t log_level, TeeLogCallback2 log_callback) :
				metee(guid, { tee_device_address::TEE_DEVICE_TYPE_NONE , nullptr }, log_level, log_callback) {}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param device device address structure
			 *  \param log_level log level to set (from enum tee_log_level)
			 */
			metee(const GUID& guid, const struct tee_device_address& device, uint32_t log_level) :
				metee(guid, device, log_level, static_cast<TeeLogCallback2>(nullptr)) {}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param device device address structure
			 *  \param log_level log level to set (from enum tee_log_level)
			 *  \param log_callback pointer to function to run for log write
			 */
			metee(const GUID &guid, const struct tee_device_address &device, uint32_t log_level, TeeLogCallback log_callback)
			{
				TEESTATUS status = TeeInitFull(&_handle, &guid, device, log_level, log_callback);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Init failed", status);
				}
			}

			/*! Constructor
			 *  \param guid GUID of the FW client that want to start a session
			 *  \param device device address structure
			 *  \param log_level log level to set (from enum tee_log_level)
			 *  \param log_callback pointer to function to run for log write (type 2)
			 */
			metee(const GUID& guid, const struct tee_device_address& device, uint32_t log_level, TeeLogCallback2 log_callback)
			{
				TEESTATUS status = TeeInitFull2(&_handle, &guid, device, log_level, log_callback);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Init failed", status);
				}
			}

			/*! Copy constructor - disabled */
			metee(const metee& other) = delete;

			/*! Move constructor
			 *  \param other Object to move from
			 */
			metee(metee&& other) noexcept : _handle(other._handle)
			{
				other._handle.handle = nullptr;
			}

			/*! Copy operator - disabled */
			metee& operator=(const metee& other) = delete;

			/*! Move operator
			 *  \param other Object to move from
			 */
			metee& operator=(metee&& other) noexcept
			{
				TeeDisconnect(&_handle);
				_handle = other._handle;
				other._handle.handle = nullptr;
				return *this;
			}

			/*! Destructor, disconnects, if connected */
			virtual ~metee()
			{
				TeeDisconnect(&_handle);
			}

			/*! Try to stop all in-process reads and writes */
			void cancel_io()
			{
				TeeCancelIO(&_handle);
			}

			/*! Connects to the TEE driver and starts a session */
			void connect()
			{
				TEESTATUS status;

				status = TeeConnect(&_handle);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Connect failed", status);
				}
			}

			/*! Read data from the TEE device synchronously.
			 *  \param timeout The timeout to complete read in milliseconds, zero for infinite
			 *  \return vector with data read from the TEE device
			 */
			std::vector<uint8_t> read(uint32_t timeout)
			{
				TEESTATUS status;
				size_t size = 0;
				std::vector<uint8_t> buffer(max_msg_len());

				status = TeeRead(&_handle, buffer.data(), buffer.size(), &size, timeout);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Read failed", status);
				}

				buffer.resize(size);
				return std::move(buffer);
			}

			/*! Writes the specified buffer to the TEE device synchronously.
			 *  \param buffer vector containing the data to be written to the TEE device.
			 *  \param timeout The timeout to complete write in milliseconds, zero for infinite
			 *  \return the number of bytes written
			 */
			size_t write(const std::vector<uint8_t> &buffer, uint32_t timeout)
			{
				TEESTATUS status;
				size_t size = 0;

				status = TeeWrite(&_handle, buffer.data(), buffer.size(), &size, timeout);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("Write failed", status);
				}

				return size;
			}

			/*! Retrieves specified FW status register.
			 *  \param fwStatusNum The FW status register number (0-5).
			 *  \return obtained FW status.
			 */
			uint32_t fw_status(uint32_t fwStatusNum)
			{
				TEESTATUS status;
				uint32_t fwStatus = 0;

				status = TeeFWStatus(&_handle, fwStatusNum, &fwStatus);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("FWStatus failed", status);
				}

				return fwStatus;
			}

			/*! Retrieves TRC register.
			 *  \return TRC value.
			 */
			uint32_t trc()
			{
				TEESTATUS status;
				uint32_t trc_val = 0;

				status = TeeGetTRC(&_handle, &trc_val);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("GetTRC failed", status);
				}

				return trc_val;
			}

			/*! Retrieves device kind.
			*  \return kind string value.
			*/
			std::string kind()
			{
				TEESTATUS status;
				const size_t KIND_SIZE = 32;
				char kind[KIND_SIZE];
				size_t kind_size = KIND_SIZE;

				status = TeeGetKind(&_handle, kind, &kind_size);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("TeeGetKind failed", status);
				}

				return kind;
			}
			/*! Set log level
			 *
			 *  \param log_level log level to set
			 *  \return previous log level
			 */
			uint32_t log_level(uint32_t log_level)
			{
				return TeeSetLogLevel(&_handle, log_level);
			}

			/*! Retrieve current log level
			 *
			 *  \return current log level
			 */
			uint32_t log_level()
			{
				return TeeGetLogLevel(&_handle);
			}

			/*! Set log callback
			 *
			 *  \param log_callback pointer to function to run for log write, set NULL to use built-in function
			 */
			void log_callback(TeeLogCallback log_callback)
			{
				TeeSetLogCallback(&_handle, log_callback);
			}

			/*! Retrieve client maximum message length (MTU)
			 *
			 *  \return client maximum message length.
			 */
			uint32_t max_msg_len()
			{
				return TeeGetMaxMsgLen(&_handle);
			}

			/*! Retrieve client protocol version
			 *
			 *  \return client protocol version.
			 */
			uint8_t protocol_ver()
			{
				return TeeGetProtocolVer(&_handle);
			}

			/*! Returns handle of TEE device
			 *  Obtains HECI device handle on Windows and mei device file descriptor on Linux
			 *  \return the handle of the session.
			 */
			TEE_DEVICE_HANDLE device_handle()
			{
				return TeeGetDeviceHandle(&_handle);
			}

			/*! Obtains version of the TEE device driver
			 *  Not implemented on Linux
			 *  \return Driver version as dotted string.
			 */
			std::string driver_version()
			{
				TEESTATUS status;
				teeDriverVersion_t driverVersion = { 0 };

				status = ::GetDriverVersion(&_handle, &driverVersion);
				if (!TEE_IS_SUCCESS(status)) {
					throw metee_exception("GetDriverVersion failed", status);
				}

				std::stringstream ss;
				ss << driverVersion.major << "."
					<< driverVersion.minor << "."
					<< driverVersion.hotfix << "."
					<< driverVersion.build;
				return ss.str();
			}

		private:
			_TEEHANDLE _handle; /*!< Internal device handle */
		};
	} // namespace security
} // namespace intel
#endif // _METEEPP_H_
