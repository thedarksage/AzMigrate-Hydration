using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Collections;
using Microsoft.Test.Storages;
using ManagedBlockValidator;
using System.Runtime.InteropServices;
using System.Management;
using InMageLogger;

namespace RebootGenTests
{
    class RebootGenTest
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
        [return: MarshalAs(UnmanagedType.I1)]
        static public extern bool S2AgentWaitForConsistentState(IntPtr pClassObject);

        static IDictionary<PhysicalDrive, PhysicalDrive> m_srcTargetMap = new Dictionary<PhysicalDrive, PhysicalDrive>();
        static public IntPtr s2Agent;
        static Logger logger = new Logger("C:\\tmp\\RebootTests.txt", true);
        static protected List<WmiDisk> DiskList = new List<WmiDisk>();

        public class WmiDisk
        {
            public UInt32 Index { get; private set; }
            public UInt32 BytesPerSector { get; private set; }
            public UInt64 Size { get; private set; }

            public WmiDisk(UInt32 index, UInt32 bytesPerSector, UInt64 size)
            {
                this.Index = index;
                this.BytesPerSector = bytesPerSector;
                this.Size = size;
            }
        }

        public static bool InitSrcTgtDiskMap()
        {
            m_srcTargetMap.Add(new KeyValuePair<PhysicalDrive, PhysicalDrive>(
                    new PhysicalDrive((int)2),
                    new PhysicalDrive((int)3)));

            return true;
        }

        public static void StartReplication(bool initSync)
        {
            logger.LogInfo("Entered StartReplication");
            s2Agent = S2AgentCreate(0, "C:\\tmp");

            UInt64 endOffset = 512;
            foreach (var disk in m_srcTargetMap.Keys)
            {
                StringBuilder strb = new StringBuilder();
                strb.AppendFormat("C:\\tmp\\PhysicalDrive{0}_{1}.log", disk.DiskNumber.ToString(), "StartReplication");
                S2AgentAddReplicationSettings(s2Agent, disk.DiskNumber, m_srcTargetMap[disk].DiskNumber, strb);
                endOffset = (initSync) ? endOffset : 0;
            }

            S2AgentStartReplication(s2Agent, 0, endOffset);
            logger.LogInfo("End of StartReplication");

            // online offline target disk for signature change
            if (initSync)
            {
                foreach (var disk in m_srcTargetMap.Keys)
                {
                    logger.LogInfo("Online target - " + m_srcTargetMap[disk].DiskNumber + " in readonly mode");
                    m_srcTargetMap[disk].Online(readOnly: false);
                    Thread.Sleep(10 * 1000);
                    logger.LogInfo("Online target in " + m_srcTargetMap[disk].DiskNumber + " in r-w mode");
                    m_srcTargetMap[disk].Offline(readOnly: false);
                }
            }

        }

        public static bool ValidateDisks()
        {
            bool status = false;

            status = S2AgentWaitForConsistentState(s2Agent);
            if (!status) return false;

            Logger logger = new Logger("C:\\tmp\\RebootGenTests.txt", true);
            foreach (var disk in m_srcTargetMap.Keys)
            {
                logger.LogInfo("Reboot Tests Validation started");
                status = BlockValidator.Validate(disk, m_srcTargetMap[disk], disk.Size, logger, 512);
                string output = (status) ? "PASSED" : "FAILED";
                logger.LogInfo("" + output);
                if (!status) break;
            }

            // Clear readonly attributes on disk
            foreach (var disk in m_srcTargetMap.Keys)
            {
                logger.LogInfo("Online source disk - " + disk.DiskNumber + " in read write mode");
                disk.Online(false);

            }

            return status;
        }

        public static bool WriteDataUsingIOMeter()
        {
            ProcessStartInfo process = new ProcessStartInfo();
            process.FileName = "IOmeter.exe";
            process.UseShellExecute = true;
            process.WorkingDirectory = @"C:\iometer";
            process.Arguments = " /c IOMeterDisk2AsSrcDisk.icf /r reboot_test_results.csv";
            logger.LogInfo("Start IOMeter");
            Process.Start(process);

            return true;
        }

        public static void CopyLogs(string sourcePath, string targetPath)
        {
            if (!Directory.Exists(targetPath))
            {
                Directory.CreateDirectory(targetPath);
            }
            foreach (var srcPath in Directory.GetFiles(sourcePath))
            {
                File.Copy(srcPath, srcPath.Replace(sourcePath, targetPath));
            }
        }

        public static int RetStatus(bool status)
        {
            string sourcePath = @"C:\tmp";
            string guid = Guid.NewGuid().ToString();
            string targetPath = @"C:\logs\" + guid;
            logger.LogInfo("Copying logs to " + targetPath + " dir");
            logger.Close();
            CopyLogs(sourcePath, targetPath);

            int res = (status) ? 0 : 1;

            return res;
        }

        public static bool OfflineSrcAndTgt()
        {
            bool status = true;
            foreach (var disk in m_srcTargetMap.Keys)
            {
                disk.Offline(readOnly: true);
                m_srcTargetMap[disk].Offline(readOnly: true);
            }

            return status;
        }

        static int Main(string[] args)
        {
            bool status = true;
            try
            {
                logger.LogInfo("=================================================");
                logger.LogInfo("Reboot Test Started");

                // Create SourceTarget Disk Map
                status = InitSrcTgtDiskMap();
                if (!status)
                {
                    logger.LogInfo("Init Src-Tgt MAP failed");
                    return RetStatus(status);
                }
                else
                {
                    logger.LogInfo("Init Src-Tgt MAP succeeded");
                }


                // Start Replication
                bool initSync = args.Length > 0;
                logger.LogInfo("Initial Sync : " + initSync);
                StartReplication(initSync);

                if (initSync)
                {
                    goto ApplyData;
                }
                else
                {
                    logger.LogInfo("Offlining SRC and Tgt");
                    status = OfflineSrcAndTgt();
                    if (!status) return RetStatus(false);

                    status = ValidateDisks();
                    if (!status) return RetStatus(false);

                    foreach (var disk in m_srcTargetMap.Keys)
                    {
                        logger.LogInfo("Online source - " +  disk.DiskNumber + " in readwrite mode");
                        disk.Offline(readOnly: false);

                        logger.LogInfo("Online target - " + m_srcTargetMap[disk].DiskNumber + " in readwrite mode");
                        m_srcTargetMap[disk].Offline(readOnly: false);
                    }

                    goto ApplyData;
                }

            ApplyData:
                WriteDataUsingIOMeter();
                Thread.Sleep(120 * 60 * 1000);
                logger.LogInfo("End IOMeter");
                RetStatus(status);
                //Process.Start("ShutDown", "-r -f -t 5");

                return 0;
            }

            catch (Exception ex)
            {
                logger.LogError("Exception occured during Reboot DI Tests", ex);

                return RetStatus(false);
            }
        }
    }
}