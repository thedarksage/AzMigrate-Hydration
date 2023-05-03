using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ReplicationEngine;
using Microsoft.Test.Storages;
using ManagedBlockValidator;
using ManagedPnpTests;
using System.Diagnostics;
using System.IO;
using InMageLogger;
using FluentAssertions;
using BlockValidator = ManagedBlockValidator.BlockValidator;
using System.Threading;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Test
{
    [TestClass]
    public class UnitTest1 : ReplicationSetup
    {
        protected static string testFile;
        protected static byte[] s_Buffer = null;

        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            InitSrcTgtDiskMap(2);
            InitializeReplication("DITEST", 0, 1);
            StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
        }

        public void WriteDisk(int diskIndex, UInt64 startOffset, UInt64 maxSize, UInt32 bufferSize)
        {
            if (0 == bufferSize)
            {
                throw new ArgumentException("bufferSize", "WriteDisk: BufferSize is 0");
            }

            byte[] Buffer = new byte[bufferSize];
            Parallel.For(0, Buffer.Length, i => Buffer[i] = (byte)0xee);            
            
            Random random = new Random();

            Task updateBuffer = Task.Factory.StartNew(() =>
                {
                    for (int j = 0; j < 10000; j++)
                    {
                        Parallel.For(0, Buffer.Length, i => Buffer[i] = (byte)j);
                    }
                }
                );
           // PhysicalDrive disk = new PhysicalDrive(diskIndex);
            Win32ApiFile disk = new Win32ApiFile(@"F:\test.dat",null );
            UInt64 currentOffset = startOffset;

            while (currentOffset < maxSize)
            {
                UInt32 bytesToWrite = ((maxSize - currentOffset) < (UInt64)bufferSize) ? (UInt32)(maxSize - currentOffset) : bufferSize;

                //Console.WriteLine("currentOffset - {0}, bytesToWrite - {1}, buffer - {2}, BufferSize - {3}", currentOffset, bytesToWrite, Buffer, bufferSize);
                try
                {
                    disk.Write(Buffer, bytesToWrite, currentOffset);
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Error - " + ex.ToString());
                }
                currentOffset += bytesToWrite;
            }
        }

        void CreateStripedVolume(IList<int> disksIds)
        {
            SetupTestFile("CreateStriped.txt");
            StreamWriter sw = File.CreateText(testFile);

            List<string> diskIdList = new List<string>();
            foreach(var disk in disksIds)
            {
                diskIdList.Add(disk.ToString());
            }
            sw.WriteLine("create volume stripe disk={0}", string.Join(",", diskIdList));

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);            
        }

        [TestMethod]
        public void DITest11()
        {
           // ValidateDisks("DITest");

            // Make sure disk is in data mode
            S2AgentWaitForConsistentState(s_s2Agent).Should().BeTrue();

            Thread.Sleep(2  * 1000);

            ValidateDisks("DI Test");

            //ApplyDataAndValidate();
        }

        [TestMethod]
        public void DITest()
        {
            ValidateDisks("DITest");
            IList<int> disks = new List<int>() { 1, 2 };

            CreateStripedVolume(disks);

            // Make sure disk is in data mode
            S2AgentWaitForConsistentState(s_s2Agent).Should().BeTrue();

            ApplyDataAndValidate();

            // Make sure disk is in data mode
            S2AgentWaitForConsistentState(s_s2Agent).Should().BeTrue();
            BreakStripedVolume();

            // Make sure disk is in data mode
            S2AgentWaitForConsistentState(s_s2Agent).Should().BeTrue();
            CreateSimpleVolume();

            // Make sure disk is in data mode
            S2AgentWaitForConsistentState(s_s2Agent).Should().BeTrue();
            ApplyDataAndValidate();
        }

        public void ApplyDataAndValidate()
        {
            Thread.Sleep(30*000);

            UInt32 bufferSize = 18 * 1024 * 512;
            WriteDisk(1, 512, 1024 * 1024 * 1024, bufferSize);
            ValidateDisks("DITest");
        }

        public void BreakStripedVolume()
        {
            SetupTestFile("BreakStripedVolume.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel volume F:");
                sw.WriteLine("del vol");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        public void CreateSimpleVolume()
        {
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk 1");
                sw.WriteLine("cre vol simple");
                sw.WriteLine("Format quick");
                sw.WriteLine("assign letter=F");
            }
            Process diskpart = Process.Start("diskpart", "/s " + testFile);
            diskpart.WaitForExit();
        }

        public int ExecuteAndValidateScript(string testFile)
        {
            Process diskpart = Process.Start("diskpart", "/s " + testFile);
            diskpart.WaitForExit();

            ValidateDisks("BreakVol");
            return diskpart.ExitCode;
        }

        public static void SetupTestFile(string testName)
        {
            testFile =  @"C:\" + testName;
            if (File.Exists(testFile))
            {
                File.Delete(testFile);
            }
        }

        public void validate()
        {
            foreach (var disk in m_srcTargetMap.Keys)
            {
                string output = BlockValidator.Validate(disk, m_srcTargetMap[disk], disk.Size, s_logger) ? "PASSED" : "FAILED";
                Console.WriteLine("STatus : "+output);
            }
        }


        [TestMethod]
        public void ValidateData()
        {
            ValidateDisks("DITest");
        }
    }
}
