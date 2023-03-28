using Microsoft.Win32;
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
    [XmlRoot("Agent_logs")]
    public class AgentLogsList
    {
        [XmlArray("copy_folders")]
        [XmlArrayItem("folder")]
        public List<CopyFolder> CopyFolders { get; set; }

        [XmlArray("copy_files")]
        [XmlArrayItem("file")]
        public List<CopyFile> CopyFiles { get; set; }

        [XmlArray("collect_events")]
        [XmlArrayItem("event")]
        public List<CollectEvent> CollectEvents { get; set; }

        [XmlArray("execute_commands")]
        [XmlArrayItem("command")]
        public List<ExecuteCommand> ExecuteCommands { get; set; }

        public AgentLogsList()
        {
            CopyFolders = new List<CopyFolder>();
            CopyFiles = new List<CopyFile>();
            CollectEvents = new List<CollectEvent>();
            ExecuteCommands = new List<ExecuteCommand>();
        }


        public void ReplacePlaceHolders()
        {
            if (CopyFolders != null)
            {
                foreach (CopyFolder folder in CopyFolders)
                {
                    if (!String.IsNullOrEmpty(folder.path) && folder.path.Contains("{"))
                    {
                        string afterReplacement = setreplaceHolders(folder.path);

                        if (!String.IsNullOrEmpty(afterReplacement))
                        {
                            folder.path = afterReplacement;
                              
                        }
                    }
                }
            }

            if (CopyFiles != null)
            {
                foreach (CopyFile file in CopyFiles)
                {
                    if (!String.IsNullOrEmpty(file.path) && file.path.Contains("{"))
                    {
                       string afterReplacement =  setreplaceHolders(file.path);

                       if (!String.IsNullOrEmpty(afterReplacement))
                       {
                           file.path = afterReplacement;

                       }

                    }
                }
            }

            if (ExecuteCommands != null)
            {
                
                foreach (ExecuteCommand ecommand in ExecuteCommands)
                {
                    if (!String.IsNullOrEmpty(ecommand.executeCommand) && ecommand.executeCommand.Contains("{"))
                    {

                       string afterReplacement =  setreplaceHolders(ecommand.executeCommand);

                       if (!String.IsNullOrEmpty(afterReplacement))
                       {
                           ecommand.executeCommand = afterReplacement;

                       }
                    }
                }
            }
        }

        public string setreplaceHolders(string value)
        {
            int startIndex = value.IndexOf('{');
            if (startIndex >= 0)
            {
                int endIndex = value.IndexOf('}');
                string placeholder = value.Substring(startIndex + 1, endIndex - startIndex - 1);
                string replacement = PathReading.GetPlaceHolderByName(placeholder);
                value = value.Replace("{" + placeholder + "}", replacement);
                return value;
            }
            return null;
        }

    }
    public class CopyFolder
    {
        [XmlAttribute("folderName")]
        public string name { get; set; }
        [XmlAttribute("path")]
        public string path { get; set; }
    }

    public class CopyFile
    {
        [XmlAttribute("fileName")]
        public string name { get; set; }
        [XmlAttribute("path")]
        public string path { get; set; }
    }

    public class CollectEvent
    {
        [XmlAttribute("eventFileName")]
        public string name { get; set; }
        [XmlAttribute("eventName")]
        public string eventName { get; set; }
    }

    public class ExecuteCommand
    {
        [XmlAttribute("outputFileName")]
        public string name { get; set; }
        [XmlAttribute("executeCommand")]
        public string executeCommand { get; set; }
        [XmlAttribute("args")]
        public string args { get; set; }
    }

    public class PathReading
    {       
       static Dictionary<string, string> placeHolders;

       static PathReading()
       {
            if (placeHolders == null)
            {
                string CSRegKey = @"SOFTWARE\\Wow6432Node\\InMage Systems\\Installed Products\\9";
                placeHolders = new Dictionary<string, string>();
                string[] VersionNumber = new string[5];

                string ProductName = Backend.GetRegPath(CSRegKey, "Product_Name");
                string Version = Backend.GetRegPath(CSRegKey, "Version");
                string systemDirectory = Environment.SystemDirectory;
                string rootDirectory = Directory.GetDirectoryRoot(systemDirectory);
                string installPath = rootDirectory + @"home\svsystems";
                
                if (!string.IsNullOrWhiteSpace(Version))
                {
                    VersionNumber = Version.Split('.');
                }

                if ((ProductName == "Caspian CX") && (int.Parse(VersionNumber[0]) >= 9))
                {
                    installPath = Backend.GetRegPath(CSRegKey, "InstallDirectory");
                    placeHolders.Add("installPath", installPath);
                    placeHolders.Add("pushInstallPath", installPath);
                }
                else
                {
                    placeHolders.Add("installPath", installPath);
                    placeHolders.Add("pushInstallPath", installPath);
                }
                placeHolders.Add("systemPath", systemDirectory);
                placeHolders.Add("rootDir", rootDirectory);

            }
       }

       public static void SetPlaceHolder(string name, string value)
       {
           if (placeHolders.ContainsKey(name))
           {
               placeHolders[name] = value;
           }
       }

        public static Dictionary<string, string> GetPlaceHolders()
        {
            return placeHolders;
        }

        public static string GetPlaceHolderByName(string name)
        {
           
            string value = null;

            if (placeHolders != null)
            {
                if(placeHolders.ContainsKey(name))
                {
                    value = placeHolders[name];
                }
            }
            return value;
        }
    }

    
    class ServerAgentLogs
    {
        static string userName, password;
        string mysqlregPath = @"SOFTWARE\Wow6432Node\MySQL AB\MySQL Server 5.5";
        private const string agentRegKey = @"SOFTWARE\Wow6432Node\InMage Systems\Installed Products\5";

        internal int CollectLogs(string folder, string serverType)
        {
            int returnFlag = 0;
            string logfile = "";
            int validateFlag;
            try
            {
                    XmlSerializer deserializer = new XmlSerializer(typeof(AgentLogsList));

                    if (serverType == "cs" || serverType == "ps")
                    {

                        string filePath = PathReading.GetPlaceHolderByName("installPath") + @"\etc\amethyst.conf";
                        string element = "CX_TYPE";
                        validateFlag = validateType(filePath, element, serverType);
                        if (validateFlag != 0)
                            return validateFlag;
                        if (serverType == "cs")
                        {
                            logfile = Directory.GetCurrentDirectory() + @"\cslogs.xml";
                        }
                        else if (serverType == "ps")
                        {
                            logfile = Directory.GetCurrentDirectory() + @"\pslogs.xml";
                        }
                    }
                    else if (serverType == "source" || serverType == "mt")
                    {
                        string installPath = Backend.GetRegPath(agentRegKey, "InstallDirectory");
                        string filePath = installPath + @"\Application Data\etc\drscout.conf";
                        string element = "Role";
                        validateFlag = validateType(filePath, element, serverType);
                        if (validateFlag != 0)
                            return validateFlag;
                        if (!String.IsNullOrEmpty(installPath))
                        {
                            string rootDirectory = Directory.GetDirectoryRoot(installPath);

                            PathReading.SetPlaceHolder("installPath", installPath);
                            PathReading.SetPlaceHolder("rootDirectory", rootDirectory);
                            if (serverType == "source")
                            {
                                logfile = Directory.GetCurrentDirectory() + @"\sourcelogs.xml";
                            }
                            else
                            {
                                logfile = Directory.GetCurrentDirectory() + @"\targetlogs.xml";
                            }
                        }
                    }

                    if (PathReading.GetPlaceHolderByName("installPath") != null)
                    {
                        TextReader reader = new StreamReader(logfile);
                        object obj = deserializer.Deserialize(reader);
                
                        AgentLogsList XmlData = (AgentLogsList)obj;
                
                        XmlData.ReplacePlaceHolders();
                        reader.Close();
                        string zipPath = folder + @".zip";
                        bool flag = true;

                
                        foreach (CopyFolder cfolder in XmlData.CopyFolders)
                        {

                            string foldername = Path.Combine(folder, cfolder.name.ToString());
                            Backend.CopyFolder(cfolder.path, foldername);
                        }

                        foreach (CopyFile cfile in XmlData.CopyFiles)
                        {

                            Backend.CopyFiles(cfile.path, folder, cfile.name);

                        }

                        foreach (CollectEvent cvent in XmlData.CollectEvents)
                        {

                            Backend.CollectEventLogs(cvent.eventName, folder, cvent.name);

                        }

                        foreach (ExecuteCommand ccommand in XmlData.ExecuteCommands)
                        {
                            if (ccommand.args != null)
                                Backend.ExecuteCommand("\"" + ccommand.executeCommand + "\"" + " " + ccommand.args, Path.Combine(folder, ccommand.name));
                            else
                                Backend.ExecuteCommand(ccommand.executeCommand, Path.Combine(folder, ccommand.name));
                        }


                        if (serverType == "cs")
                        {

                            string mysqlPath = Backend.GetRegPath(mysqlregPath, "Location");

                            if (mysqlPath != null)
                            {
                                Backend.CopyFiles(mysqlPath, folder, "my.ini");
                            }

                            string[] credentials = Backend.CredentialsMasking(folder + "\\amethyst.conf");

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

                        }

                        if (serverType == "ps")
                        {
                            string[] credentials = Backend.CredentialsMasking(folder + "\\amethyst.conf");
                            string[] files = Directory.GetFiles(PathReading.GetPlaceHolderByName("installPath"), "perf.log*", SearchOption.AllDirectories);
                            foreach (string file in files)
                            {
                                try
                                {
                                    string fileName = Path.GetFileName(file);
                                    string srcDir = Path.GetDirectoryName(file);
                                    string dir = srcDir.Replace(PathReading.GetPlaceHolderByName("installPath"), "");
                                    string destDir = folder + dir;
                                    Backend.CopyFiles(srcDir, destDir, fileName);
                                }
                                catch (Exception ex)
                                {
                                    Trace.Write(ex.Message);
                                }
                            }
                        }
                        if (serverType == "mt")
                        {
                            Backend.ExecuteCommand("\"\"" + PathReading.GetPlaceHolderByName("installPath") + "\\cdpcli\" --showreplicationpairs | find \"Catalogue\"\"", Path.Combine(folder, "retentiondriveinfo" + ".txt"));
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
                                    catch (Exception ex)
                                    {
                                        Trace.WriteLine(ex.Message);
                                    }

                                }
                            }
                        }

                        if (Directory.Exists(Path.Combine(folder, "LocaleMetaData")))
                        {  
                            Directory.Delete(Path.Combine(folder, "LocaleMetaData"), true);
                        }
                    ZipFile.CreateFromDirectory(folder, zipPath);

                    Directory.Delete(folder, flag);
                }
            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);  
            }
            return returnFlag;

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

        private static int validateType(string filePath, string element, string sType)
        {
            string role = null;
            if (File.Exists(filePath))
            {
                var lines = File.ReadAllLines(filePath);
                foreach (string line in lines)
                {
                    if (line != null && (line.StartsWith(element)))
                    {
                        string[] lineEditsplit = line.Split('=');

                        if (lineEditsplit.Length == 2)
                        {
                            if (sType == "cs" || sType == "ps")
                            {
                                role = lineEditsplit[1].Replace("\"", "");
                                role = role.Trim();
                            }
                            else
                            {
                                role = lineEditsplit[1];
                            }

                        }

                    }
                }

                if (!string.IsNullOrEmpty(role))
                {
                    if (sType == "cs" && role == "2")
                    {
                        Trace.Write("This is not a " + sType);
                        return Constants.NotConfigurationServer;
                    }
                      
                    if (sType == "ps" && role == "1")
                    {
                        Trace.Write("This is not a " + sType);
                        return Constants.NotProcessServer;
                    }
                        
                    if (sType == "source" && role != "Agent")
                    {
                        Trace.Write("This is not a " + sType);
                        return Constants.NotSource;
                    }
                        
                    if (sType == "mt" && role != "MasterTarget")
                    {
                        Trace.Write("This is not a " + sType);
                        return Constants.NotMastertarget;
                    }
                        
                }
                else
                {
                    return Constants.InvalidServerTpe;
                }

            }
            else
            {
                Trace.Write("This is not a " + sType);
                if (sType == "cs")
                    return Constants.NotConfigurationServer;
                if (sType == "ps")
                    return Constants.NotProcessServer;
                if (sType == "source")
                    return Constants.NotSource;
                if (sType == "mt")
                    return Constants.NotMastertarget;
            }
            return Constants.SuccessReturnCode;
        }

   }
}
