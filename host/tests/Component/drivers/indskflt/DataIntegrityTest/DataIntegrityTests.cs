using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using Microsoft.Test.Storages;
using ManagedBlockValidator;
using FluentAssertions;
using System.Threading;
using System.IO;
using System.Collections.Generic;
using System.Management;
using System.Text;
using InMageLogger;
using ReplicationEngine;
using System.Diagnostics;
using System.Threading.Tasks;

namespace DataIntegrityTest
{
    [TestClass]
    public class DataIntegrityTests : ReplicationSetup
    {
        private static TestContext testContextInstance;
        public TestContext TestContext
        {
            get
            {
                return testContextInstance;
            }
            set
            {
                testContextInstance = value;
            }
        }

        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            string BaseLogDir = (string)context.Properties["LoggingDirectory"];
            s_logDir = Path.Combine(BaseLogDir, "DataIntegrityTests", context.TestName);
            CreateDir(s_logDir);
            string LogFilePath = Path.Combine(s_logDir, context.TestName + @".log");
            s_logger = new Logger(LogFilePath, true);
            InitSrcTgtDiskMap(1);
            InitializeReplication(context.TestName, 0, 1024 * 1024);
            StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
        }

        [ClassCleanup]
        public static void CleanupS2()
        {
            CleanupReplication();
        }

        [Ignore]
        [TestMethod]
        public void DIFullDiskTesForever()
        {
            for (; ; )
            {
                Thread.Sleep(30 * 60 * 1000);
                foreach (var disk in m_srcTargetMap.Keys)
                {
                    disk.Offline(readOnly: true);
                    S2AgentWaitForConsistentState(s_s2Agent);
                    BlockValidator.Validate(disk, m_srcTargetMap[disk], disk.Size, s_logger).Should().BeTrue();
                    // Online disk back
                    disk.Online(readOnly: false);
                }
            }
        }

        [TestMethod]
        public void DIFullDiskTestofSectorSizedWrites()
        {
            StartAndValidateWrites(1);
        }

        [TestMethod]
        public void DIFullDiskTestof1KSizedWrites()
        {
            StartAndValidateWrites(2);
        }


        [TestMethod]
        public void DIFullDiskTestof1KSizedUnalignedWrites()
        {
            StartAndValidateWrites(2, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4KSizedWrites()
        {
            StartAndValidateWrites(8);
        }

        [TestMethod]
        public void DIFullDiskTestof4KSizedUnalignedWrites()
        {
            StartAndValidateWrites(8, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8KSizedWrites()
        {
            StartAndValidateWrites(16);
        }

        [TestMethod]
        public void DIFullDiskTestof8KSizedUnalignedWrites()
        {
            StartAndValidateWrites(16, true);
        }

        [TestMethod]
        public void DIFullDiskTestof16KSizedWrites()
        {
            StartAndValidateWrites(32);
        }

        [TestMethod]
        public void DIFullDiskTestof16KSizedUnalignedWrites()
        {
            StartAndValidateWrites(32, true);
        }

        [TestMethod]
        public void DIFullDiskTestof32KSizedWrites()
        {
            StartAndValidateWrites(64);
        }

        [TestMethod]
        public void DIFullDiskTestof32KSizedUnalignedWrites()
        {
            StartAndValidateWrites(64, true);
        }

        [TestMethod]
        public void DIFullDiskTestof64KSizedWrites()
        {
            StartAndValidateWrites(128);
        }

        [TestMethod]
        public void DIFullDiskTestof64KSizedUnalignedWrites()
        {
            StartAndValidateWrites(128, true);
        }


        [TestMethod]
        public void DIFullDiskTestof128KSizedWrites()
        {
            StartAndValidateWrites(256);
        }

        [TestMethod]
        public void DIFullDiskTestof128KSizedUnalignedWrites()
        {
            StartAndValidateWrites(256, true);
        }

        [TestMethod]
        public void DIFullDiskTestof256KSizedWrites()
        {
            StartAndValidateWrites(512);
        }

        [TestMethod]
        public void DIFullDiskTestof256KSizedUnalignedWrites()
        {
            StartAndValidateWrites(512, true);
        }

        [TestMethod]
        public void DIFullDiskTestof18KSizedWrites()
        {
            StartAndValidateWrites(18432, true);
        }

        [TestMethod]
        public void DIFullDiskTestof1MSizedWrites()
        {
            StartAndValidateWrites(2 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof1MSizedUnalignedWrites()
        {
            StartAndValidateWrites(2 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof2MSizedWrites()
        {
            StartAndValidateWrites(4096, true);
        }

        [TestMethod]
        public void DIFullDiskTestof2MSizedUnalignedWrites()
        {
            StartAndValidateWrites(4096, true);
        }

        [TestMethod]
        public void DIFullDiskTestof3MSizedWrites()
        {
            StartAndValidateWrites(6 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof3MSizedUnalignedWrites()
        {
            StartAndValidateWrites(6 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4MSizedWrites()
        {
            StartAndValidateWrites(8 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4MSizedUnalignedWrites()
        {
            StartAndValidateWrites(8 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof5MSizedWrites()
        {
            StartAndValidateWrites(10 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof5MSizedUnalignedWrites()
        {
            StartAndValidateWrites(10 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof6MSizedWrites()
        {
            StartAndValidateWrites(12 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof6MSizedUnalignedWrites()
        {
            StartAndValidateWrites(12 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof7MSizedWrites()
        {
            StartAndValidateWrites(14 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof7MSizedUnalignedWrites()
        {
            StartAndValidateWrites(14 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8MSizedWrites()
        {
            StartAndValidateWrites(16384, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8MSizedUnalignedWrites()
        {
            StartAndValidateWrites(16384, true);
        }

        [TestMethod]
        public void DIFullDiskTestof9MSizedWrites()
        {
            StartAndValidateWrites(18 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof9MSizedUnalignedWrites()
        {
            StartAndValidateWrites(18 * 1024, true);
        }

        public void StartAndValidateWrites(UInt32 numSectors, bool unaligned = false)
        {
            Parallel.ForEach(m_srcTargetMap.Keys, (disk) =>
            {
                UInt32 bufferSize = numSectors * disk.BytesPerSector;
                UInt64 startOffset = (unaligned) ? (UInt64)512 : bufferSize;

                disk.Online(readOnly: false);
                s_logger.LogInfo("Onlined disk in read write mode");
                WriteDisk(disk.DiskNumber, startOffset, disk.Size, bufferSize);
                disk.Offline(readOnly: true);
                s_logger.LogInfo("Offlined disk in read only mode");
            }
            );
            ValidateDisks(TestContext.TestName);
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
                s_logger.LogInfo("currentOffset - " + currentOffset + ", bytesToWrite - " + bytesToWrite + ", buffer - " + Buffer + ", BufferSize - " + bufferSize);
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

                if (totalBytes > (30*1024*1024))
                {
                    long timeSpent = stopw.ElapsedMilliseconds;
                    if (timeSpent < 1000)
                    {
                        Thread.Sleep((int) (1000 - timeSpent));
                    }
                    totalBytes = 0;
                    stopw.Reset();
                }
            }
        }

    }
}
