using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.Eventing.Reader;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using System.Xml;
using System.IO.Compression;

namespace LogsCollection
{

    public class SystemDetails
    {
        public string mysqlRegpath { get; set; }
        public string draInstallPath { get; set; }
        public string asrEvtVwr { get; set; }
        public string systemInfo { get; set; }
        public string draInstallLogs { get; set; }
        public string inetpubLogs { get; set; }
    }

    class CServer
    {

        static string userName, password;
        internal void CsLogs(string folder)
        {
            try
            {
                string systemDirectory = Environment.SystemDirectory;
                string rootDirectory = Directory.GetDirectoryRoot(systemDirectory);
                string installPath = rootDirectory + @"home\svsystems";
                                              
                string systemLogs = Path.Combine(Path.Combine(systemDirectory, CSMT.LogFiles.ToString()), CSMT.HTTPERR.ToString());
                string systemPath = Path.Combine(systemDirectory, CSMT.config.ToString());
                
                XmlSerializer deserializer = new XmlSerializer(typeof(SystemDetails));
                string logfile = Directory.GetCurrentDirectory() + @"\cslogs.xml";
                XmlReader reader = XmlReader.Create(logfile);
                
                object obj = deserializer.Deserialize(reader);
                SystemDetails XmlData = (SystemDetails)obj;
                reader.Close();
                
                string csVar = Path.Combine(folder, CSMT.var.ToString());
                string csEtc = Path.Combine(folder, CSMT.etc.ToString());
                string csPush = Path.Combine(folder, CSMT.pushinstall.ToString());
                string csDraInstall = Path.Combine(folder, CSMT.DRAInstallationlogs.ToString());
                string csTemp = Path.Combine(folder, CSMT.Temp.ToString());
                string csInetPub = Path.Combine(folder, CSMT.inetpub.ToString());
                string csHttpError = Path.Combine(folder, CSMT.HTTPERR.ToString());
                string csVcon = Path.Combine(folder, CSMT.vcon.ToString());
                string phpPath = Path.Combine(folder, rootDirectory, "thirdparty", CSMT.php5nts.ToString());


                string transportLog = Path.Combine(folder, "Transport_Logs");
                string zipPath = folder +  @".zip";
                bool flag = true;

                string mysqlPath = Backend.GetRegPath(XmlData.mysqlRegpath, CSMT.Location.ToString());

                if (mysqlPath != null)
                {
                    Backend.CopyFiles(mysqlPath, folder, "my.ini");
                }

                Backend.CopyFiles(Path.Combine(installPath, CSMT.etc.ToString()), csEtc, "*.conf");
                Backend.CopyFiles(Path.Combine(rootDirectory, CSMT.Temp.ToString()), csTemp, "*.log");
                Backend.CopyFiles(phpPath, folder, "php.ini");
                Backend.CopyFiles(Path.Combine(installPath, CSMT.etc.ToString()), csEtc, "version");
                Backend.CopyFiles(installPath, folder, "patch.log");

                Backend.CopyFiles(rootDirectory, folder, "CX_TP_InstallLogFile.log");
                Backend.CopyFiles(rootDirectory, folder, "CX_InstallLogFile.log");

                Backend.CopyFiles(systemPath, folder, "*.log");

                //Copy var folder
                Backend.CopyFolder(Path.Combine(installPath, CSMT.var.ToString()), csVar);
                Backend.CopyFolder(Path.Combine(installPath, CSMT.vcon.ToString()), csVcon);
                Backend.CopyFolder(XmlData.inetpubLogs, csInetPub);
                Backend.CopyFolder(systemLogs, csHttpError);
                
                ////copy dra installations logs
                Backend.CopyFolder(XmlData.draInstallLogs, csDraInstall);
                Backend.ExecuteCommand(XmlData.systemInfo, Path.Combine(folder, XmlData.systemInfo + ".txt"));
                Backend.CollectEventLogs("System", folder, "system.evtx");
                Backend.CollectEventLogs("Application", folder, "Application.evtx");

                //Backend.CopyFiles(Path.Combine(databaseData, CSMT.data.ToString()), folder, "*.*");
                
                if (!Directory.Exists(transportLog))
                {
                    Directory.CreateDirectory(transportLog);
                    Backend.CopyFolder(Path.Combine(Path.Combine(installPath, CSMT.transport.ToString()), CSMT.log.ToString()), transportLog);
                    Backend.CopyFiles(Path.Combine(installPath, CSMT.transport.ToString()), transportLog, "*.conf");
                }

                if (Directory.Exists(XmlData.draInstallPath))
                {
                    Backend.CopyFiles(XmlData.draInstallPath, folder, "*.log");
                    Backend.CollectEventLogs(XmlData.asrEvtVwr, folder, "asr.evtx");
                    Directory.Delete(Path.Combine(folder, "LocaleMetaData"), true);
                }

                string[] credentials = Backend.CredentialsMasking(folder + "\\etc\\amethyst.conf");

                userName = credentials[0];
                
                password = credentials[1];
                
                if (DatabaseDump(userName, password, folder))
                {
                    Trace.Write("\n Database dump collected.");
                }
                else
                {
                    Trace.Write("\n Failed to collect database dump.");
                }

                if (DatabaseTableHealth(userName, password, folder))
                {
                    Trace.Write("\n Database tables health collected.");
                }
                else
                {
                    Trace.Write("\n Failed to collect tables health.");
                }

                ZipFile.CreateFromDirectory(folder, zipPath);
                Directory.Delete(folder, flag);
            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);
            }

        }

        internal static bool DatabaseDump(string username, string password, string folder)
        {
            //CS database dump collections
            string dumpfile = folder + "\\dbdump.sql";
            bool flag = false;

            try
            {
                using (StreamWriter sw = new StreamWriter(dumpfile, true))
                {
                    ProcessStartInfo proc = new ProcessStartInfo();
                    proc.FileName = "mysqldump";
                    proc.RedirectStandardInput = false;
                    proc.RedirectStandardOutput = true;
                    proc.RedirectStandardError = true;
                    proc.Arguments = "-u " + username + " -p" + password + " --databases svsdb1";
                    proc.UseShellExecute = false;
                    Process p = Process.Start(proc);
                    string res = p.StandardOutput.ReadToEnd();
                    sw.WriteLine(res);
                    sw.Flush();
                    sw.Close();
                    p.WaitForExit();
                    if (p.ExitCode == 0)
                        flag = true;
                }
            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);
            }
            return flag;
        }

        internal static bool DatabaseTableHealth(string username, string password, string folder)
        {
            //CS database table health
            string dumpfile = folder + "\\dbtablehealth.txt";
            bool Retflag = false;

            try
            {
                using (StreamWriter sw = new StreamWriter(dumpfile, true))
                {
                    ProcessStartInfo proc = new ProcessStartInfo();
                    proc.FileName = "mysqlcheck";
                    proc.RedirectStandardInput = false;
                    proc.RedirectStandardOutput = true;
                    proc.RedirectStandardError = true;
                    proc.Arguments = "-u " + username + " -p" + password + " --databases svsdb1";
                    proc.UseShellExecute = false;
                    Process p = Process.Start(proc);
                    string res = p.StandardOutput.ReadToEnd();
                    sw.WriteLine(res);
                    sw.Flush();
                    sw.Close();
                    p.WaitForExit();
                    if (p.ExitCode == 0)
                        Retflag = true;
                }

            }
            catch (Exception ex)
            {

                Trace.Write(ex.Message);
            }
            return Retflag;
        }

    }
}
