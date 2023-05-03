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
    class PServer
    {

        private const string pushRegPath = @"SOFTWARE\Wow6432Node\SV Systems\PushInstaller";
        string systemInfo = "SystemInfo";

        internal void PsLogs(string folder)
        {
            
            try
            {
                string systemDirectory = Environment.SystemDirectory;
                string rootDirectory = Directory.GetDirectoryRoot(systemDirectory);
                string installPath = rootDirectory + @"home\svsystems";

                string systemPath = Path.Combine(systemDirectory, CSMT.config.ToString());
                
                if (!Directory.Exists(folder))
                {
                    Directory.CreateDirectory(folder);
                }
                string psvar = Path.Combine(folder, CSMT.var.ToString());
                string psetc = Path.Combine(folder, CSMT.etc.ToString());
                string psPush = Path.Combine(folder, CSMT.pushinstall.ToString());
                string psDraInstall = Path.Combine(folder, CSMT.DRAInstallationlogs.ToString());
                string psTemp = Path.Combine(folder, CSMT.Temp.ToString());
                string psvcon = Path.Combine(folder, CSMT.vcon.ToString());
                string transport = Path.Combine(folder, "Transport_Logs");
                string phpPath = Path.Combine(folder, rootDirectory, "thirdparty", CSMT.php5nts.ToString());
                string zipPath = folder + @".zip";
                bool flag = true;


                string pushPath = Backend.GetRegPath(pushRegPath, CSMT.InstallDirectory.ToString());
                if (pushPath != null)
                {
                    Backend.CopyFiles(pushPath, psPush, "*.log");
                    Backend.CopyFiles(pushPath, psPush, "*.conf");
                }
                Backend.CopyFiles(Path.Combine(installPath, CSMT.etc.ToString()), psetc, "*.conf");

                Backend.CopyFiles(Path.Combine(rootDirectory, CSMT.Temp.ToString()), psTemp, "*.log");
                Backend.CopyFiles(phpPath, folder, "php.ini");
                Backend.CopyFiles(Path.Combine(installPath, CSMT.etc.ToString()), psetc, "version");
                Backend.CopyFiles(installPath, folder, "patch.log");

                //Copy var folder
                Backend.CopyFolder(Path.Combine(installPath, CSMT.var.ToString()), psvar);

                Backend.ExecuteCommand(systemInfo, Path.Combine(folder, systemInfo + ".txt"));
                Backend.CollectEventLogs("System", folder, "system.evtx");
                Backend.CollectEventLogs("Application", folder, "Application.evtx");
                Backend.CopyFiles(rootDirectory, folder, "CX_TP_InstallLogFile.log");
                Backend.CopyFiles(rootDirectory, folder, "CX_InstallLogFile.log");

                Backend.CopyFiles(systemPath, folder, "*.log");

                string[] credentials = Backend.CredentialsMasking(folder + "\\etc\\amethyst.conf");

                string[] files = Directory.GetFiles(installPath, "perf.log*", SearchOption.AllDirectories);
                foreach (string file in files)
                {
                    try
                    {
                        string fileName = Path.GetFileName(file);
                        string srcDir = Path.GetDirectoryName(file);
                        string dir = srcDir.Replace(installPath, "");
                        string destDir = folder + dir;
                        Backend.CopyFiles(srcDir, destDir, fileName);
                    }
                    catch(Exception ex)
                    {
                        Trace.Write(ex.Message);
                    }
                }
                
                if (!Directory.Exists(transport))
                {
                    Directory.CreateDirectory(transport);
                    Backend.CopyFolder(Path.Combine(Path.Combine(installPath, CSMT.transport.ToString()), CSMT.log.ToString()), transport);
                    Backend.CopyFiles(Path.Combine(installPath, "transport".ToString()), transport, "*.conf");
                }
                
                ZipFile.CreateFromDirectory(folder, zipPath);
                Directory.Delete(folder, flag);

            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);
            }

       }
               
    }
}
