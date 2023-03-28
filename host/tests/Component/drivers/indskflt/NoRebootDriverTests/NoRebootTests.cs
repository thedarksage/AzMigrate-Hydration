using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Collections;
using Microsoft.Test.Storages;
using ManagedBlockValidator;
using System.Runtime.InteropServices;
using System.Management;
using System.Xml;
using ReplicationEngine;
using FluentAssertions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using InMageLogger;

namespace NoRebootDriverTests
{
    [TestClass]
    public class NoRebootTests : ReplicationSetup
    {
        public TestContext TestContext { get; set; }

        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            string BaseLogDir = (string)context.Properties["LoggingDirectory"];
            s_logDir = Path.Combine(BaseLogDir, "NoRebootDriverTests", context.TestName);
            CreateDir(s_logDir);
            string LogFilePath = s_logDir + @"\NoRebootDITests.txt";
            s_logger = new Logger(LogFilePath, true);
            InitSrcTgtDiskMap(1);
        }

        [TestCleanup]
        public void ClassCleanup()
        {
            if (null != s_logger)
            {
                s_logger.Close();
                s_logger = null;
            }
        }

        public static void StartReplication(UInt64 endOffset)
        {
            s_logger.LogInfo("Entered StartReplication");
            s_s2Agent = S2AgentCreate(0, s_logDir);
            
            foreach (var disk in m_srcTargetMap.Keys)
            {
                disk.Offline(true);
                StringBuilder target = new StringBuilder();
                target = new StringBuilder();
                target.AppendFormat(@"E:\diskfiles\disk{0}.dsk", disk.DiskNumber.ToString());
                StringBuilder strb = new StringBuilder();
                strb.AppendFormat(s_logDir + @"\PhysicalDrive{0}_{1}.log", disk.DiskNumber.ToString(), "StartReplication");
                S2AgentAddFileReplicationSettings(s_s2Agent, disk.DiskNumber, target, strb);
                endOffset = (endOffset != 0) ? disk.Size : 0;
            }

            S2AgentStartReplication(s_s2Agent, 0, endOffset);
            StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
            s_logger.LogInfo("End of StartReplication");
        }

        public static bool StartWrites(UInt32 numSectors)
        {
            var startTime = DateTime.Now;
            s_logger.LogInfo("ChurnGenerator  : startTime : " + startTime);
            bool status = false;
			
			while (DateTime.Now - startTime < TimeSpan.FromMinutes(10))
            {
				foreach (var disk in m_srcTargetMap.Keys)
				{
					UInt32 bufferSize = numSectors * disk.BytesPerSector;

					disk.Online(readOnly: false);
					status = WriteDisk(disk.DiskNumber, 512, disk.Size, bufferSize);
					if (!status) break;
					disk.Offline(readOnly: true);
				}
			}
			
            s_logger.LogInfo("Exiting ChurnGenerator  : endTime : " + DateTime.Now);

            return status;
        }

        public static bool WriteDisk(int diskIndex, UInt64 startOffset, UInt64 maxSize, UInt32 bufferSize)
        {
            bool status = true;

            var Buffer = new byte[bufferSize];
            Random random = new Random();

            PhysicalDrive disk = new PhysicalDrive(diskIndex);
            UInt64 currentOffset = startOffset;

            while (currentOffset < maxSize)
            {
                UInt32 bytesToWrite = ((maxSize - currentOffset) < (UInt64)bufferSize) ? (UInt32)(maxSize - currentOffset) : bufferSize;

                random.NextBytes(Buffer);
                try
                {
                    disk.Write(Buffer, bytesToWrite, currentOffset);
                }
                catch (Exception ex)
                {
                    s_logger.LogError("Disk Write operation failed at offset :" + currentOffset, ex);
                    status = false;
                }

                currentOffset += bytesToWrite;
            }

            return status;
        }

        public static bool MatchDI(string testName)
        {
            bool status = false;

            foreach (var disk in m_srcTargetMap.Keys)
            {
                s_logger.LogInfo("Validation started for test " + testName + " on disk - " + disk.DiskNumber);
                status = BlockValidator.Validate(disk, m_srcTargetMap[disk], disk.Size, s_logger, 512);
                string output = status ? "PASSED" : "FAILED";
                s_logger.LogInfo("" + testName + " : Validation " + output);
                if (!status) break;
            }

            return status;
        }

        public static bool Validate(string testName)
        {
            bool status = false;

            status = S2AgentWaitForConsistentState(s_s2Agent);
            if (!status) return false;

            status = MatchDI(testName);

            OnlineDisks(false);

            return status;
        }

        public static void CopyLogs(string sourcePath, string targetPath)
        {
            if (Directory.Exists(sourcePath))
            {
                if (!Directory.Exists(targetPath)) Directory.CreateDirectory(targetPath);

                foreach (var srcPath in Directory.GetFiles(sourcePath))
                {
                    File.Copy(srcPath, srcPath.Replace(sourcePath, targetPath));
                }
            }
        }

        [TestMethod]
        public void DoInitialSyncAndMatchDI()
        {
            s_logger.LogInfo("===================================================");
            s_logger.LogInfo("Start No Reboot Tests");

            // Do initial sync and start draining
            StartReplication(1);

            // Apply data along with draining in parallel
            s_logger.LogInfo("Apply Churn : 1");
            StartWrites(512).Should().BeTrue();

            // Validate DI
            s_logger.LogInfo("Validating DI first time : 1");
            Validate("ApplyLoadAndMatchDI").Should().BeTrue();
        }

        [TestMethod]
        public void ApplyDataInBitMapMode()
        {
            // Apply data when service is stopped
            s_logger.LogInfo("Apply Churn : 2");
            StartWrites(512).Should().BeTrue();
        }

        [TestMethod]
        public void StartServiceAndValidateDI()
        {
            // Drain Changes
            StartReplication(0);

            // Verify DI 
            s_logger.LogInfo("Validating DI after reboot : 2");
            Validate("VerifyDIAfterReboot").Should().BeTrue();
        }

        [TestMethod]
        public void DITestInRebootMode()
        {
            // Drain changes
            StartReplication(0);
            s_logger.LogInfo("Validating DI after reboot : 2");

            // Verify DI
            Validate("VerifyDIAfterReboot").Should().BeTrue();
            s_logger.LogInfo("Applying load : 3");

            // Start Writes
            StartWrites(512).Should().BeTrue();
            s_logger.LogInfo("Validating DI : 3");

            // Validate DI
            Validate("VerifyDIAfterApplyingChurn").Should().BeTrue();
            s_logger.LogInfo("Applying load : 4");
        }

        [TestMethod]
        public void VerifyDI()
        {
            // Drain changes
            StartReplication(0);

            // Verify DI
            s_logger.LogInfo("Validating DI : 4");
            Validate("VerifyDIAfterReboot").Should().BeTrue();
        }

        [ClassCleanup]
        public static void NoRebootClassCleanup()
        {
            string sourcePath = s_logDir;
            string guid = Guid.NewGuid().ToString();
            string targetPath = @"C:\logs\" + guid;
            if (s_logger != null)
            {
                s_logger.LogInfo("Copying logs to " + targetPath);
                s_logger.Close();
            }
            CopyLogs(sourcePath, targetPath);
        }
    }
}
