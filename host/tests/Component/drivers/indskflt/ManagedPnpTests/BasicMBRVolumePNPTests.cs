using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using System.Management;
using System.IO;
using System.Diagnostics;
using FluentAssertions;
using System.Text;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.Test.Storages;

using BlockValidator = ManagedBlockValidator.BlockValidator;
using System.Threading;

namespace ManagedPnpTests
{
    [TestClass]
    public class BasicMBRVolumePNPTests : BasicVolumePNPTests
    {
        [ClassInitialize]
        public static void InitBasicMBRVolumePnpTestSetup(TestContext context)
        {
            testName = context.TestName;
            string BaseLogDir = (string)context.Properties["LoggingDirectory"];
            string logdir = Path.Combine(BaseLogDir, "BasicMBRVolumePNPTests", testName);
            BasicVolumePnpClassInit(1, logdir);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateFourPrimaryPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("CreateFourPrimaryPartitionTests.txt");

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
        public void DeletePrimaryPartitionTests()
        {
            SetupTestFile("DeletePrimaryPartitionTests.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre par pri size=100");
                }
                sw.WriteLine("sel par 1");
                sw.WriteLine("del par");
                sw.WriteLine("sel par 2");
                sw.WriteLine("del par");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateExtendedPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("CreateExtendedPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par extended size=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteExtendedPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("DeleteExtendedPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par extended size=100");
                sw.WriteLine("del par");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateLogicalPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("CreateLogicalPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par extended size=100");
                sw.WriteLine("cre par log");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateLinuxPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("CreateLinuxPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100 id=81");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteLinuxPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("DeleteLinuxPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100 id=81");
                sw.WriteLine("del par");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateLinuxPartitionWithOffset()
        {
            // Create 4 primary partition
            SetupTestFile("CreateLinuxPartitionWithOffset.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100 id=81 offset=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OEMPartitonCreationTests()
        {
            // Create 4 primary partition
            SetupTestFile("OEMPartitonCreationTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri id=12 size=100 ");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OEMPartitonDeletionFailureTests()
        {
            // Create 4 primary partition
            SetupTestFile("OEMPartitonDeletionFailureTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri id=12 size=100");
                sw.WriteLine("del par");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OEMPartitonDeletionSuccessTests()
        {
            // Create 4 primary partition
            SetupTestFile("OEMPartitonDeletionSuccessTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri id=12 size=100");
                sw.WriteLine("del par OVERRIDE");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OEMPartitonCreationWithOffsetTests()
        {
            // Create 4 primary partition
            SetupTestFile("OEMPartitonCreationWithOffsetTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri id=12 size=100 offset=500");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OEMPartitonDeletionWithOffsetTests()
        {
            // Create 4 primary partition
            SetupTestFile("OEMPartitonDeletionWithOffsetTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri id=12 size=100 offset=500");
                sw.WriteLine("del par override");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendPrimaryPartitionTests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendPrimaryPartitionTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendPrimaryPartitionWithSizeTests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendPrimaryPartitionWithSizeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendPrimaryLowerBoundery1Tests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendPrimaryLowerBoundery1Tests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=1");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendPrimaryLowerBoundery8Tests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendPrimaryLowerBoundery8Tests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=8");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendLogicalExtendWithValidSizeTests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendLogicalExtendWithValidSizeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=1000");
                sw.WriteLine("cre par log size=100");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendLogicalExtendMultipleTimesWithValidSizeTests()
        {
            // Create 4 primary partition
            SetupTestFile("ExtendLogicalExtendMultipleTimesWithValidSizeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=1000");
                sw.WriteLine("cre par log size=100");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100");
                sw.WriteLine("extend size=100");
                sw.WriteLine("extend size=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendWithoutSizeArgumentTests()
        {
            SetupTestFile("ExtendWithoutSizeArgumentTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=1000");
                sw.WriteLine("cre par log size=100");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendLogicalWithLowerBoundary()
        {
            SetupTestFile("ExtendLogicalWithLowerBoundary.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=1000");
                sw.WriteLine("cre par log size=500");
                sw.WriteLine("select vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=7");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateLogicalWithOffsetInExtendedPartitonFreespace()
        {
            SetupTestFile("CreateLogicalWithOffsetInExtendedPartitonFreespace.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=400 ");
                sw.WriteLine("cre par log size=100 offset=500");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        // TODO: Maximum number of partitions on disks
        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void AssignMultipleMountPoints()
        {
            SetupTestFile("AssignMultipleMountPoints.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100");
                sw.WriteLine("assign");
                for (int i = 1; i <= 3; i++)
                {
                    string mountPath = string.Format("{0}\\fortest{1}", testRoot, i);
                    if (Directory.Exists(mountPath))
                    {
                        Directory.Delete(mountPath);
                    }
                    Directory.CreateDirectory(mountPath);

                    sw.WriteLine(string.Format("assign mount={0}", mountPath));
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveMultipleMountPoints()
        {
            SetupTestFile("RemoveMultipleMountPoints.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100");
                sw.WriteLine("assign");
                for (int i = 1; i <= 3; i++)
                {
                    string mountPath = string.Format("{0}\\fortest{1}", testRoot, i);
                    if (Directory.Exists(mountPath))
                    {
                        Directory.Delete(mountPath);
                    }
                    Directory.CreateDirectory(mountPath);

                    sw.WriteLine(string.Format("assign mount={0}", mountPath));
                }

                for (int i = 1; i < 3; i++)
                {
                    string mountPath = string.Format("{0}\\fortest{1}", testRoot, i);
                    sw.WriteLine(string.Format("sel vol {0}", mountPath));
                    sw.WriteLine("remove");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveDismountWithoutAnyDriveLetter()
        {
            SetupTestFile("RemoveDismountWithoutAnyDriveLetter.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("remove dismount");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveDismountWithOneDriveLetter()
        {
            SetupTestFile("RemoveDismountWithOneDriveLetter.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("assign");
                sw.WriteLine("remove dismount");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveDismountWithOneMountPoint()
        {
            SetupTestFile("RemoveDismountWithoutOneDriveLetter.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine(string.Format("assign mount={0}\\fortest16", testRoot));
                sw.WriteLine("remove dismount");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void SetPrimaryPartitionAsActive()
        {
            SetupTestFile("SetPrimaryPartitionAsActive.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("active");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void SetOnlyOnePartitionAsActiveAtaTime()
        {
            SetupTestFile("SetOnlyOnePartitionAsActiveAtaTime.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("active");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void SetInactiveOnAnActivePrimaryPartition()
        {
            SetupTestFile("SetInactiveOnAnActivePrimaryPartition.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("active");
                sw.WriteLine("inactive");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void InactiveOnAnActiveLogicalPartition()
        {
            SetupTestFile("InactiveOnAnActiveLogicalPartition.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=500");
                sw.WriteLine("cre par log");
                sw.WriteLine("active");
                sw.WriteLine("inactive");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void InactiveOnAnExtendedPartitionTest()
        {
            SetupTestFile("InactiveOnAnExtendedPartitionTest.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par ext size=500");
                sw.WriteLine("inactive");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.BasicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void InactiveOnAnNonActivePrimaryPartitionTest()
        {
            SetupTestFile("InactiveOnAnNonActivePrimaryPartitionTest.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=500");
                sw.WriteLine("inactive");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestInitialize]
        public void BasicVolumeInitialize()
        {
            foreach (var disk in m_srcTargetMap.Keys) disk.Offline(readOnly: false);
            foreach (var disk in m_srcTargetMap.Keys) disk.ResetDiskInfo();
            foreach (var disk in m_srcTargetMap.Keys) disk.Online(readOnly: false);
        }

        public TestContext TestContext { get; set; }

        [TestCleanup]
        public void BasicVolumeCleanup()
        {
            foreach (var disk in m_srcTargetMap.Keys) disk.Offline(readOnly: false);
            foreach (var disk in m_srcTargetMap.Keys) disk.ResetDiskInfo();
            foreach (var disk in m_srcTargetMap.Keys) disk.Online(readOnly: false);
        }
    }
}
