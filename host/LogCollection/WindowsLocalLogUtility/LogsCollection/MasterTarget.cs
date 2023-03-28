using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Compression;
using System.Diagnostics;

namespace LogsCollection
{
    class MasterTarget
    {
        private const string MTRegKey = @"SOFTWARE\Wow6432Node\InMage Systems\Installed Products\5";
        string systemInfo = "SystemInfo";

        internal void MTarget(string folder)
        {
            string installPath = Backend.GetRegPath(MTRegKey, "InstallDirectory");
            string rootDirectory = Directory.GetDirectoryRoot(installPath);
            try
            {
                if(installPath != null)
                {
                    string dpLogs = Path.Combine(folder, "DPLogs");
                    string transportLog = Path.Combine(folder, "Transport_Logs");
                    string failoverDataLogs = Path.Combine(folder, CSMT.data.ToString());
                    string zipPath = folder + @".zip";
                    bool flag = true;
                    
                    Backend.CopyFiles(installPath, folder, "*.log");
                    Backend.CopyFiles(Path.Combine(installPath, CSMT.FileRep.ToString()),folder, "*.log");
                    
                    if (!Directory.Exists(dpLogs))
                    {
                        Directory.CreateDirectory(dpLogs);
                        Backend.CopyFolder(Path.Combine(installPath, CSMT.vxlogs.ToString()), dpLogs);
                    }

                    Backend.CopyFolder(Path.Combine(Path.Combine(installPath, CSMT.Failover.ToString()), CSMT.data.ToString()), failoverDataLogs);
                   
                    if (!Directory.Exists(transportLog))
                    {
                        Directory.CreateDirectory(transportLog);
                        Backend.CopyFolder(Path.Combine(Path.Combine(installPath, CSMT.transport.ToString()), CSMT.log.ToString()), transportLog);
                        Backend.CopyFiles(Path.Combine(installPath, CSMT.transport.ToString()), transportLog, "*.conf");
                    }

                    Backend.ExecuteCommand(systemInfo, Path.Combine(folder, systemInfo + ".txt"));
                    Backend.CollectEventLogs("System", folder, "system.evtx");
                    Backend.CollectEventLogs("Application", folder, "Application.evtx");

                    Backend.CopyFiles(rootDirectory, folder, "SMR_InstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "SMR_UnInstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "UA_InstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "UA_UnInstallLogFile.log");

                    Directory.Delete(Path.Combine(folder, "LocaleMetaData"), true);

                    
                    Backend.ExecuteCommand("\"\"" + installPath + "\\cdpcli\" --showreplicationpairs | find \"Catalogue\"\"", Path.Combine(folder, "retentiondriveinfo" + ".txt"));
                    string retentionLogPath = folder + @"\retentiondriveinfo.txt";
                    if (File.Exists(retentionLogPath))
                    {
                        string[] retentioncontents = System.IO.File.ReadAllLines(retentionLogPath);
                        
                        foreach (string retentioncontent in retentioncontents)
                        {
                            try
                            {

                                if (!string.IsNullOrEmpty(retentioncontent))
                                {
                                    string[] retwntionPath = retentioncontent.Split(new string[] { " " }, StringSplitOptions.RemoveEmptyEntries);
                                    string retentionPathString = "";

                                    if (retwntionPath.Length == 3)
                                        retentionPathString = retwntionPath[2];
                                                                        
                                        try
                                        {
                                            string fileName = Path.GetFileName(retentionPathString);
                                            string srcDir = Path.GetDirectoryName(retentionPathString);
                                            string dir = srcDir.Replace(":", "");
                                            string destDir = folder + "\\" + dir;
                                            Backend.CopyFiles(srcDir, destDir, fileName);
                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.Write(ex.Message);
                                        }
                                 }
                            }
                            catch(Exception ex)
                            {
                                Trace.WriteLine(ex.Message);
                            }
                           
                        }
                    }
                    
                    ZipFile.CreateFromDirectory(folder, zipPath);
                    Directory.Delete(folder, flag);
                 }
            }
            catch (Exception ex)
            {

                Trace.Write("MTarget: " + ex.Message);
            }
            
        }
    }
}
