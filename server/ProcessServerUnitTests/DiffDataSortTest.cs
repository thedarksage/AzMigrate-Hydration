using System;
using System.IO;
using System.Linq;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ProcessServer;

namespace ProcessServerUnitTests
{
    /// <summary>
    /// Summary description for DiffDataSortTest
    /// </summary>
    [TestClass]
    public class DiffDataSortTest
    {
        public DiffDataSortTest()
        {
            //
            // TODO: Add constructor logic here
            //
        }

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

        private static readonly Random s_rand = new Random();
        private static readonly string s_testFolderPath = Path.Combine(Path.GetTempPath(), "DiffdatasortTest");
        private static readonly string s_nativeDiffdatasortResultFile = "nativedds.txt";
        private static readonly string s_managedDiffdatasortResultFile = "manageddds.txt";
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
        // Use TestInitialize to run code before running each test 
         [TestInitialize()]
         public void MyTestInitialize()
        {
            if (Directory.Exists(s_testFolderPath))
                Directory.Delete(s_testFolderPath, true);
        }
        
        // Use TestCleanup to run code after each test has run
         [TestCleanup()]
         public void MyTestCleanup()
        {
            if (Directory.Exists(s_testFolderPath))
                Directory.Delete(s_testFolderPath, true);
            if(File.Exists(s_nativeDiffdatasortResultFile))
                File.Delete(s_nativeDiffdatasortResultFile);
            if(File.Exists(s_managedDiffdatasortResultFile))
                File.Delete(s_managedDiffdatasortResultFile);
        }
        
        #endregion

        [TestMethod]
        [DataRow("completed_diff_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_tag_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_tso_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_tag_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_tso_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, true)]
        [DataRow("completed_diff_tso__P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, false)]
        [DataRow("completed_ediffcompleted_diff_tso__P127794935171250000_1000_5_E127794935212187500_2000_ME1_1549f93984040.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, false)]
        [DataRow("completed_ediffcompleted_diff_tso__P127794935171250000_1000_5_E127794935212187500_2000_ME1.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, false)]
        [DataRow("completed_diff_tso__P127794935171250000_1000_5_E127794935212187500_2000_ME1.dat.gz", 127794935171250000, 1000, 5, 127794935212187500, 2000, 1, false)]
        public void DiffFileParserTest(string filename,
            long prevTimeStamp,
            long prevSequenceNumber,
            long prevContinuationId,
            long endTimeStamp,
            long endSequenceNumber,
            long endContinuationId,
            bool isValidFile)
        {
            DiffFileParser diffFileParser = null;
            try
            {
                diffFileParser = new DiffFileParser(filename);
            }
            catch(Exception ex)
            {
                TestContext.WriteLine(ex.ToString());
                Assert.IsFalse(isValidFile);
                return;
            }

            Assert.IsTrue(isValidFile);
            Assert.AreEqual(diffFileParser.PrevTimestamp, (ulong)prevTimeStamp);
            Assert.AreEqual(diffFileParser.PrevContinuationId, prevContinuationId);
            Assert.AreEqual(diffFileParser.PrevSequenceNumber, (ulong)prevSequenceNumber);
            Assert.AreEqual(diffFileParser.EndTimestamp, (ulong)endTimeStamp);
            Assert.AreEqual(diffFileParser.EndContinuationId, endContinuationId);
            Assert.AreEqual(diffFileParser.EndSequenceNumber, (ulong)endSequenceNumber);
        }

        /// <summary>
        /// This method compares the result of native and managed diffdatasort
        /// Copy native diffdatasort binary to output folder before
        /// running this test.
        /// </summary>
        /// <param name="directory">Directory where to run the test</param>
        /// <param name="nooffiles">No of files to be created in the directory</param>
        /// <param name="filesize">Max size of each file</param>
        [TestMethod]
        [DataRow(100, 100)]
        [DataRow(1000, 100)]
        [DataRow(2000, 100)]
        public void DiffdatasortTest(int noOfFiles, int filesize)
        {
            Directory.CreateDirectory(s_testFolderPath);

            GenerateFilesForPair(s_testFolderPath, noOfFiles, filesize);
            ManagedDiffDataSort(s_testFolderPath, "completed_diff");
            NativeDiffDataSort(s_testFolderPath);

            Assert.IsTrue(File.ReadAllText(s_nativeDiffdatasortResultFile).Equals(File.ReadAllText(s_managedDiffdatasortResultFile), StringComparison.Ordinal));
        }

        public static void ManagedDiffDataSort(string directory, string pattern)
        {
            DiffDataSort diffdatasort = new DiffDataSort(directory, pattern);
            var sortedfiles = diffdatasort.SortFiles();
            File.WriteAllLines(s_managedDiffdatasortResultFile, sortedfiles.Select(item => Path.GetFileName(item)));
        }

        static void NativeDiffDataSort(string directory)
        {
            string diffdatasortPath = "\"" + @"diffdatasort.exe" + "\"";
            directory = directory.TrimEnd(new char[] { '/', '\\' });
            directory = directory + "/";
            // copy diffdatasort to current directory
            string args = "-p " + "completed_diff" + " \"" + directory + "\"";
            int exitcode = ProcessUtils.RunProcess(diffdatasortPath, args, out string stdout, out string stderr, out int pid);

            if (exitcode != 0)
                throw new Exception($"diffdatasort failed for directory {directory} with error {stderr} and exitcode {exitcode}");

            File.WriteAllLines(s_nativeDiffdatasortResultFile, stdout.Split(
                new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries)
                .Select(item => Path.GetFileName(item)));
        }

        static void GenerateFilesForPair(string directory, int noOfFiles, int filesize)
        {
            for (int i = 0; i < noOfFiles; i++)
            {
                DateTime currTime = DateTime.UtcNow;
                long currTicks = currTime.Ticks;
                long currSec = new DateTimeOffset(currTime).ToUnixTimeSeconds();
                long randinterval = s_rand.Next();
                string prevTS = (currTicks + randinterval).ToString();
                string nextTS = (currTicks + randinterval + s_rand.Next(100000)).ToString();
                randinterval = s_rand.Next(10000);
                string prevContId = (currSec + randinterval).ToString();
                string nextContId = (currSec + randinterval + s_rand.Next(100)).ToString();
                int id1 = s_rand.Next(10);
                int id2 = s_rand.Next(10);
                string ext;
                // 1:1 ratio of compressed and uncompressed files
                if (s_rand.Next() % 2 == 0)
                {
                    ext = ".dat";
                }
                else
                {
                    ext = ".dat.gz";
                }

                string filename;
                int ft = s_rand.Next(5);
                //Make 20% files as TSO and 20% as TAG files
                if (ft % 5 == 1)
                {
                    filename = $"completed_diff_tag_P{prevTS}_{prevContId}_{id1}_E{nextTS}_{nextContId}_WE{id2}{ext}";
                }
                else if (ft % 5 == 2)
                {
                    filename = $"completed_diff_tso_P{prevTS}_{prevContId}_{id1}_E{nextTS}_{nextContId}_WE{id2}{ext}";
                }
                else
                {
                    filename = $"completed_diff_P{prevTS}_{prevContId}_{id1}_E{nextTS}_{nextContId}_WE{id2}{ext}";
                }
                filename = Path.Combine(directory, filename);

                byte[] fileContent = new byte[filesize];
                s_rand.NextBytes(fileContent);
                File.WriteAllBytes(filename, fileContent);
            }
        }
    }
}
