using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading;
using ReplicationEngine;
using Microsoft.Test.Storages;
using FluentAssertions;
using System.IO;
using InMageLogger;
using System.Threading.Tasks;
using System.Diagnostics;

namespace Microsoft.Test.Inmage.DriverMode
{
    [TestClass]
    public abstract class DriverModes : ReplicationSetup
    {
        public enum DriverMode { Data, MetaData, BitMap};

        public static DriverMode driverState;

        private static TestContext tContext;
        public static TestContext TestContext
        {
            get { return tContext; }
            set { tContext = value; }
        }

        [ClassInitialize]
        public static void DriverModeTestsClassInitialize(TestContext context, DriverMode state, int numDisks, string logFile)
        {
            tContext = context;
            driverState = state;

            CreateDir(@"C:\tmp");
            s_logger = new Logger(logFile, true);
            s_logger.LogInfo("Starting tests");

            InitSrcTgtDiskMap(numDisks);
        }

        [ClassCleanup]
        public static void DriverModeTestsClassCleanUp()
        {
            s_logger.LogInfo("Test " + tContext.TestName + " result : " + tContext.CurrentTestOutcome);
            s_logger.Close();
        }

        public void StartAndValidateWrites(UInt32 numSectors, bool unaligned = false)
        {
            Parallel.ForEach(m_srcTargetMap.Keys, (disk) =>
            {
                UInt32 bufferSize = numSectors * disk.BytesPerSector;
                UInt64 startOffset = (unaligned) ? (UInt64)512 : bufferSize;

                disk.Online(readOnly: false);
                WriteDisk(disk.DiskNumber, startOffset, disk.Size, bufferSize);
                disk.Offline(readOnly: true);
            }
            );

            if (driverState == DriverMode.MetaData)
            {
                s_logger.LogInfo("Reset Driver State");
                SetDriverFlags(s_s2Agent, BitOperation.BitOpReset);
            }
            else if (driverState == DriverMode.BitMap)
            {
                s_logger.LogInfo("Starting InMage Agent");
                StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
            }

            ValidateDisks(tContext.TestName);
            OnlineDisks(false);
        }

        public void WriteDisk(int diskIndex, UInt64 startOffset, UInt64 maxSize, UInt32 bufferSize)
        {
            if (0 == bufferSize)
            {
                throw new ArgumentException("bufferSize", "WriteDisk: BufferSize is 0");
            }
            var Buffer = new byte[bufferSize];
            Random random = new Random();

            PhysicalDrive disk = new PhysicalDrive(diskIndex);
            UInt64 currentOffset = startOffset;

            Stopwatch stopw = Stopwatch.StartNew();

            ulong totalBytes = 0;
            while (currentOffset < maxSize)
            {

                UInt32 bytesToWrite = ((maxSize - currentOffset) < (UInt64)bufferSize) ? (UInt32)(maxSize - currentOffset) : bufferSize;

                random.NextBytes(Buffer);
                //Console.WriteLine("currentOffset - {0}, bytesToWrite - {1}, buffer - {2}, BufferSize - {3}", currentOffset, bytesToWrite, Buffer, bufferSize);
                try
                {
                    disk.Write(Buffer, bytesToWrite, currentOffset);
                    totalBytes += bytesToWrite;
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Error - " + ex.ToString());
                }
                currentOffset += bytesToWrite;

                if (totalBytes > (30 * 1024 * 1024))
                {
                    long timeSpent = stopw.ElapsedMilliseconds;
                    if (timeSpent < 1000)
                    {
                        Thread.Sleep((int)(1000 - timeSpent));
                    }
                    totalBytes = 0;
                    stopw.Reset();
                }
            }
        }
    }
}

