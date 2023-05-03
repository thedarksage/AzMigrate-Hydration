using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using FluentAssertions;

namespace ManagedPnpTests
{
    [TestClass]
    public class BasicGPTVolumePNPTests : BasicVolumePNPTests
    {
        public TestContext TestContext { get; set; }

        [ClassInitialize]
        public static void InitBasicVolumePnpTestSetup(TestContext context)
        {
            testName = context.TestName;
            string BaseLogDir = (string)context.Properties["LoggingDirectory"];
            string logdir = Path.Combine(BaseLogDir, "BasicMBRVolumePNPTests", testName);
            BasicVolumePnpClassInit(1, logdir);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void Create4PrimaryPartitionTestsOnBasicGPTDisk()
        {
            // Create 4 primary partition
            SetupTestFile("Create4PrimaryPartitionTestsOnBasicGPTDisk.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre par pri size=100");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void Create10PrimaryPartitionsOnBaiscGPTDisk()
        {
            SetupTestFile("Create10PrimaryPartitionsOnBaiscGPTDisk.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                for (int i = 0; i < 10; i++)
                {
                    sw.WriteLine("cre par pri size=90");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void Create128PrimaryPartitionsOnBaiscGPTDisk()
        {
            SetupTestFile("Create128PrimaryPartitionsOnBaiscGPTDisk.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                for (int i = 0; i < 127; i++)
                {
                    sw.WriteLine("cre par pri size=7");
                }
                sw.WriteLine("cre par pri");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestInitialize]
        public void BasicVolumeInitialize()
        {
            foreach (var disk in m_srcTargetMap.Keys)
            {
                disk.ResetDisk();
            }
        }
    }
}
