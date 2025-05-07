/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2025 Intel Corporation
 */

using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace Intel.Security
{
    public class MeteeException : Exception
    {
        public TeeStatus ErrorCode { get; }

        public enum TeeStatus
        {
            TeeSuccess = 0x0000,
            TeeInternalError = 0x0001,
            TeeDeviceNotFound = 0x0002,
            TeeDeviceNotReady = 0x0003,
            TeeInvalidParameter = 0x0004,
            TeeUnableToCompleteOperation = 0x0005,
            TeeTimeout = 0x0006,
            TeeNotSupported = 0x0007,
            TeeClientNotFound = 0x0008,
            TeeBusy = 0x0009,
            TeeDisconnected = 0x000A,
            TeeInsufficientBuffer = 0x000B,
            TeePermissionDenied = 0x000C
        }

        public MeteeException(string message, int errorCode) : base(message)
        {
            ErrorCode = (TeeStatus)errorCode;
        }
    }

    public class Metee : IDisposable
    {

        // Reference: https://learn.microsoft.com/en-us/dotnet/standard/garbage-collection/implementing-dispose#implement-the-dispose-pattern-using-a-custom-safe-handle
        // Wraps the IntPtr allocated by Marshal.AllocHGlobal() into a SafeHandle.
        class LocalAllocHandle : SafeHandleZeroOrMinusOneIsInvalid
        {
            private LocalAllocHandle() : base(ownsHandle: true) { }

            // No need to implement a finalizer - SafeHandle's finalizer will call ReleaseHandle for you.
            protected override bool ReleaseHandle()
            {
                Marshal.FreeHGlobal(handle);
                return true;
            }

            // Allocate bytes with Marshal.AllocHGlobal() and wrap the result into a SafeHandle.
            public static LocalAllocHandle Allocate(int numberOfBytes)
            {
                IntPtr nativeHandle = Marshal.AllocHGlobal(numberOfBytes);
                LocalAllocHandle safeHandle = new LocalAllocHandle();
                safeHandle.SetHandle(nativeHandle);
                return safeHandle;
            }
        }

        private LocalAllocHandle _safeHandle = LocalAllocHandle.Allocate(Marshal.SizeOf<TeeHandle>());

        #region DllImport

        private enum TeeDeviceType
        {
            None = 0,
            Path = 1,
            Guid = 3,
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct TeeDeviceAddress : IDisposable
        {

            public TeeDeviceAddress(string devicePath)
            {
                DeviceType = TeeDeviceType.Path;
                DataPtr = Marshal.StringToHGlobalAnsi(devicePath);
            }

            public TeeDeviceAddress(Guid guid)
            {
                DeviceType = TeeDeviceType.Guid;
                DataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
                Marshal.StructureToPtr(guid, DataPtr, false);
            }

            [MarshalAs(UnmanagedType.I4)]
            public TeeDeviceType DeviceType;

            public IntPtr DataPtr;

            public void Dispose()
            {
                if (DataPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(DataPtr);
            }
        }

        // Struct should be big enough to hold the data
        // without depending on the padding in the C struct
        [StructLayout(LayoutKind.Sequential)]
        private struct TeeHandle
        {
            private readonly IntPtr Field1;
            private readonly IntPtr Field2;
            private readonly IntPtr Field3;
            private readonly IntPtr Field4;
            private readonly IntPtr Field5;
        }

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeInit(IntPtr handle, ref Guid guid, IntPtr device);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeInitFull(IntPtr handle, ref Guid guid, TeeDeviceAddress deviceAddress, [MarshalAs(UnmanagedType.U4)] TeeLogLevel logLevel, IntPtr logCallback);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeConnect(IntPtr handle);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeDisconnect(IntPtr handle);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeRead(IntPtr handle, byte[] buffer, int bufferSize, out int bytesRead, uint timeout);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeWrite(IntPtr handle, byte[] buffer, int bufferSize, out int bytesWritten, uint timeout);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeFWStatus(IntPtr handle, uint fwStatusNum, out uint fwStatus);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeGetTRC(IntPtr handle, out uint trc);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int TeeGetKind(IntPtr handle, StringBuilder kind, ref int kindSize);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern uint TeeSetLogLevel(IntPtr handle, uint logLevel);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern uint TeeGetLogLevel(IntPtr handle);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern uint TeeGetMaxMsgLen(IntPtr handle);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern uint TeeGetProtocolVer(IntPtr handle);

        [DllImport("metee.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetDriverVersion(IntPtr handle, out TeeDriverVersion driverVersion);

        [StructLayout(LayoutKind.Sequential)]
        private struct TeeDriverVersion
        {
            public readonly short Major;
            public readonly short Minor;
            public readonly short Hotfix;
            public readonly short Build;
        }

        #endregion

        public enum TeeLogLevel
        {
            Quiet = 0,   // no log prints
            Error = 1,   // error log prints
            Verbose = 2, // verbose log prints
        }

        /**
         * Use this constructor to query driver, without connecting to FW client.
         */
        public Metee()
        {
            Guid zeroGuid = Guid.Empty;
            int status = TeeInit(_safeHandle.DangerousGetHandle(), ref zeroGuid, IntPtr.Zero);
            if (status != 0)
            {
                throw new MeteeException("Init failed", status);
            }
            FwStatus = new FwSts(this);
        }

        /**
         * Use this constructor to connect to default driver instance.
         * Useful when only one instance of driver is present in the system.
         *
         * @param clientGuid - GUID of the FW client that want to start a session
         * @param logLevel - log level to set
         * @param logCallback - callback to be called on each log message
         */
        public Metee(Guid clientGuid, TeeLogLevel logLevel = TeeLogLevel.Quiet, IntPtr logCallback = default)
        {
            var device = new TeeDeviceAddress { DeviceType = TeeDeviceType.None, DataPtr = IntPtr.Zero };

            int status = TeeInitFull(_safeHandle.DangerousGetHandle(), ref clientGuid, device, logLevel, logCallback);
            if (status != 0)
            {
                throw new MeteeException("Init failed", status);
            }
            FwStatus = new FwSts(this);
        }

        /**
         * Use this constructor to connect to a specific driver instance.
         * This is particularly useful when multiple driver instances are present in the system
         * and you have the instance path.
         *
         * @param clientGuid - FW client GUID 
         * @param devicePath - Instance path
         * @param logLevel - log level to set
         * @param logCallback - callback to be called on each log message
         */
        public Metee(Guid clientGuid, string devicePath, TeeLogLevel logLevel = TeeLogLevel.Quiet, IntPtr logCallback = default)
        {
            int status = 1;
            using (var device = new TeeDeviceAddress(devicePath))
            {
                status = TeeInitFull(_safeHandle.DangerousGetHandle(), ref clientGuid, device, logLevel, logCallback);
            }
            if (status != 0)
            {
                throw new MeteeException("Init failed", status);
            }
            FwStatus = new FwSts(this);
        }

        /**
         * Use this constructor to connect to a specific driver instance.
         * This is particularly useful when multiple driver instances are present in the system
         * and you have the instance interface GUID.
         *
         * @param clientGuid - FW client GUID 
         * @param interfaceGuid - Instance interface GUID
         * @param logLevel - log level to set
         * @param logCallback - callback to be called on each log message
         */
        public Metee(Guid clientGuid, Guid interfaceGuid, TeeLogLevel logLevel = TeeLogLevel.Quiet, IntPtr logCallback = default)
        {
            int status = 1;
            using (var device = new TeeDeviceAddress(interfaceGuid))
            {
                status = TeeInitFull(_safeHandle.DangerousGetHandle(), ref clientGuid, device, logLevel,
                    logCallback);
            }
            if (status != 0)
            {
                throw new MeteeException("Init failed", status);
            }
            FwStatus = new FwSts(this);
        }

        /**
         * Connects to the TEE driver and starts a session
         */
        public void Connect()
        {
            int status = TeeConnect(_safeHandle.DangerousGetHandle());
            if (status != 0)
            {
                throw new MeteeException("Connect failed", status);
            }
        }

        /**
         * Read data from the TEE device synchronously.
         * @param timeout - timeout in milliseconds
         * @return - data read from the device
         */
        public byte[] Read(uint timeout)
        {
            var maxMsgLen = GetMaxMsgLen();
            var buffer = new byte[maxMsgLen];

            int status = TeeRead(_safeHandle.DangerousGetHandle(), buffer, buffer.Length, out var bytesRead, timeout);
            if (status != 0)
            {
                throw new MeteeException("Read failed", status);
            }

            Array.Resize(ref buffer, bytesRead);
            return buffer;
        }

        /**
         * Write data to the TEE device synchronously.
         * @param buffer - data to write
         * @param timeout - timeout in milliseconds
         * @return - number of bytes written
         */
        public uint Write(byte[] buffer, uint timeout)
        {
            int status = TeeWrite(_safeHandle.DangerousGetHandle(), buffer, buffer.Length, out var bytesWritten, timeout);
            if (status != 0)
            {
                throw new MeteeException("Write failed", status);
            }

            return (uint)bytesWritten;
        }

        #region Properties getters

        private uint GetFwStatus(RegIndex index)
        {
            int status = TeeFWStatus(_safeHandle.DangerousGetHandle(), (uint)index, out var fwStatus);
            if (status != 0)
            {
                throw new MeteeException("FWStatus failed", status);
            }

            return fwStatus;
        }
        private uint GetTrc()
        {
            int status = TeeGetTRC(_safeHandle.DangerousGetHandle(), out var trc);
            if (status != 0)
            {
                throw new MeteeException("GetTRC failed", status);
            }

            return trc;
        }

        private string GetKind()
        {
            int kindSize = 32;
            StringBuilder kind = new StringBuilder(kindSize);

            int status = TeeGetKind(_safeHandle.DangerousGetHandle(), kind, ref kindSize);
            if (status != 0)
            {
                throw new MeteeException("TeeGetKind failed", status);
            }

            return kind.ToString();
        }

        private uint SetLogLevel(uint logLevel)
        {
            return TeeSetLogLevel(_safeHandle.DangerousGetHandle(), logLevel);
        }

        private uint GetLogLevel()
        {
            return TeeGetLogLevel(_safeHandle.DangerousGetHandle());
        }

        private uint GetMaxMsgLen()
        {
            return TeeGetMaxMsgLen(_safeHandle.DangerousGetHandle());
        }

        private uint GetProtocolVer()
        {
            return TeeGetProtocolVer(_safeHandle.DangerousGetHandle());
        }

        private string GetDriverVersion()
        {
            int status = GetDriverVersion(_safeHandle.DangerousGetHandle(), out var driverVersion);
            if (status != 0)
            {
                throw new MeteeException("GetDriverVersion failed", status);
            }

            return $"{driverVersion.Major}.{driverVersion.Minor}.{driverVersion.Hotfix}.{driverVersion.Build}";
        }

        #endregion

        /**
         * Obtains kind of the TEE device 
         */
        public string Kind => GetKind();

        /**
         * Obtains version of the TEE device driver
         */
        public string DriverVersion => GetDriverVersion();

        /**
         * Retrieve client protocol version
         */
        public uint ProtocolVersion => GetProtocolVer();

        /**
         * Get/Set current log level
         */
        public uint LogLevel
        {
            get => GetLogLevel();
            set => SetLogLevel(value);
        }
        /**
         * Retrieve client maximum message length (MTU)
         */
        public uint MaxMsgLen => GetMaxMsgLen();

        /**
         * Retrieves TRC register.
         */
        public uint Trc => GetTrc();

        public enum RegIndex
        {
            FwStatus1 = 0,
            FwStatus2 = 1,
            FwStatus3 = 2,
            FwStatus4 = 3,
            FwStatus5 = 4,
            FwStatus6 = 5,
        }
        public class FwSts
        {
            private Metee _metee;
            internal FwSts(Metee metee)
            {
                _metee = metee;
            }

            public uint this[RegIndex index] => _metee.GetFwStatus(index);
        }

        /**
         * Retrieves specified FW status register.
         */
        public FwSts FwStatus;

        #region IDisposable

        private bool _isDisposed = false;

        /** 
         * Public implementation of Dispose pattern callable by consumers.
        */
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /**
         * Protected implementation of Dispose pattern.
         */ 
        protected virtual void Dispose(bool disposing)
        {
            if (!_isDisposed)
            {
                _isDisposed = true;

                if (disposing)
                {
                    if (_safeHandle != null)
                    {
                        TeeDisconnect(_safeHandle.DangerousGetHandle());
                        _safeHandle?.Dispose();
                        _safeHandle = null;
                    }
                }
            }
        }
        #endregion
    }
}