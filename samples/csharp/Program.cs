/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2025 Intel Corporation
 */
using Intel.Security;
using System;

namespace metee_csharp_sample
{
    internal class Program
    {
        static void PrintFwVersion(byte[] data)
        {
            var version = new FwVersionResponse(data);
            Console.WriteLine($"CodeMinor: {version.CodeMinor}");
            Console.WriteLine($"CodeMajor: {version.CodeMajor}");
            Console.WriteLine($"CodeBuildNo: {version.CodeBuildNo}");
            Console.WriteLine($"CodeHotFix: {version.CodeHotFix}");
        }

        const byte MkhiGetFwVersionCommandId = 0x02;
        const byte MkhiGenGroupId = 0xFF;

        class FwMkhiCommandRequest
        {
            private int _data;

            public int GroupId
            {
                set => _data |= value;
            }
            public int Command { set => _data |= (value << 8); }

            public byte[] Data => BitConverter.GetBytes(_data);
        }

        class FwVersionRequest : FwMkhiCommandRequest
        {
            public FwVersionRequest()
            {
                GroupId = MkhiGenGroupId;
                Command = MkhiGetFwVersionCommandId;
            }
        }

        class FwMkhiCommandResponse
        {
            protected const int ResultOffset = 4;

            protected readonly byte[] Data;

            protected FwMkhiCommandResponse(byte[] data)
            {
                Data = data;
                if (Data.Length < ResultOffset)
                    throw new ArgumentException("Data length is too short");
            }
        }

        class FwVersionResponse : FwMkhiCommandResponse
        {
            public FwVersionResponse(byte[] data) : base(data)
            {
                if (Data.Length < (ResultOffset + 8))
                    throw new ArgumentException("Data length is too short");
            }

            public int CodeMinor => BitConverter.ToInt16(Data, ResultOffset);
            public int CodeMajor => BitConverter.ToInt16(Data, ResultOffset + 2);
            public int CodeBuildNo => BitConverter.ToInt16(Data, ResultOffset + 4);
            public int CodeHotFix => BitConverter.ToInt16(Data, ResultOffset + 6);
        }

        static byte[] BasicMeTeeFlow(Metee metee)
        {
            Console.WriteLine($"Driver Version {metee.DriverVersion}");
            Console.WriteLine($"TRC {metee.Trc}");
            Console.WriteLine($"FWSTS 1 0x{metee.FwStatus[Metee.RegIndex.FwStatus1]:X8}");
            Console.WriteLine($"FWSTS 2 0x{metee.FwStatus[Metee.RegIndex.FwStatus2]:X8}");

            metee.Connect();

            Console.WriteLine($"Protocol Version {metee.ProtocolVersion}");
            Console.WriteLine($"MaxMsgLen {metee.MaxMsgLen}");

            metee.Write(new FwVersionRequest().Data, Int32.MaxValue);
            return metee.Read(Int32.MaxValue);
        }

        static void Main(string[] args)
        {
            var mkhiClient = new Guid(0x8e6a6715, 0x9abc, 0x4043, 0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f);

            try
            {
                var teeDriverInterface = new Guid(0xE2D1FF34, 0x3458, 0x49A9,
                    0x88, 0xDA, 0x8E, 0x69, 0x15, 0xCE, 0x9B, 0xE5);
                using (var metee = new Metee(mkhiClient, teeDriverInterface))
                {
                    var res = BasicMeTeeFlow(metee);
                    PrintFwVersion(res);
                }

                using (var metee = new Metee(mkhiClient))
                {
                    var res = BasicMeTeeFlow(metee);
                    PrintFwVersion(res);
                }

                // This code demonstrates how to use MeTee with a given device path.
                // Keep in mind that the device path may vary across different systems.
                //
                // var pathToTeeDriver = @"\\?\PCI#VEN_8086..HERE-GOES-YOUR-DEVICE-PATH...";
                // using (var metee = new Metee(mkhiClient, pathToTeeDriver))
                // {
                //    var res = BasicMeTeeFlow(metee);
                //    PrintFwVersion(res);
                // }
            }
            catch (DllNotFoundException e)
            {
                // Ensure that metee.dll is in the same directory as the executable
                Console.WriteLine(e);
            }
        }
    }
}