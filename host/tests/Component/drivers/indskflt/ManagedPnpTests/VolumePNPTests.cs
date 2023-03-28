using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using FluentAssertions;
using Microsoft.Test.Storages;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ReplicationEngine;
using BlockValidator = ManagedBlockValidator.BlockValidator;
using InMageLogger;

namespace ManagedPnpTests
{
    [TestClass]
    public abstract class VolumePNPTests : ReplicationSetup
    {
        static protected string testRoot = "C:\\tmp";
        static protected string cleanDisksFile;
        protected static string testFile;
        static protected bool s_bInitialized = false;
        static protected object initLock = new object();
        static protected UInt64 startOffset = 0;
        static protected UInt64 endOffset = 1;
        static protected string testName;

        protected static void PnPTestsInitialize(int numDisks, string logdir)
        {
            lock (initLock)
            {
                if (!s_bInitialized)
                {
                    s_logDir = logdir;
                    CreateDir(s_logDir);
                    testRoot = s_logDir;
                    string LogFilePath = s_logDir + @"\VolumePnpTests.log";
                    s_logger = new Logger(LogFilePath, true);
                    InitPnpTestSetup();
                    InitSrcTgtDiskMap(numDisks);
                    InitializeReplication(testName, startOffset, endOffset);
                    StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
                    s_bInitialized = true;
                }
            }
        }

        protected static void InitPnpTestSetup()
        {
            StringBuilder sb = new StringBuilder(testRoot);
            sb.Append("\\VolumePNPTests\\" + Guid.NewGuid().ToString());
            testRoot = sb.ToString();

            // Create Test Directory
            Directory.CreateDirectory(testRoot);
        }

        public int ExecuteAndValidateScript(string testFile)
        {
            int status = 0;
			Process diskpart = Process.Start("diskpart", "/s " + testFile);
            s_logger.LogInfo("Waiting for diskpart exit");
            diskpart.WaitForExit();
            s_logger.LogInfo("Diskpart is done");

            OfflineDisks(true);
            s_logger.LogInfo("Offlined disk in read only mode");
            status = ValidateDisks(testName);
            OnlineDisks(false);
            s_logger.LogInfo("Onlined disk in read write mode");

            return status;
        }

        public static void SetupTestFile(string testName)
        {
            testFile = testRoot + @"\" + testName;
            if (File.Exists(testFile))
            {
                File.Delete(testFile);
            }
        }

        public void CleanDisks()
        {
            Process cleanDisks = Process.Start("diskpart", "/s " + cleanDisksFile);
            cleanDisks.WaitForExit();
            cleanDisks.ExitCode.Should().Be(0);
        }
    }
}
