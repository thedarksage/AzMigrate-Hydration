using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace ProcessServerUnitTests
{
    [TestClass]
    public class FSUtilsTests
    {
        private TestContext testContextInstance;

        /// <summary>
        ///Gets or sets the test context which provides
        ///information about and functionality for the current test run.
        ///</summary>
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

        #region Additional test attributes

        public static string s_originalFile = "gziptestoriginal.txt";
        public static string s_compressedFile = "gziptestcompressed.txt.gz";
        public static string s_extractedFile = "gziptestextracted.txt";
        private static readonly string s_testFolderPath = Path.Combine(Path.GetTempPath(), "FSUtilsTests");

        //
        // You can use the following additional attributes as you write your tests:
        //
        // Use ClassInitialize to run code before running the first test in the class
        // [ClassInitialize()]
        // public static void MyClassInitialize(TestContext testContext) { }
        //
        // Use ClassCleanup to run code after all tests in a class have run
        // [ClassCleanup()]
        // public static void MyClassCleanup() { }
        //

        void DeleteFile(string filename)
        {
            if (File.Exists(filename))
                File.Delete(filename);
        }

        //// Use TestInitialize to run code before running each test 
        [TestInitialize()]
        public void MyTestInitialize()
        {
            // Delete the folder from the previous run
            MyTestCleanup();
        }

        // Use TestCleanup to run code after each test has run
        [TestCleanup()]
        public void MyTestCleanup()
        {
            DeleteFile(s_originalFile);
            DeleteFile(s_compressedFile);
            DeleteFile(s_extractedFile);

            if (Directory.Exists(s_testFolderPath))
                Directory.Delete(@"\\?\" + s_testFolderPath, recursive: true);
        }

        #endregion

        [TestMethod]
        #region Null or Whitespace inputs
        [DataRow("", true, "", typeof(ArgumentNullException), null)]
        [DataRow("", true, "  ", typeof(ArgumentNullException), null)]
        [DataRow("  ", false, "", typeof(ArgumentNullException), null)]
        [DataRow("  ", false, null, typeof(ArgumentNullException), null)]
        [DataRow(null, true, "\t\r", typeof(ArgumentNullException), null)]
        #endregion Null or Whitespace inputs
        #region Relative path inputs
        [DataRow(@"C:\GoodPath", true, @"RelPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        [DataRow(@"C:\GoodPath", true, @"RelPath\Multi", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        [DataRow(@"C:\GoodPath", false, @"\RelPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        [DataRow(@"C:\GoodPath", false, @"C:RelPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        [DataRow(@"C:\GoodPath", false, @"C:", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        [DataRow(@"RelPath", true, @"C:\GoodPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: itemPath")]
        [DataRow(@"RelPath\Multi", true, @"C:\GoodPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: itemPath")]
        [DataRow(@"\RelPath", false, @"C:\GoodPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: itemPath")]
        [DataRow(@"C:RelPath", false, @"C:\GoodPath", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: itemPath")]
        #endregion Relative path inputs
        #region File indicator with dir path
        [DataRow(@"C:\parent\child\", true, @"C:\parent", typeof(ArgumentException), "Invalid file name\r\nParameter name: itemPath")]
        [DataRow(@"C:\parent\child/", true, @"C:\parent", typeof(ArgumentException), "Invalid file name\r\nParameter name: itemPath")]
        [DataRow(@"C:\parent\child/\", true, @"C:\parent", typeof(ArgumentException), "Invalid file name\r\nParameter name: itemPath")]
        #endregion File indicator with dir path
        #region Special invalid inputs
        [DataRow(@"\\computer", false, @"\\", typeof(ArgumentException), "Invalid long path\r\nParameter name: expectedParentFolder")]
        [DataRow(@"\\?\UNC\computer", false, @"\\", typeof(ArgumentException), "Invalid long path\r\nParameter name: expectedParentFolder")]
        [DataRow(@"\\?\C:\item", false, @"\\?\", typeof(ArgumentException), "Invalid long path\r\nParameter name: expectedParentFolder")]
        [DataRow(@"C:\item", false, @"\\?\", typeof(ArgumentException), "Invalid long path\r\nParameter name: expectedParentFolder")]
        [DataRow(@"\\computer", false, @"\\?\UNC\", typeof(ArgumentException), "Invalid long path\r\nParameter name: expectedParentFolder")]
        #endregion Special invalid inputs
        #region Positive test cases
        [DataRow(@"\\computer\share\file.txt", true, @"\\computer\share", null, null)]
        [DataRow(@"\\computer\share\file.txt", true, @"\\computer\share\", null, null)]
        [DataRow(@"\\computer\share\parent\file.txt", true, @"\\computer\share\parent", null, null)]
        [DataRow(@"\\computer\share\parent\child\", false, @"\\computer\share\parent", null, null)]
        [DataRow(@"\\computer\share\parent\child", false, @"\\computer\share\parent\", null, null)]
        [DataRow(@"\\?\C:\parent1\\parent2\./../\child", false, @"C:\parent1\", null, null)]
        [DataRow(@"\\?\C:\parent1\\parent2\./../\child.txt", true, @"C:\parent1\parent3\/..\.", null, null)]
        [DataRow(@"C:\parent1\\parent2\./..//..\.\child.txt", true, @"C:\", null, null)]
        [DataRow(@"C:\parent1\\parent2\./../\child.txt", true, @"\\?\C:\", null, null)]
        [DataRow(@"C:\parent1\\parent2\./../\child.txt", true, @"\\?\C:", null, null)]
        [DataRow(@"C:\parent1\ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/ReallyLargeParent\\/LongFolder", false, @"C:\parent1", null, null)]
        #endregion Positive test cases
        #region Negative test cases
        [DataRow(@"\\computer", true, @"\\computer", typeof(InvalidOperationException), null)]
        [DataRow(@"\\computer", true, @"\\computer\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\computer\", false, @"\\computer", typeof(InvalidOperationException), null)]
        [DataRow(@"\\computer\", false, @"\\computer\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\computer\parent a\child", false, @"\\computer\parent b\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\computer\parent a\child", true, @"\\computer\parent b", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\parent1\\parent2\./../\child", false, @"C:\parent2\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\parent1\\parent2\./../\child.txt", true, @"C:\parent1\parent3\/..\.\parent3", typeof(InvalidOperationException), null)]
        [DataRow(@"C:\", false, @"C:\", typeof(InvalidOperationException), null)]
        [DataRow(@"C:\somefolder\", false, @"C:\somefolder\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\", false, @"C:\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\somefolder", false, @"\\?\C:\somefolder\", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\somefolder\", false, @"C:\somefolder", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:", false, @"\\?\C:", typeof(InvalidOperationException), null)]
        [DataRow(@"\\?\C:\", false, @"\\?\C:\", typeof(InvalidOperationException), null)]
        [DataRow(@"D:\", false, @"C:\", typeof(InvalidOperationException), null)]
        [DataRow(@"D:\parent\child.txt", true, @"C:\parent", typeof(InvalidOperationException), null)]
        [DataRow(@"C:", false, @"C:\", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: itemPath")]
        [DataRow(@"C:\", false, @"C:", typeof(ArgumentException), "Absolute path is expected\r\nParameter name: expectedParentFolder")]
        #endregion Negative test cases
        public void EnsureItemUnderParentTest(
            string itemPath,
            bool isFile,
            string expectedParentFolder,
            Type expectedException,
            string expectedMessage)
        {
            Exception ex = null;

            try
            {
                FSUtils.EnsureItemUnderParent(itemPath, isFile, expectedParentFolder);
            }
            catch (Exception caughtEx)
            {
                TestContext.WriteLine(caughtEx.ToString());
                ex = caughtEx;
            }

            Assert.AreEqual(expectedException, ex?.GetType());

            if (expectedMessage != null)
            {
                Assert.AreEqual(expectedMessage, ex?.Message);
            }
        }

        [TestMethod]
        [DataRow(1024)]
        [DataRow(1024 * 1024)]
        public void GzipCompressExtractTest(int maxFileSize)
        {
            Random rand = new Random();
            int filesize = 0;
            filesize = rand.Next(1, maxFileSize);

            byte[] fileContent = new byte[filesize];
            rand.NextBytes(fileContent);
            File.WriteAllBytes(s_originalFile, fileContent);

            string originalFileHash;
            using (var md5 = MD5.Create())
            {
                using (var stream = File.OpenRead(s_originalFile))
                {
                    var hash = md5.ComputeHash(stream);
                    originalFileHash = BitConverter.ToString(hash).ToLowerInvariant();
                }
            }

            FSUtils.CompressFile(s_originalFile, s_compressedFile);
            FSUtils.UncompressFile(s_compressedFile, s_extractedFile);

            string extractedFileHash;
            using (var md5 = MD5.Create())
            {
                using (var stream = File.OpenRead(s_extractedFile))
                {
                    var hash = md5.ComputeHash(stream);
                    extractedFileHash = BitConverter.ToString(hash).ToLowerInvariant();
                }
            }

            Assert.AreEqual(extractedFileHash, originalFileHash);
        }

        [TestMethod]
        [DataRow("C:\\e5cb8ffd-ad0d-41ba-b168-1a90e1f5a71d\\{3159046854}", "C:\\\\e5cb8ffd-ad0d-41ba-b168-1a90e1f5a71d\\\\{3159046854}")]
        [DataRow("C:\\eaccab14-00f4-4bc3-b8ec-cf7f084e5c15\\dev\\sdb", "C:\\\\eaccab14-00f4-4bc3-b8ec-cf7f084e5c15\\\\dev\\\\sdb")]
        [DataRow("36000c29bc00728bbb21500ccfad8ef8e", "36000c29bc00728bbb21500ccfad8ef8e")]
        [DataRow("/dev/mapper/36000c29e20183d26360ba3ad56a8e39f", "/dev/mapper/36000c29e20183d26360ba3ad56a8e39f")]
        public void FormatDeviceNameTest(string deviceName, string expectedDeviceName)
        {
            string formattedDeviceName = FSUtils.FormatDeviceName(deviceName);
            Assert.AreEqual(formattedDeviceName, expectedDeviceName);
        }

        [TestMethod]
        [DataRow(1024)]
        [DataRow(1024 * 1024)]
        public void GetUncompressedFileSizeTest(int maxFileSize)
        {
            Random rand = new Random();
            int filesize = 0;
            filesize = rand.Next(1, maxFileSize);

            byte[] fileContent = new byte[filesize];
            rand.NextBytes(fileContent);
            File.WriteAllBytes(s_originalFile, fileContent);

            string originalFileHash;
            using (var md5 = MD5.Create())
            {
                using (var stream = File.OpenRead(s_originalFile))
                {
                    var hash = md5.ComputeHash(stream);
                    originalFileHash = BitConverter.ToString(hash).ToLowerInvariant();
                }
            }

            FSUtils.CompressFile(s_originalFile, s_compressedFile);
            long compressedFileSize = FSUtils.GetUncompressedFileSize(s_compressedFile);
            Assert.AreEqual(compressedFileSize, new FileInfo(s_originalFile).Length);
        }

        [Ignore]
        [TestMethod]
        // Folders
        [DataRow(120, false, false)]
        [DataRow(245, false, false)]
        [DataRow(247, false, false)]
        [DataRow(248, false, true)]
        [DataRow(249, false, true)]
        [DataRow(255, false, true)]
        [DataRow(259, false, true)]
        [DataRow(260, false, true)]
        [DataRow(261, false, true)]
        [DataRow(289, false, true)]
        [DataRow(310, false, true)]
        [DataRow(5000, false, true)]
        // Files
        [DataRow(120, true, false)]
        [DataRow(245, true, false)]
        [DataRow(247, true, false)]
        [DataRow(248, true, false)]
        [DataRow(249, true, false)]
        [DataRow(255, false, true)]
        [DataRow(259, true, false)]
        [DataRow(260, true, true)]
        [DataRow(261, true, true)]
        [DataRow(289, true, true)]
        [DataRow(310, true, true)]
        [DataRow(5000, true, true)]
        public void GetLongPathTest(int length, bool isFile, bool needsLongPath)
        {
            // TODO-SanKumar-2006: These tests and validations succeeds even on
            // a machine with the LongPath setting enabled.

            //bool longPathEnabled = Convert.ToBoolean((int)
            //    Registry.LocalMachine
            //    .OpenSubKey(@"SYSTEM\CurrentControlSet\Control\FileSystem")
            //    .GetValue("LongPathsEnabled", 0));
            //Assert.IsFalse(longPathEnabled, "Long path is enabled on this Windows machine");

            StringBuilder inputPathSB = new StringBuilder(s_testFolderPath);

            const int MAX_COMPONENT_LEN = 255;
            char startChar = 'a';
            while (inputPathSB.Length < length)
            {
                inputPathSB.Append(Path.DirectorySeparatorChar);
                var pendingLen = length - inputPathSB.Length;
                Assert.IsTrue(pendingLen > 0, $"Fix the length {length} - couldn't append the last component name");
                inputPathSB.Append(startChar++, pendingLen > MAX_COMPONENT_LEN ? MAX_COMPONENT_LEN : pendingLen);
            }

            Assert.AreEqual(inputPathSB.Length, length);

            var inputPath = inputPathSB.ToString();

            GetLongPathTestInternal(optimalPath: false);
            GetLongPathTestInternal(optimalPath: true);

            void GetLongPathTestInternal(bool optimalPath)
            {
                try
                {
                    CreateDeletePath(inputPath);

                    Assert.IsFalse(needsLongPath, $"Path incorrectly expected to need long path - {inputPath}");
                }
                catch (PathTooLongException) when (needsLongPath)
                {
                    // Expected
                }
                catch (DirectoryNotFoundException) when (needsLongPath)
                {
                    // Expected

                    // TODO-SanKumar-2006: This particular test only works, if
                    // the path is >= 260 for both directory and file. Why isn't
                    // the create directory failing for 248 <= size < 260?
                }

                string returnedPath = FSUtils.GetLongPath(inputPath, isFile, optimalPath);

                Assert.IsTrue(
                    !optimalPath || needsLongPath || !returnedPath.StartsWith(@"\\?\"),
                    $"Path unexpectedly converted to long path - {inputPath} - {returnedPath}");

                CreateDeletePath(returnedPath);
            }

            void CreateDeletePath(string path)
            {
                if (isFile)
                {
                    string parentDir = Path.GetDirectoryName(path);

                    Directory.CreateDirectory(parentDir);

                    File.OpenWrite(path).Close();

                    File.Delete(path);

                    Directory.Delete(parentDir);
                }
                else
                {
                    Directory.CreateDirectory(path);

                    Directory.Delete(path, recursive: true);
                }
            }
        }

        [TestMethod]
        // Already in long path notation
        [DataRow(@"\\?\C:\", false, false, @"\\?\C:\", null, null)]
        [DataRow(@"//?/C:/", false, false, @"\\?\C:\", null, null)]
        [DataRow(@"\\.\C:\SomeFolder", false, false, @"\\.\C:\SomeFolder", null, null)]
        [DataRow(@"//./C:/SomeFolder", false, false, @"\\.\C:\SomeFolder", null, null)]
        [DataRow(@"\??\C:\SomeFolder\SomeFile.txt", false, false, @"\??\C:\SomeFolder\SomeFile.txt", null, null)]
        [DataRow(@"/??/C:/SomeFolder/SomeFile.txt", false, false, @"\??\C:\SomeFolder\SomeFile.txt", null, null)]
        // Canonicalization applied in both optimal and unoptimal conversions - mandatory for long
        // path to work
        [DataRow(@"C:/", true, true, @"C:\", null, null)]
        [DataRow(@"C:/", true, false, @"\\?\C:\", null, null)]
        [DataRow(@"C:\SomeFolder/SomeFile.txt", true, true, @"C:\SomeFolder\SomeFile.txt", null, null)]
        [DataRow(@"C:\SomeFolder/SomeFile.txt", true, false, @"\\?\C:\SomeFolder\SomeFile.txt", null, null)]
        // UNC paths
        [DataRow(@"\\computer", false, true, @"\\computer", null, null)]
        [DataRow(@"//computer", false, false, @"\\?\UNC\computer", null, null)]
        [DataRow(@"\\computer\share\file.txt", true, true, @"\\computer\share\file.txt", null, null)]
        [DataRow(@"\\computer\share\file.txt", true, false, @"\\?\UNC\computer\share\file.txt", null, null)]
        [DataRow(@"\\?\UNC\computer\share", false, true, @"\\?\UNC\computer\share", null, null)]
        [DataRow(@"\\?\UNC\computer\share\file.txt", true, false, @"\\?\UNC\computer\share\file.txt", null, null)]
        // Paths ending with . or space
        [DataRow(@"C:\SomeFolder\SomeFile.txt ", true, true, @"\\?\C:\SomeFolder\SomeFile.txt ", null, null)]
        [DataRow(@"C:\SomeFolder.", true, true, @"\\?\C:\SomeFolder.", null, null)]
        // Negative cases
        [DataRow(null, true, false, null, typeof(ArgumentNullException), "Value cannot be null.\r\nParameter name: path")]
        [DataRow("", true, false, null, typeof(NotSupportedException), "Long paths aren't supported for relative paths")]
        [DataRow("   ", false, true, null, typeof(NotSupportedException), "Long paths aren't supported for relative paths")]
        [DataRow("relative", true, true, null, typeof(NotSupportedException), "Long paths aren't supported for relative paths")]
        public void GetLongPathAdditionalTest(
            string inputPath,
            bool isFile,
            bool optimalPath,
            string expectedPath,
            Type expectedException,
            string expectedMessage)
        {
            string returnedPath;

            try
            {
                returnedPath = FSUtils.GetLongPath(inputPath, isFile, optimalPath);
            }
            catch (Exception ex)
            {
                TestContext.WriteLine(ex.ToString());

                Assert.AreEqual(expectedException, ex.GetType());

                if (expectedMessage != null)
                {
                    Assert.AreEqual(expectedMessage, ex.Message);
                }

                return;
            }

            Assert.AreEqual(expectedPath, returnedPath);
        }

        [TestMethod]
        [DataRow(5UL, "5 B")]
        [DataRow(0UL, "0 B")]
        [DataRow(555UL, "555 B")]
        [DataRow(1024UL, "1024 B")]
        [DataRow(1026UL, "1 KB")]
        [DataRow(1048576UL, "1024 KB")]
        [DataRow(1048578UL, "1024 KB")]
        [DataRow(1073741824UL, "1024 MB")]
        public void GetMemoryWithHighestUnitsTest(ulong memoryInBytes, string memoryAsString)
        {
            Assert.AreEqual(FSUtils.GetMemoryWithHighestUnits(memoryInBytes), memoryAsString);
        }
    }
}
