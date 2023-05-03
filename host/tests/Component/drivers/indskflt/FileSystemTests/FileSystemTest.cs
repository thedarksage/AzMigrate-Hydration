using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Text;
using System.IO;
using ReplicationEngine;
using System.Collections.Generic;
using System.Management;
using System.Diagnostics;
using FluentAssertions;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.Test.Storages;
using BlockValidator = ManagedBlockValidator.BlockValidator;
using System.Collections;

namespace FileSystemTests
{
    [TestClass]
    public class FileSystemTest : ReplicationSetup
    {
        static protected string testRoot = "C:\\tmp";
        static string testFile;
        static string testName;

        private TestContext testContextInstance;
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
        public static void InitFileSystemTestSetup(TestContext context)
        {
            testName = context.TestName;
            CreateTestDir();
            InitSrcTgtDiskMap(1);
            InitializeReplication(testName, 0, 1);
            StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
        }

        static protected void CreateTestDir()
        {
            StringBuilder sb = new StringBuilder(testRoot);
            sb.Append("\\FileSystemTests\\" + Guid.NewGuid().ToString());
            testRoot = sb.ToString();

            // Create Test Directory
            Directory.CreateDirectory(testRoot);
        }

        public void SetupTestFile(string testName)
        {
            testFile = testRoot + @"\" + testName;
            if (File.Exists(testFile))
            {
                File.Delete(testFile);
            }
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

        public int ExecuteScript(string testFile)
        {
            Process diskpart = Process.Start("diskpart", "/s " + testFile);
            diskpart.WaitForExit();

            return diskpart.ExitCode;
        }

        public static string CreateFile(char volName)
        {
            string root = volName.ToString();
            StringBuilder sb = new StringBuilder(root);
            sb.Append("\\FileIOTests");
            root = sb.ToString();

            Directory.CreateDirectory(root);
            string guid = Guid.NewGuid().ToString();

            string fileName = sb.Append("\\" + guid + "- File" + volName + ".txt").ToString();

            return fileName;
        }

        public static void WriteBufferedIO(char volName, int size, int len)
        {
            try
            {
                int bufSize = (int)size / 2;
                byte[] data = generateData(bufSize, len);
                //string fileName = CreateFile(volName);
                string fileName = volName + ":\\Test.txt";
                //string fileName = "C:\\Test.txt";
                FileStream fs = new FileStream(fileName, FileMode.OpenOrCreate, FileAccess.ReadWrite);
                Console.WriteLine("Size " + data.Length);
                fs.Write(data, 0, bufSize);

                BufferedStream bs = new BufferedStream(fs, len);
                byte[] data1 = generateData(bufSize, len);
                bs.Write(data1, 0, bufSize);
                //fileHandle.Close();
                bs.Close();
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public static byte[] generateData(int size, int len)
        {
            byte[] data = new byte[size];
            int counter = size / len;

            int j = 0;
            for (int i = 0; i < counter; i++)
            {

                byte[] cdata = generateRandomData(len);
                cdata.CopyTo(data, j);
                j += cdata.Length;
            }

            return data;
        }

        public static byte[] generateRandomData(int len)
        {
            byte[] data = new byte[len];
            Random rand = new Random();
            rand.NextBytes(data);

            return data;
        }

        public static void WriteIO(int size, int len)
        {
            try
            {
                byte[] data = generateData(size, len);
                string file = @"F:\text.txt";
                using (FileStream fileHandle = new FileStream(file, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None, size, FileOptions.None))
                    fileHandle.Write(data, 0, data.Length);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public static void WriteToFile(string volName, int bufferSize)
        {
            try
            {
                string dir = volName + ":\\FileIOTests";
                Directory.CreateDirectory(dir);
                Directory.SetCurrentDirectory(dir);

                DriveInfo drive = new DriveInfo(volName);
                int maxSize = (int) drive.AvailableFreeSpace;
                
                int currentOffset = 0;
                while (currentOffset < maxSize)
                {
                    int remainingBytes = maxSize - currentOffset;
                    int bytesToWrite = (remainingBytes < bufferSize) ? remainingBytes : bufferSize;
                    byte[] data = generateRandomData(bytesToWrite);

                    string file = Guid.NewGuid().ToString() + ".txt";
                    using (FileStream fh = new FileStream(file, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None, bytesToWrite, FileOptions.None))
                    {
                        fh.Write(data, 0, data.Length);
                    }

                    currentOffset += bytesToWrite;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        public void CreateVolumeAndValidate(int ioSize, string formatType)
        {
            // Create 4 primary partition
            SetupTestFile(testName + ".txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri");
                sw.WriteLine(string.Format("format fs={0}", formatType));
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[0]));
            }

            ExecuteScript(testFile).Should().Be(0);

            
            int volSize = 1024 * 1024;
            foreach (var disk in m_srcTargetMap.Keys)
            {
                volSize = (int)disk.Size;
                WriteToFile(driveLetters[0].ToString(), ioSize);
            }

            //WriteIO(972 * 1024 * 1024, ioSize);

            OfflineDisks(true);
            ValidateDisks(testName);
            OnlineDisks(false);
        }

        public static void WriteBufferedIOWrites(string volName, int bytesToWrite)
        {
            string dir = volName + ":\\FileIOTests";
            Directory.CreateDirectory(dir);
            Directory.SetCurrentDirectory(dir);

            byte[] data = generateRandomData(bytesToWrite);

            FileStream fs = new FileStream("test.txt", FileMode.Create, FileAccess.ReadWrite);
            BufferedStream bs = new BufferedStream(fs);
            bs.Write(data, 0, data.Length);

            bs.Close();

        }

        public static void WriteBIO()
        {
            Directory.SetCurrentDirectory("F:\\test");

            FileStream fs = new FileStream("test.txt", FileMode.Create, FileAccess.ReadWrite);
            BufferedStream bs = new BufferedStream(fs);
            //Console.WriteLine("Length: {0}\tPosition: {1}", bs.Length, bs.Position);
            byte[] data = generateRandomData(10 * 1024 * 1024);
            bs.Write(data, 0, data.Length);
            //fs.Write(data, 0, data.Length);
            //fs.Flush(false);

            /*
            for (int i = 0; i < 64; i++)
            {
                bs.WriteByte((byte)i);
            }
            Console.WriteLine("Length: {0}\tPosition: {1}", bs.Length, bs.Position);
            
            byte[] ba = new byte[bs.Length];
            bs.Position = 0;
            bs.Read(ba, 0, (int)bs.Length);
            foreach (byte b in ba)
            {
                Console.Write("{0,-3}", b);
            }

            string s = "Foo";
            for (int i = 0; i < 3; i++)
            {
                bs.WriteByte((byte)s[i]);
            }
            Console.WriteLine("\nLength: {0}\tPosition: {1}\t", bs.Length, bs.Position);

            for (int i = 0; i < (256 - 67) + 1; i++)
            {
                bs.WriteByte((byte)i);
            }
            Console.WriteLine("\nLength: {0}\tPosition: {1}\t", bs.Length, bs.Position);
            */
            //bs.Close();
        }
        [Ignore]
        [TestMethod]
        public void WriteIOToFile()
        {
            //WriteToFile("F", 256 * 1024 * 1024);
            WriteBIO();
        }

        [TestMethod]
        public void Write256MWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(256 * 1024 * 1024, "NTFS");
        }

        [TestMethod]
        public void Write256KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(512, "FAT32");
        }

        [TestMethod]
        public void Write128KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(256, "FAT32");
        }

        [TestMethod]
        public void Write64KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(128, "FAT32");
        }

        [TestMethod]
        public void Write32KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(64, "FAT32");
        }

        [TestMethod]
        public void Write16KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(32, "FAT32");
        }

        [TestMethod]
        public void Write8KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(16, "FAT32");
        }

        [TestMethod]
        public void Write4KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(8, "FAT32");
        }

        [TestMethod]
        public void Write1KWritesToFAT32Volume()
        {
            CreateVolumeAndValidate(2, "FAT32");
        }

        [TestMethod]
        public void Write256KWritesToFATVolume()
        {
            CreateVolumeAndValidate(512, "FAT");
        }

        [TestMethod]
        public void Write128KWritesToFATVolume()
        {
            CreateVolumeAndValidate(256, "FAT");
        }

        [TestMethod]
        public void Write64KWritesToFATVolume()
        {
            CreateVolumeAndValidate(128, "FAT");
        }

        [TestMethod]
        public void Write32KWritesToFATVolume()
        {
            CreateVolumeAndValidate(64, "FAT");
        }

        [TestMethod]
        public void Write16KWritesToFATVolume()
        {
            CreateVolumeAndValidate(32, "FAT");
        }

        [TestMethod]
        public void Write8KWritesToFATVolume()
        {
            CreateVolumeAndValidate(16, "FAT");
        }

        [TestMethod]
        public void Write4KWritesToFATVolume()
        {
            CreateVolumeAndValidate(8, "FAT");
        }

        [TestMethod]
        public void Write1KWritesToFATVolume()
        {
            CreateVolumeAndValidate(2, "FAT");
        }

        [TestMethod]
        public void Write256KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(512, "ntfs");
        }

        [TestMethod]
        public void Write128KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(256, "ntfs");
        }

        [TestMethod]
        public void Write64KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(128, "ntfs");
        }

        [TestMethod]
        public void Write32KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(64, "ntfs");
        }

        [TestMethod]
        public void Write16KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(32, "ntfs");
        }

        [TestMethod]
        public void Write8KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(16, "ntfs");
        }

        [TestMethod]
        public void Write4KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(8, "ntfs");
        }

        [TestMethod]
        public void Write1KWritesToNTFSVolume()
        {
            CreateVolumeAndValidate(2, "ntfs");
        }

        [Ignore]
        [TestMethod]
        public void CreateNTFSVolume()
        {
            // Create 4 primary partition
            SetupTestFile("CreateNTFSVolume.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=1000");
                sw.WriteLine(string.Format("format fs={0}", "ntfs"));
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[0]));
            }

            ExecuteScript(testFile).Should().Be(0);

            //WriteBufferedIO((char)driveLetters[0], 100 * 1024, 512);
            WriteIO(1000, 512);

            OfflineDisks(true);
            ValidateDisks(TestContext.TestName);
            OnlineDisks(false);
        }

        [Ignore]
        [TestMethod]
        public void CreateFATVolume()
        {
            // Create 4 primary partition
            SetupTestFile("CreateNTFSVolume.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100");
                sw.WriteLine(string.Format("format fs={0}", "FAT"));
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[0]));
            }

            ExecuteScript(testFile).Should().Be(0);

            WriteBufferedIO((char)driveLetters[0], 100, 512);

            OfflineDisks(true);
            ValidateDisks(testName);

            OnlineDisks(false);
        }

        [Ignore]
        [TestMethod]
        public void CreateFAT32Volume()
        {
            // Create 4 primary partition
            SetupTestFile("CreateNTFSVolume.txt");
            ArrayList driveLetters = GetFreeDriveLetters();

            using (StreamWriter sw = File.CreateText(testFile))
            {
                sw.WriteLine("sel disk " + m_srcTargetMap.Keys.ElementAt(0).DiskNumber.ToString());
                sw.WriteLine("cre par pri size=100");
                sw.WriteLine(string.Format("format fs={0}", "FAT32"));
                sw.WriteLine(string.Format("assign letter={0}", driveLetters[0]));
            }

            ExecuteScript(testFile).Should().Be(0);

            WriteBufferedIO((char)driveLetters[0], 512, 512);

            OfflineDisks(true);
            ValidateDisks(testName);
            OnlineDisks(false);
        }

        [TestInitialize]
        public void FileSystemTestsInitialize()
        {
            foreach (var disk in m_srcTargetMap.Keys)
            {
                byte[] buffer = new byte[512];
                disk.Offline(readOnly: false);
                disk.Read(buffer, 512, 0).Should().Be(512);
                Array.Clear(buffer, 446, 64);
                disk.Write(buffer, 512, 0).Should().Be(512);
                disk.Online(readOnly: false);
            }
        }
    }
}
