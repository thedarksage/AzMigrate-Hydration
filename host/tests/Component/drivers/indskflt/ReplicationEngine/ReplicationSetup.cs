using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Test.Storages;
using System.Management;
using System.Runtime.InteropServices;
using System.Xml;
using System.IO;
using System.Threading;
using FluentAssertions;
using BlockValidator = ManagedBlockValidator.BlockValidator;
using InMageLogger;

namespace ReplicationEngine
{
    public class WmiDisk
    {
        public UInt32 Index { get; private set; }
        public UInt32 BytesPerSector { get; private set; }
        public UInt64 Size { get; private set; }
        public string DeviceId { get; private set; }

        public WmiDisk(UInt32 index, UInt32 bytesPerSector, UInt64 size)
        {
            this.Index = index;
            this.BytesPerSector = bytesPerSector;
            this.Size = size;
            DeviceId = "";
        }
        public WmiDisk(UInt32 index, UInt32 bytesPerSector, UInt64 size, string id)
        {
            this.Index = index;
            this.BytesPerSector = bytesPerSector;
            this.Size = size;
            DeviceId = id;
        }
    }

    public abstract class ReplicationSetup
    {
        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern IntPtr S2AgentCreate(int replicationType, string testRoot);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void StartSVAgents(IntPtr pClassObject, ulong ulFlags = 0);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll")]
        static public extern void S2AgentDispose(IntPtr pTestClassObject);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentStartReplication(IntPtr pClassObject, UInt64 startOffset, UInt64 endOffset);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentAddFileReplicationSettings(IntPtr pClassObject, int uiSrcDisk, StringBuilder target, StringBuilder logFile);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static public extern void S2AgentAddReplicationSettings(IntPtr pClassObject, int uiSrcDisk, int uiDestDisk, StringBuilder logFile);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        static public extern bool S2AgentInsertTag(IntPtr pObject, string tagName);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        static public extern bool S2AgentWaitForConsistentState(IntPtr pClassObject, bool dontApply = false);

        [DllImport("Microsoft.Test.Inmage.S2Agent.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        static public extern bool SetDriverFlags(IntPtr pClassObject, BitOperation flag);
        
        static protected List<WmiDisk> DiskList = new List<WmiDisk>();
        static protected IDictionary<PhysicalDrive, Win32ApiFile> m_srcTargetMap = new Dictionary<PhysicalDrive, Win32ApiFile>();
 		static protected uint SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING = 0x00000001;
        static protected IntPtr s_s2Agent;
        static protected string s_logDir = @"C:\tmp";
        protected static Logger s_logger = null;
        static protected string s_destDir = @"E:\diskfiles";


        public enum BitOperation
        {
            BitOpNotDefined = 0,
            BitOpSet = 1,
            BitOpReset = 2
        };

        public static bool IsSystemDisk(UInt32 diskIndex)
        {
            return (0 == diskIndex);
        }

        public static void InitSrcTgtDiskMap(int numDisks)
        {
            CreateDir(s_destDir);
            CreateDir(s_logDir);

            ManagementObjectSearcher searcher = new
                ManagementObjectSearcher("SELECT * FROM Win32_DiskDrive");

            foreach (ManagementObject wmi_HD in searcher.Get())
            {
                UInt32 uDiskIndex = UInt32.Parse(wmi_HD["Index"].ToString());
                UInt32 uBytesPerSector = UInt32.Parse(wmi_HD["BytesPerSector"].ToString());
                UInt64 uSize = UInt64.Parse(wmi_HD["Size"].ToString());
                string deviceId = wmi_HD["DeviceID"].ToString();
                string signature = wmi_HD["Signature"].ToString();
                if (!IsSystemDisk(uDiskIndex))
                {
                    DiskList.Add(new WmiDisk(uDiskIndex, uBytesPerSector, uSize, signature));
                }
            }

            DiskList.Sort((d1, d2) => d1.Index.CompareTo(d2.Index));

            var max = DiskList.Max(x => x.Size);
            DiskList.RemoveAll(x => x.Size == max);

            DiskList.Sort((d1, d2) => d1.Size.CompareTo(d2.Size));

            if (DiskList.Count < numDisks)
            {
                throw new Exception("Tests startup failed. Disks count = " + DiskList.Count);
            }

            HashSet<string> diskIds = new HashSet<string>();
            for (int i = 0; i < numDisks; i++)
            {

                WmiDisk srcDisk = DiskList[i];
                if ((0 != diskIds.Count) && diskIds.Contains(srcDisk.DeviceId))
                {
                    continue;
                }

                diskIds.Add(srcDisk.DeviceId);
                string targetFile = string.Format(@"E:\diskfiles\disk{0}.dsk", srcDisk.Index.ToString());

                m_srcTargetMap.Add(new KeyValuePair<PhysicalDrive, Win32ApiFile>(
                    new PhysicalDrive((int)srcDisk.Index, srcDisk.BytesPerSector, srcDisk.Size),
                    new Win32ApiFile(targetFile, (long)srcDisk.Size)));

                s_logger.LogInfo("Source Disk Size : " + srcDisk.Size);
                FileInfo fi = new FileInfo(targetFile);
                s_logger.LogInfo("Target File Size : " + fi.Length);
            }
        }

        protected static void CreateDir(string dir)
        {
            if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        }

		public static bool DeleteDir(string dir)
        {
            if (Directory.Exists(dir)) Directory.Delete(dir, true);

            return true;
        }

        protected static void InitializeReplication(string testName, UInt64 startOffset, UInt64 endOffset)
        {
            s_s2Agent = S2AgentCreate(0, s_logDir);

            foreach (var disk in m_srcTargetMap.Keys)
            {
                StringBuilder target = new StringBuilder();
                target = new StringBuilder();
                target.AppendFormat(@"E:\diskfiles\disk{0}.dsk", disk.DiskNumber.ToString());
                StringBuilder strb = new StringBuilder();
                strb.AppendFormat(s_logDir +  @"\PhysicalDrive{0}_{1}.log", disk.DiskNumber.ToString(), testName);
                S2AgentAddFileReplicationSettings(s_s2Agent, disk.DiskNumber, target, strb);
                //endOffset = (endOffset != 0) ? disk.Size : endOffset;
            }

            S2AgentStartReplication(s_s2Agent, startOffset, endOffset);
        }

        public static void CleanupReplication()
        {
            S2AgentDispose(s_s2Agent);
        }

        protected static int ValidateDisks(string testName)
        {
            bool done = false;

            s_logger.LogInfo("Wait for tag");
            S2AgentWaitForConsistentState(s_s2Agent, false).Should().BeTrue();
            s_logger.LogInfo("Tag is issued successfully");

            foreach (var disk in m_srcTargetMap.Keys)
            {
                s_logger.LogInfo("Validation started for test " +testName + " on disk : " + disk.DiskNumber);
                s_logger.LogInfo("Source Disk Size : " + disk.Size);
				done = BlockValidator.Validate(disk, m_srcTargetMap[disk], disk.Size, s_logger);
                string output = done ? "PASSED" : "FAILED";
                s_logger.LogInfo(testName + " : Validation : " + output);
                done.Should().BeTrue();
            }

            int status = (done) ? 0 : 1;

            return status;
        }

        protected static void OfflineDisks(bool readOnly)
        {
            // Offline source in readonly mode
            foreach (var disk in m_srcTargetMap.Keys)
            {
                disk.Offline(readOnly);
            }
        }

        protected static void OnlineDisks(bool readOnly)
        {
            // Clear readonly attributes on disk
            foreach (var disk in m_srcTargetMap.Keys)
            {
                disk.Online(readOnly);
            }
        }
    }
}
