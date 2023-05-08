using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using System.Management;
using System.IO;
using System.Diagnostics;
using FluentAssertions;
using System.Text;
using System.Linq;
using System.Collections;
using Microsoft.Test.Storages;

namespace ManagedPnpTests
{
    [TestClass]
    public class DynamicVolumePnpTests : VolumePNPTests
    {
        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            testName = context.TestName;
            string BaseLogDir = (string)context.Properties["LoggingDirectory"];
            string logdir = Path.Combine(BaseLogDir, "DynamicVolumePnpTests", testName);
            PnPTestsInitialize(4, logdir);
        }

        public ArrayList GetFreeDriveLetters()
        {
            ArrayList driveLetters = new ArrayList(22);

            // Increment from ASCII values for A-Z
            for (int i = 69; i <= 90; i++)
            {
                driveLetters.Add(Convert.ToChar(i));
            }

            foreach (string drive in Directory.GetLogicalDrives())
            {
                // Remove unused drive letters from list
                driveLetters.Remove(drive[0]);
            }

            return driveLetters;
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateDynamicMBRDisk()
        {
            SetupTestFile("CreateDynamicMBRDisk.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ConvertBasicMBRWithVolumesToDynamicMBR()
        {
            SetupTestFile("ConvertBasicMBRWithVolumesToDynamicMBR.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre par pri size=100");
                }
                sw.WriteLine("con dynamic");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OfflineDynamicMBRDiskTests()
        {
            SetupTestFile("OfflineDynamicMBRDiskTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                }
                sw.WriteLine("offline disk");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OnlineDynamicMBRDiskTests()
        {
            SetupTestFile("OnlineDynamicMBRDiskTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("offline disk");
                sw.WriteLine("online disk");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [Ignore]
        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OfflineDynamicGPTDiskTests()
        {
            SetupTestFile("OfflineDynamicGPTDiskTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con gpt");
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("offline disk");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [Ignore]
        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void OnlineDynamicGPTDiskTests()
        {
            SetupTestFile("OnlineDynamicGPTDiskTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con gpt");
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("offline disk");
                sw.WriteLine("online disk");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void MakeDiskOnlineAndCreateVolume()
        {
            SetupTestFile("MakeDiskOnlineAndCreateVolume.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("offline disk");
                sw.WriteLine("online disk");
                sw.WriteLine("cre vol simple size=100");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ShrinkAndExtendVolumes()
        {
            SetupTestFile("ShrinkAndExtendVolumes.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                    sw.WriteLine(string.Format("assign letter={0}", driveLetters[i]));
                }
                sw.WriteLine("sel vol " + driveLetters[0]);
                sw.WriteLine("online vol");
                sw.WriteLine("shrink desired=100 minimum=50");
                sw.WriteLine("sel vol " + driveLetters[1]);
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
            }
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ShrinkSimpleVolumeAndCreateSpannedVolumeTests()
        {
            SetupTestFile("ShrinkSimpleVolumeAndCreateSpannedVolumeTests.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple");
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[0]));
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple");
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
                sw.WriteLine("sel vol " + driveLetters[1]);
                sw.WriteLine("online vol");
                sw.WriteLine("shrink desired=100");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("sel vol " + driveLetters[0]);
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
            }
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateFourSimpleVolumesInDynamicMBRDisk()
        {
            SetupTestFile("CreateFourSimpleVolumesInDynamicMBRDisk.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteSimpleVolumeTests()
        {
            SetupTestFile("DeleteSimpleVolumeTests.txt");
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("online vol");
                sw.WriteLine("del vol");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteSimpleVolumesInDynamicMBRDiskTests()
        {
            SetupTestFile("DeleteSimpleVolumesInDynamicMBRDiskTests.txt");

            ArrayList driveLetters = GetFreeDriveLetters();
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                    sw.WriteLine(string.Format("assign letter={0}", driveLetters[i]));
                }
                sw.WriteLine("sel vol " + driveLetters[1]);
                sw.WriteLine("del vol");
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendSimpleVolumeTests()
        {
            SetupTestFile("ExtendSimpleVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString());
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ShrinkSimpleVolumeTests()
        {
            SetupTestFile("ShrinkSimpleVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=200");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("shrink desired=100 minimum=50");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateSpannedVolumeTests()
        {
            SetupTestFile("CreateSpannedVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteSpannedVolumeTests()
        {
            SetupTestFile("DeleteSpannedVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("delete vol");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendSpannedVolumeTests()
        {
            SetupTestFile("ExtendSpannedVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("extend size=100 disk=" + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ShrinkSpannedVolumeTests()
        {
            SetupTestFile("ShrinkSpannedVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend size=200 disk=" + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("shrink desired=100 minimum=50");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateStripedVolume()
        {
            SetupTestFile("CreateStripedVolume.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume stripe size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteStripedVolume()
        {
            SetupTestFile("DeleteStripedVolume.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume stripe size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
                sw.WriteLine("online vol");
                sw.WriteLine("del vol");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ExtendSimpleVolumeWithoutSizeTests()
        {
            SetupTestFile("ExtendSimpleVolumeWithoutSizeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("sel vol");
                sw.WriteLine("online vol");
                sw.WriteLine("extend");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void AddMirrorToExistingSimpleVolume()
        {
            SetupTestFile("AddMirrorToExistingSimpleVolume.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("add disk " + disk2);
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteMirroredVolumeTests()
        {
            SetupTestFile("DeleteMirroredVolumeTests.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("add disk " + disk2);
                sw.WriteLine("online vol");
                sw.WriteLine("del vol");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void BreakMirroredVolumeTests()
        {
            SetupTestFile("BreakMirroredVolumeTests.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("add disk " + disk2);
                sw.WriteLine("break disk " + disk2);
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveAMirrorFromMirroredVolumesTests()
        {
            SetupTestFile("RemoveAMirrorFromMirroredVolumesTests.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("add disk " + disk2);
                sw.WriteLine("break disk " + disk2 + " nokeep");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ReconnectFailedMirroredDisk()
        {
            SetupTestFile("ReconnectFailedMirroredDisk.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine("add disk " + disk2);
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("offline disk");
                sw.WriteLine("online disk");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ReplaceFailedMirrorWithNewMirror()
        {
            SetupTestFile("ReplaceFailedMirrorWithNewMirror.txt");
            ArrayList driveLetters = GetFreeDriveLetters();
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
                sw.WriteLine("add disk " + disk2);
                sw.WriteLine("break disk " + disk2 + " nokeep");
                sw.WriteLine("sel vol " + driveLetters[1]);
                sw.WriteLine("add disk " + disk3);
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateRaid5Volume()
        {
            SetupTestFile("CreateRaid5Volume.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume raid size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void DeleteRaid5Volume()
        {
            SetupTestFile("DeleteRaid5Volume.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume raid size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
                sw.WriteLine("online vol");
                sw.WriteLine("del vol");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void AssignDriveLetterToRaid5VolumeTests()
        {
            SetupTestFile("AssignDriveLetterToRaid5VolumeTests.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();
            ArrayList driveLetters = GetFreeDriveLetters();
            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume raid size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }


        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void ReactivateRaid5Disk()
        {
            SetupTestFile("ReactivateRaid5Disk.txt");
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume raid size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("offline disk");
                sw.WriteLine("online disk");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        //[Ignore]
        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RepairRaid5Disk()
        {
            SetupTestFile("RepairRaid5Disk.txt");
            ArrayList driveLetters = GetFreeDriveLetters();
            string disk1 = m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString();
            string disk2 = m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString();
            string disk3 = m_srcTargetMap.Keys.ElementAt(2).DiskNumber.ToString();
            string disk4 = m_srcTargetMap.Keys.ElementAt(3).DiskNumber.ToString();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + disk4);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk2);
                sw.WriteLine("con dynamic");
                sw.WriteLine("sel disk " + disk3);
                sw.WriteLine("con dynamic");
                sw.WriteLine("create volume raid size=100 disk=" + disk1 + "," + disk2 + "," + disk3);
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("offline disk");
                sw.WriteLine("select disk " + disk2);
                sw.WriteLine("sel vol " + driveLetters[1]);
                sw.WriteLine("repair disk=" + disk4);
                sw.WriteLine("sel disk " + disk1);
                sw.WriteLine("online disk");
            }
            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void CreateSimpleVolWithoutSizeAndAssignDriveLetter()
        {
            SetupTestFile("CreateSimpleVolWithoutSizeAndAssignDriveLetter.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple");
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveDriveLetterFromSimpleVolumeTests()
        {
            SetupTestFile("RemoveDriveLetterFromSimpleVolumeTests.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                sw.WriteLine("cre vol simple size=100");
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[1]));
                sw.WriteLine(string.Format("remove letter={0}", driveLetters[1]));
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }


        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void AssignDriveLettersToMultipleSimpleVolumes()
        {
            SetupTestFile("AssignDriveLettersToMultipleSimpleVolumes.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i < 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                    sw.WriteLine(string.Format("assign letter={0}", driveLetters[i]));
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void AssignMountPointsToMultipleSimpleVolumes()
        {
            SetupTestFile("AssignMountPointsToMultipleSimpleVolumes.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 1; i <= 3; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
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
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveMultipleDriveLetters()
        {
            SetupTestFile("RemoveMultipleDriveLetters.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 0; i <= 4; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                    sw.WriteLine(string.Format("assign letter={0}", driveLetters[i]));
                }

                for (int i = 1; i <= 4; i++)
                {
                    string driveLetter = string.Format("{0}", driveLetters[i]);
                    sw.WriteLine(string.Format("sel vol {0}", driveLetter));
                    sw.WriteLine("remove");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestMethod]
        [TestCategory(TestCategories.DynamicVolumePnpTests), TestCategory(TestCategories.PnpTests)]
        public void RemoveMultipleMountPoints()
        {
            SetupTestFile("RemoveMultipleMountPoints.txt");

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(1).DiskNumber.ToString());
                sw.WriteLine("con dynamic");
                for (int i = 1; i <= 3; i++)
                {
                    sw.WriteLine("cre vol simple size=100");
                    string mountPath = string.Format("{0}\\fortest{1}", testRoot, i);
                    if (Directory.Exists(mountPath))
                    {
                        Directory.Delete(mountPath);
                    }
                    Directory.CreateDirectory(mountPath);

                    sw.WriteLine(string.Format("assign mount={0}", mountPath));
                }

                for (int i = 1; i <= 3; i++)
                {
                    string mountPath = string.Format("{0}\\fortest{1}", testRoot, i);
                    sw.WriteLine(string.Format("sel vol {0}", mountPath));
                    sw.WriteLine("remove");
                }
            }

            // Execute Dispart
            ExecuteAndValidateScript(testFile).Should().Be(0);
        }

        [TestInitialize]
        public void DynamicVolumeInitialize()
        {
            foreach (var disk in m_srcTargetMap.Keys) disk.Offline(readOnly: false);
            foreach (var disk in m_srcTargetMap.Keys) disk.ResetDiskInfo();
            foreach (var disk in m_srcTargetMap.Keys) disk.Online(readOnly: false);
        }

        [TestCleanup]
        public void DynamicVolumeCleanup()
        {
            foreach (var disk in m_srcTargetMap.Keys) disk.Offline(readOnly: false);
            foreach (var disk in m_srcTargetMap.Keys) disk.ResetDiskInfo();
            foreach (var disk in m_srcTargetMap.Keys) disk.Online(readOnly: false);

        }

    }
}
