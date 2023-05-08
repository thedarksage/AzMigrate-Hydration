using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.Eventing.Reader;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Compression;


namespace LogsCollection
{
    class Source
    {
        private const string MTRegKey = @"SOFTWARE\Wow6432Node\InMage Systems\Installed Products\5";

        internal void SLogs(string folder)
        {
            string installPath = Backend.GetRegPath(MTRegKey, "InstallDirectory");
            string rootDirectory = Directory.GetDirectoryRoot(installPath);
            
            try
            {

                if (installPath != null)
                {
                    string dpLogs = Path.Combine(folder, "DPLogs");
                    string transport = Path.Combine(folder, "Transport_Logs");
                    string applicationPolicyLogs = Path.Combine(folder, "AppPolicy_Logs");
                    string failoverDataLogs = Path.Combine(folder, CSMT.data.ToString());
                    string transportLog = Path.Combine(folder, "Transport_Logs");
                    string zipPath = folder + @".zip";
                    bool flag = true;
                    
                    Backend.CopyFiles(installPath, folder, "*.log");
                    Backend.CopyFiles(installPath, folder, "s2.xfer*");
                   
                    Backend.CopyFiles(rootDirectory, folder, "SMR_InstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "SMR_UnInstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "UA_InstallLogFile.log");
                    Backend.CopyFiles(rootDirectory, folder, "UA_UnInstallLogFile.log");

                    Backend.CopyFiles(Path.Combine(installPath, "Application Data".ToString()), folder, "*.log");

                    Backend.CopyFiles(Path.Combine(Path.Combine(installPath, "Application Data".ToString()), CSMT.etc.ToString()), folder, "*.log");
                    Backend.CopyFiles(Path.Combine(Path.Combine(installPath, "Application Data".ToString()), CSMT.ApplicationPolicyLogs.ToString()), applicationPolicyLogs, "*.log");
                    Backend.CopyFiles(Path.Combine(installPath, CSMT.FileRep.ToString()), folder, "*.log");

                    
                    Backend.CopyFolder(Path.Combine(Path.Combine(installPath, CSMT.Failover.ToString()), CSMT.data.ToString()), failoverDataLogs);
                    
                    string command = "\"" + installPath + "\\drvutil\" --ps --dm";
                    Backend.ExecuteCommand(command, Path.Combine(folder, "driverinfo" + ".txt"));

                    string cdCommand = "\"" + installPath + "\\drvutil\" --cd";
                    Backend.ExecuteCommand(cdCommand, Path.Combine(folder, "driverstatinfo" + ".txt"));

                    string timeCommand = "\"" + installPath + "\\drvutil\" --getglobaltimestamp";
                    Backend.ExecuteCommand(cdCommand, Path.Combine(folder, "globaltimestamp" + ".txt"));
                   

                    if (!Directory.Exists(dpLogs))
                    {
                        Directory.CreateDirectory(dpLogs);
                        Backend.CopyFolder(Path.Combine(installPath, CSMT.vxlogs.ToString()), dpLogs);
                    }
                   
                    if (!Directory.Exists(transportLog))
                    {
                        Directory.CreateDirectory(transportLog);
                        Backend.CopyFiles(Path.Combine(installPath, CSMT.transport.ToString()), transportLog, "*.conf");
                    }
                    ZipFile.CreateFromDirectory(folder, zipPath);
                    Directory.Delete(folder, flag);

                }

            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);
            }

         }

     }
}
