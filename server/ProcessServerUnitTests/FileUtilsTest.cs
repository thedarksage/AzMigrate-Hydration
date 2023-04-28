using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.IO;
using System.Security.AccessControl;
using System.Security.Principal;

namespace ProcessServerUnitTests
{
    [TestClass]
    public class FileUtilsTest
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

        private static readonly Random s_rand = new Random();
        private static readonly string s_testFolderPath = Path.Combine(Path.GetTempPath(), "FileUtilsTest");

        //// Use ClassInitialize to run code before running the first test in the class
        //[ClassInitialize()]
        //public static void MyClassInitialize(TestContext testContext)
        //{
        //}

        //// Use ClassCleanup to run code after all tests in a class have run
        //[ClassCleanup()]
        //public static void MyClassCleanup()
        //{
        //}

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
            if (Directory.Exists(s_testFolderPath))
                Directory.Delete(s_testFolderPath, recursive: true);
        }

        #endregion

        [TestMethod]
        [DataRow(null, "tmp.txt", "backup_prev.txt", "backup_curr.txt", typeof(ArgumentNullException), "Value cannot be null.\r\nParameter name: filePath")]
        [DataRow("file.txt", null, "backup_prev.txt", "backup_curr.txt", typeof(ArgumentNullException), "Value cannot be null.\r\nParameter name: tmpFilePath")]
        [DataRow("file.txt", "tmp.txt", "backup_prev.txt", "backup_curr.txt", null, null)]
        [DataRow(@"File\file.txt", "tmp.txt", "backup_prev.txt", "backup_curr.txt", null, null)]
        [DataRow("file.txt", "tmp.txt", null, null, null, null)]
        [DataRow("file.txt", @"Temp\tmp.txt", null, null, null, null)]
        [DataRow("file.txt", "tmp.txt", @"Backup\backup_prev.txt", "backup_curr.txt", null, null)]
        [DataRow("file.txt", "tmp.txt", "backup_prev.txt", @"Backup\backup_curr.txt", null, null)]
        [DataRow(@"Intermediate\file.txt", @"Intermediate\tmp.txt", @"Intermediate\backup_prev.txt", @"Intermediate\backup_curr.txt", null, null)]
        public void ReliableSaveFileTest(
            string filePath,
            string tmpFilePath,
            string prevBakFilePath,
            string currBakFilePath,
            Type expectedException,
            string expectedMessage)
        {
            // Create the test folder
            Directory.CreateDirectory(s_testFolderPath);

            Exception ex = null;

            // Put the paths under the test folder
            if (filePath != null)
                filePath = Path.Combine(s_testFolderPath, filePath);

            if (tmpFilePath != null)
                tmpFilePath = Path.Combine(s_testFolderPath, tmpFilePath);

            if (prevBakFilePath != null)
                prevBakFilePath = Path.Combine(s_testFolderPath, prevBakFilePath);

            if (currBakFilePath != null)
                currBakFilePath = Path.Combine(s_testFolderPath, currBakFilePath);

            try
            {
                // Pattern of usage of this method. File is created for the first
                // time and then on replaced with the future changes.

                // NOTE-SanKumar-2004: This test must be RUN AS ADMINISTRATOR
                FileUtils.ReliableSaveFile(
                    filePath,
                    tmpFilePath,
                    prevBakFilePath,
                    currBakFilePath,
                    multipleBackups: false, // TODO-SanKumar-2004: Write tests for multiple backups
                    writeData: sw =>
                    {
                        if (prevBakFilePath != null)
                            sw.Write(prevBakFilePath);
                    },
                    flush: s_rand.Next(0, 2) == 0);

                FileUtils.ReliableSaveFile(
                    filePath,
                    tmpFilePath,
                    prevBakFilePath,
                    currBakFilePath,
                    multipleBackups: false, // TODO-SanKumar-2004: Write tests for multiple backups
                    writeData: sw => sw.Write(filePath),
                    flush: s_rand.Next(0, 2) == 0);
            }
            catch (Exception caughtEx)
            {
                TestContext.WriteLine(caughtEx.ToString());
                ex = caughtEx;
            }

            // NOTE-SanKumar-2004: This test must be RUN AS ADMINISTRATOR
            Assert.AreEqual(expectedException, ex?.GetType());

            if (expectedMessage != null)
            {
                Assert.AreEqual(expectedMessage, ex?.Message);
            }

            if (expectedException == null)
            {
                Assert.AreEqual(filePath, File.ReadAllText(filePath));
                ValidateAccess(File.GetAccessControl(filePath, AccessControlSections.Access), isDir: false);

                if (currBakFilePath != null)
                {
                    Assert.AreEqual(filePath, File.ReadAllText(currBakFilePath));
                    ValidateAccess(File.GetAccessControl(currBakFilePath, AccessControlSections.Access), isDir: false);

                    var bakDir = Path.GetDirectoryName(currBakFilePath);
                    if (bakDir != s_testFolderPath)
                        ValidateAccess(Directory.GetAccessControl(bakDir, AccessControlSections.Access), isDir: true);
                }

                if (prevBakFilePath != null)
                {
                    Assert.AreEqual(prevBakFilePath, File.ReadAllText(prevBakFilePath));
                    ValidateAccess(File.GetAccessControl(prevBakFilePath, AccessControlSections.Access), isDir: false);

                    var bakDir = Path.GetDirectoryName(prevBakFilePath);
                    if (bakDir != s_testFolderPath)
                        ValidateAccess(Directory.GetAccessControl(bakDir, AccessControlSections.Access), isDir: true);
                }

                if (tmpFilePath != null)
                {
                    Assert.IsFalse(File.Exists(tmpFilePath));

                    var tmpDir = Path.GetDirectoryName(tmpFilePath);
                    if (tmpDir != s_testFolderPath)
                        ValidateAccess(Directory.GetAccessControl(tmpDir, AccessControlSections.Access), isDir: true);
                }
            }
        }

        private static void ValidateAccess(FileSystemSecurity security, bool isDir)
        {
            Assert.IsTrue(security.AreAccessRulesProtected);
            var rules = security.GetAccessRules(includeExplicit: true, includeInherited: true, targetType:typeof(SecurityIdentifier));
            Assert.AreEqual(2, rules.Count);
            Assert.IsTrue(
                (rules[0].IdentityReference.Value == FSUtils.AdministratorsSid.Value && rules[1].IdentityReference.Value == FSUtils.LocalSystemSid.Value) ||
                (rules[0].IdentityReference.Value == FSUtils.LocalSystemSid.Value && rules[1].IdentityReference.Value == FSUtils.AdministratorsSid.Value));
            foreach (AuthorizationRule currRule in rules)
            {
                Assert.IsFalse(currRule.IsInherited);
                Assert.AreEqual(PropagationFlags.None, currRule.PropagationFlags);
                Assert.AreEqual(
                    isDir ? InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit : InheritanceFlags.None,
                    currRule.InheritanceFlags);
            }
        }
    }
}
