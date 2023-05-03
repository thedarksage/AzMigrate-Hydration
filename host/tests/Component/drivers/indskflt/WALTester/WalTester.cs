using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using Microsoft.Test.Storages;
using ManagedStorageProvider;
using FluentAssertions;
using System.Threading.Tasks;
using System.Threading;
using System.IO;
using System.Linq;
using System.Text;

namespace WALTester
{
    [TestClass]
    public class WALTester
    {
        [DllImport("C:\\projects\\Debug\\S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern IntPtr S2AgentCreate(int replicationType, string testRoot);

        [DllImport("C:\\projects\\Debug\\S2Agent.dll")]
        static public extern void S2AgentDispose(IntPtr pTestClassObject);

        [DllImport("C:\\projects\\Debug\\S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentStartReplication(IntPtr pClassObject, UInt64 startOffset, UInt64 endOffset);

        [DllImport("C:\\projects\\Debug\\S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentAddReplicationSettings(IntPtr pClassObject, int uiSrcDisk, int uiDestDisk, StringBuilder logFile);

        [DllImport("C:\\projects\\Debug\\S2Agent.dll", CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentResetTSO(IntPtr pClassObject, int uiSrcDisk);

        [DllImport("C:\\projects\\Debug\\S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentInsertTag(IntPtr pObject, string tagName);

        private static IDictionary<int, PhysicalDrive> m_physicalDrives;
        private static IDictionary<int, PhysicalDrive> m_workingDrives = new SortedDictionary<int, PhysicalDrive> ();
        private static ulong m_maxCounter = ulong.MaxValue;
        private static ulong m_bytesPerSector = 0;
        private static string m_testRootSrc = "c:\\tmp\\WalTest";
        static IntPtr s2Agent;

        [ClassInitialize]
        public static void InitializeWALSetup(TestContext context)
        {
            m_physicalDrives = StorageProvider.GetPhysicalDrives(bIncludeSystemDisk: false);
            m_physicalDrives.Count.Should().BeGreaterThan(0);

            foreach(int diskIndex in m_physicalDrives.Keys)
            {
                if (0 == m_bytesPerSector)
                {
                    m_bytesPerSector = m_physicalDrives[diskIndex].BytesPerSector;
                }

                if (m_physicalDrives[diskIndex].BytesPerSector != m_bytesPerSector)
                {
                    continue;
                }

                if (m_physicalDrives[diskIndex].Size < m_maxCounter)
                {
                    m_maxCounter = m_physicalDrives[diskIndex].Size;
                }
                m_workingDrives.Add(new KeyValuePair<int, PhysicalDrive>(diskIndex, m_physicalDrives[diskIndex]));
            }

            m_workingDrives.Count.Should().BeGreaterThan(0);


            try
            {
                // Delete all files from root folder
                var dirs = Directory.GetDirectories(m_testRootSrc);
                foreach (string dir in dirs)
                {
                    DirectoryInfo dirInfo = new DirectoryInfo(dir);
                    dirInfo.Delete(recursive: true);
                }

            }
            catch(DirectoryNotFoundException ex)
            {
                Directory.CreateDirectory(m_testRootSrc);
            }

            s2Agent = S2AgentCreate(1, m_testRootSrc);
            foreach (var disk in m_workingDrives.Values)
            {
                StringBuilder strb = new StringBuilder();
                strb.AppendFormat("c:\\tmp\\PhysicalDrive{0}.log", disk.DiskNumber.ToString());

                S2AgentAddReplicationSettings(s2Agent, disk.DiskNumber, -1, strb);
            }

            S2AgentStartReplication(s2Agent, 0, 0);

        }

        [TestMethod]
        [TestCategory("WriteOrderTest")]
        public void SingleDiskSectorWriteShouldBeWOConsistent()
        {
            Task writeTask = Task.Run(() => GenerateWrites());
            Task validationTask = Task.Run(() => ValidationThread());

            while(!writeTask.IsCompleted || !writeTask.IsCanceled || !validationTask.IsCompleted)
            {
                // Issue a tag
                S2AgentInsertTag(s2Agent, Guid.NewGuid().ToString());
                Thread.Sleep(5*60 * 1000);
            }
        }

        void ValidationThread()
        {
            int count = 1;

            StringBuilder sb = new StringBuilder();

            while (true)
            {
                sb.Clear();
                sb.AppendFormat("{0}\\{1}", m_testRootSrc, (count + 1));
                if (Directory.Exists(sb.ToString()))
                {
                    // Validate it.
                    ValidateDisksWO(m_testRootSrc + "\\" + count).Should().BeTrue();
                    ++count;
                }
            }

        }
        void GenerateWrites()
        {
            ulong counter = 1;
            byte[] buffer = new byte[m_bytesPerSector];
            int destOffset = (int) m_bytesPerSector - sizeof(ulong);
            ulong storageOffset = 1;
            Array.Clear(buffer, 0, buffer.Length);

            string datafile = @"c:\tmp\data.log";
            
            // Create a file to write to. 
            using (StreamWriter sw = File.CreateText(datafile))
            {
                sw.WriteLine("Game Started");
                while (true)
                {
                    Buffer.BlockCopy(BitConverter.GetBytes(counter), 0, buffer, destOffset, sizeof(ulong));

                    foreach (var disk in m_workingDrives.Keys)
                    {
                        m_workingDrives[disk].Write(buffer, (uint)m_bytesPerSector, 0x1000);
                    }
                    counter++;
                    storageOffset++;
                    Thread.Sleep(100);
                }
                
            }
        }

        bool ValidateDisksWO(string walRoot)
        {
            DirectoryInfo dirInfo = new DirectoryInfo(walRoot);
            var dirs = dirInfo.GetDirectories().OrderBy(d => d.Name);
            bool bCanDecrement = true;

            byte[] ulCounter = new byte[m_bytesPerSector];

            UInt64 validDiskCounter = UInt64.MaxValue;


            // Enumerate all disk directories inside wal root
            foreach(var currentDiskDir in dirs)
            {
                string dirName = currentDiskDir.FullName;

                // Enumerate all files in currentDiskDir
                var files = currentDiskDir.EnumerateFiles().OrderByDescending(f => f.Name);
                if (0 == files.Count())
                {
                    return true;
                }

                string fileName = files.First().FullName;

                UInt64 contentCounter;

                // Read Contents for first file
                using (FileStream fs = new FileStream(fileName, FileMode.Open, FileAccess.Read))
                {
                    using (BinaryReader br = new BinaryReader(fs, new ASCIIEncoding()))
                    {
                        byte[] contents = br.ReadBytes((int) m_bytesPerSector);
                        contentCounter = BitConverter.ToUInt64(contents, (int) m_bytesPerSector - sizeof(UInt64));
                    }
                }

                // This is first disk .. so continue
                if (validDiskCounter == UInt64.MaxValue)
                {
                    validDiskCounter = contentCounter;
                    continue;
                }

                if (contentCounter == validDiskCounter)
                {
                    continue;
                }

                if (bCanDecrement)
                {
                    bCanDecrement = false;
                    if (contentCounter == validDiskCounter - 1)
                    {
                        validDiskCounter--;
                        continue;
                    }
                }

                return false;
            }

            return true;
        }
    }
}
