namespace ExchangeValidation
{
    using Microsoft.Win32;
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Net;
    using System.Runtime.InteropServices;
    using System.Security.Principal;
    using System.Text;

    internal class Exchange
    {
        public string applicationName;
        public bool bEseutil;
        public bool bUseExistingVsnap;
        public string eseutilPath;
        public string eventTag;
        public string inMageCdpCliPath;
        public string inMageConsistencyPath;
        public string inMageFailoverFolder;
        public string inMageInstallPath;
        private int NumThreads = 2;
        public string seqNum;
        public List<SgDbMap> sgdbPathDrive = new List<SgDbMap>();
        public HashSet<KeyValuePair<string, string>> sgPathChkPrefix = new HashSet<KeyValuePair<string, string>>();
        public string srcHostName;
        public HashSet<KeyValuePair<string, string>> srcLOGPath = new HashSet<KeyValuePair<string, string>>();
        public string virtualServerName;

        public Exchange()
        {
            this.Init();
        }

        public string __Function1()
        {
            StackTrace trace = new StackTrace();
            return trace.GetFrame(1).GetMethod().Name;
        }

        public bool CreateDir(string dirPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED CreateDir()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                Directory.CreateDirectory(dirPath.ToString());
                Trace.WriteLine("   " + dirPath, "Directory Path      ");
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateDir(). Message = {0}", exception.Message);
                Trace.WriteLine("Exception in CreateDir(). Message = {0}", exception.Message);
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateDir()", DateTime.Now.ToString());
            return flag;
        }

        public bool CreateFileAndRedirectOutputToFile(string fileName, Process processConsole)
        {
            Trace.WriteLine("   DEBUG   ENTERED CreateFileAndRedirectOutputToFile()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                FileStream stream = new FileStream(fileName, FileMode.OpenOrCreate, FileAccess.ReadWrite);
                StreamWriter writer = new StreamWriter(stream);
                writer.AutoFlush = true;
                StreamReader standardOutput = processConsole.StandardOutput;
                writer.Write(standardOutput.ReadToEnd());
                standardOutput.Close();
                writer.Close();
                stream.Close();
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateFileAndRedirectOutputToFile(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CreateFileAndRedirectOutputToFile(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateFileAndRedirectOutputToFile()", DateTime.Now.ToString());
            return flag;
        }

        ~Exchange()
        {
        }

        public bool FindFile(string dir, string extension, ref List<FileInfo> items, string fileString)
        {
            string str = fileString;
            string str2 = fileString;
            string str3 = "res";
            bool flag = true;
            try
            {
                DirectoryInfo info = new DirectoryInfo(dir);
                FileInfo[] files = info.GetFiles(extension);
                if (extension == "*.log")
                {
                    str = str + ".log";
                    str2 = str2 + "tmp.log";
                    foreach (FileInfo info2 in files)
                    {
                        if ((!(info2.Name == str) && !(info2.Name == str2)) && !info2.Name.Contains(str3))
                        {
                            items.Add(new FileInfo(info2.FullName));
                        }
                    }
                    return flag;
                }
                if (extension == "*.txt")
                {
                    foreach (FileInfo info3 in files)
                    {
                        if (System.IO.File.Exists(info3.FullName))
                        {
                            items.Add(new FileInfo(info3.FullName));
                        }
                    }
                    return flag;
                }
                foreach (DirectoryInfo info4 in info.GetDirectories())
                {
                    this.FindFile(info4.FullName, extension, ref items, fileString);
                }
                foreach (FileInfo info5 in info.GetFiles(extension))
                {
                    if (!(info5.Name == "tmp.edb"))
                    {
                        items.Add(new FileInfo(info5.FullName));
                    }
                }
            }
            catch (UnauthorizedAccessException exception)
            {
                //Console.WriteLine("UnauthorizedAccessException in FindFile(). Message = {0}", exception.Message);
                flag = false;
            }
            catch (Exception exception2)
            {
                Console.WriteLine("search dir = {0}", dir);
                Trace.WriteLine(string.Format("search dir = {0}", dir));
                Console.WriteLine("Exception in FindFile(). Message = {0}", exception2.Message);
                Trace.WriteLine(string.Format("Exception in FindFile(). Message = {0}", exception2.Message));
                flag = false;
            }
            return flag;
        }

        public bool FindFile1(string dir, string extension, string fileString, out List<FileInfo> items)
        {
            string str = fileString;
            string str2 = fileString;
            string str3 = "res";
            bool flag = true;
            items = new List<FileInfo>();
            try
            {
                DirectoryInfo info = new DirectoryInfo(dir);
                FileInfo[] files = info.GetFiles(extension);
                if (extension == "*.log")
                {
                    str = str + ".log";
                    str2 = str2 + "tmp.log";
                    foreach (FileInfo info2 in files)
                    {
                        if ((!(info2.Name == str) && !(info2.Name == str2)) && !info2.Name.Contains(str3))
                        {
                            items.Add(new FileInfo(info2.FullName));
                        }
                    }
                    return flag;
                }
                if (extension == "*.txt")
                {
                    foreach (FileInfo info3 in files)
                    {
                        if (System.IO.File.Exists(info3.FullName))
                        {
                            items.Add(new FileInfo(info3.FullName));
                        }
                    }
                    return flag;
                }
                foreach (DirectoryInfo info4 in info.GetDirectories())
                {
                    this.FindFile(info4.FullName, extension, ref items, fileString);
                }
                foreach (FileInfo info5 in info.GetFiles(extension))
                {
                    if (!(info5.Name == "tmp.edb"))
                    {
                        items.Add(new FileInfo(info5.FullName));
                    }
                }
            }
            catch (UnauthorizedAccessException exception)
            {
                //Console.WriteLine("UnauthorizedAccessException in FindFile1(). Message = {0}", exception.Message);
                flag = false;
            }
            catch (Exception exception2)
            {
                Console.WriteLine("search dir = {0}", dir);
                Trace.WriteLine(string.Format("search dir = {0}", dir));
                Console.WriteLine("Exception in FindFile1(). Message = {0}", exception2.Message);
                Trace.WriteLine(string.Format("Exception in FindFile1(). Message = {0}", exception2.Message));
                flag = false;
            }
            return flag;
        }

        private string GetAgentInstallPath()
        {
            string str = "";
            try
            {
                str = this.ReadRegistryEntry(@"SOFTWARE\SV Systems\VxAgent", "InstallDirectory");
                if (string.IsNullOrEmpty(str))
                {
                    str = this.ReadRegistryEntry(@"SOFTWARE\Wow6432Node\SV Systems\VxAgent", "InstallDirectory");
                    if (string.IsNullOrEmpty(str))
                    {
                        str = "";
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in GetAgentInstallPath(). Message ={0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in GetAgentInstallPath(). Message ={0}", exception.Message));
            }
            return str;
        }

        public string getAppNameFromFileName(string fileName)
        {
            try
            {
                FileInfo info = new FileInfo(fileName);
                return info.Name.Split(new char[] { '_' })[0];
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in getAppNameFromFileName(). Message = {0}", exception.Message);
                return "";
            }
        }

        public DateTime GetDateAndTime()
        {
            return DateTime.Now;
        }

        public string GetExchangeEseutilPath(string appName)
        {
            Trace.WriteLine("   DEBUG   ENTERED GetExchangeEseutilPath()", DateTime.Now.ToString());
            string regSubKey = "";
            string regPath = "";
            string strA = "";
            string str4 = "";
            try
            {
                if ((string.Compare(appName, "exchange2003", true) == 0) || (string.Compare(appName, "exchange", true) == 0))
                {
                    strA = "Exchange2003";
                    regPath = @"SOFTWARE\Microsoft\Exchange\Setup";
                    regSubKey = "ExchangeServerAdmin";
                }
                else if (string.Compare(appName, "exchange2007", true) == 0)
                {
                    regPath = @"SOFTWARE\Microsoft\Exchange\v8.0\Setup";
                    regSubKey = "MsiInstallPath";
                }
                else if (string.Compare(appName, "exchange2010", true) == 0)
                {
                    regPath = @"SOFTWARE\Microsoft\ExchangeServer\v14\Setup";
                    regSubKey = "MsiInstallPath";
                }
                str4 = this.ReadRegistryEntry(regPath, regSubKey);
                if (string.IsNullOrEmpty(str4))
                {
                    return "";
                }
                if (string.Compare(strA, "Exchange2003", true) == 0)
                {
                    str4 = str4 + @"\bin\eseutil.exe";
                }
                else
                {
                    str4 = str4 + @"bin\eseutil.exe";
                }
                if (!System.IO.File.Exists(str4))
                {
                    Console.WriteLine("\n[Error] : Eseutil.exe doesn't exists in the path = {0}", str4);
                    Trace.WriteLine("  " + str4, "Eseutil.exe Not Found");
                }
                else
                {
                    Trace.WriteLine("   " + str4, "Eseutil.exe Path    ");
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in GetExchangeEseutilPath(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in GetExchangeEseutilPath(). Message = {0}", exception.Message));
                str4 = "";
            }
            Trace.WriteLine("   DEBUG   EXITED GetExchangeEseutilPath()", DateTime.Now.ToString());
            return str4;
        }

        public void GetLogonInfo()
        {
            try
            {
                string name = WindowsIdentity.GetCurrent().Name;
                string hostName = Dns.GetHostName();
                int id = Process.GetCurrentProcess().Id;
                Console.WriteLine("Running under the user : {0}", name);
                Trace.WriteLine("Running under the user : " + name);
                Console.WriteLine("Local Machine Name is  : {0}", hostName);
                Trace.WriteLine("Local Machine Name is  : " + hostName);
                Console.WriteLine("Process ID : {0}", id);
                Trace.WriteLine("Process ID : " + id);
            }
            catch (Exception exception)
            {
                Trace.WriteLine(string.Format("Exception in GetLogonInfo(). Message = {0}", exception.Message));
            }
        }

        public int GetNumThreads()
        {
            return this.NumThreads;
        }

        public ProcessStartInfo GetProcessInit(string fileName, string procArg)
        {
            Trace.WriteLine("   DEBUG   ENTERED GetProcessInit()", DateTime.Now.ToString());
            ProcessStartInfo info = new ProcessStartInfo();
            try
            {
                info.FileName = fileName.ToString();
                info.Arguments = procArg.ToString();
                info.UseShellExecute = false;
                info.RedirectStandardOutput = true;
                info.CreateNoWindow = true;
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in GetProcessInit(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in GetProcessInit(). Message = {0}", exception.Message));
            }
            Trace.WriteLine("   DEBUG   EXITED GetProcessInit()", DateTime.Now.ToString());
            return info;
        }

        public StringBuilder GetVolPathName(string databasePath)
        {
            StringBuilder lpszVolumePathName = new StringBuilder();
            try
            {
                if (!NativeMethods.GetVolumePathName(databasePath, lpszVolumePathName, 0x100))
                {
                    Console.WriteLine("[Error] : Failed to get volume path name\n");
                    Trace.WriteLine("[Error] : Failed to get volume path name\n");
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in GetVolPathName(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in GetVolPathName(). Message = {0}", exception.Message));
            }
            return lpszVolumePathName;
        }

        private void Init()
        {
            try
            {
                this.inMageInstallPath = this.GetAgentInstallPath();
                this.inMageConsistencyPath = this.inMageInstallPath;
                this.inMageConsistencyPath = this.inMageConsistencyPath + @"\consistency";
                this.inMageCdpCliPath = this.inMageInstallPath;
                this.inMageCdpCliPath = this.inMageCdpCliPath + @"\cdpcli.exe ";
                this.inMageFailoverFolder = this.inMageInstallPath;
                this.inMageFailoverFolder = this.inMageFailoverFolder + @"\Failover";
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in Init(). Message ={0}", exception.Message);
            }
        }

        public bool IsExchangeServer(string applnName, bool bEse, string esePath, ref string exchEseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED IsExchangeServer()", DateTime.Now.ToString());
            bool flag = true;
            List<string> list = new List<string>();
            list.Add("Eseutil.exe");
            list.Add("Ese.dll");
            list.Add("Exchmem.dll");
            try
            {
                exchEseutilPath = this.GetExchangeEseutilPath(applnName);
                if (string.IsNullOrEmpty(exchEseutilPath))
                {
                    if (bEse)
                    {
                        Trace.WriteLine("[INFO] Overriding the exchange eseutil path ");
                        exchEseutilPath = esePath;
                        Console.WriteLine("\n***** Detected No Exchange server installed");
                        Console.WriteLine("      Searching for the required eseutil files in the path \"{0}\" to perform validation", exchEseutilPath);
                        if (Directory.Exists(exchEseutilPath))
                        {
                            foreach (string str in list)
                            {
                                string path = exchEseutilPath;
                                path = path + @"\" + str;
                                if (System.IO.File.Exists(path))
                                {
                                    Console.WriteLine("      File Name = {0}", str);
                                    Console.WriteLine("      File Path = {0}", path);
                                    Trace.WriteLine("   " + path, "Found ESE File       ");
                                }
                                else
                                {
                                    Console.WriteLine("      File Name = {0}", str);
                                    Console.WriteLine("      File Path = {0}", path);
                                    Trace.WriteLine("   " + path, "ESE File not found  ");
                                    Console.WriteLine("      [Error] File not found in the specified location");
                                    flag = false;
                                    break;
                                }
                            }
                            if (string.Compare(applnName, "Exchange", true) == 0)
                            {
                                string str3 = exchEseutilPath;
                                str3 = str3 + @"\Exosal.dll";
                                if (System.IO.File.Exists(str3))
                                {
                                    Console.WriteLine("      File Name = Exosal.dll");
                                    Console.WriteLine("      File Path = {0}", str3);
                                    Trace.WriteLine("   " + str3, "Found ESE File       ");
                                }
                                else
                                {
                                    Console.WriteLine("      File Name = Exosal.dll");
                                    Console.WriteLine("      File Path = {0}", str3);
                                    Trace.WriteLine("   " + str3, "ESE File not found  ");
                                    Console.WriteLine("      [Error] File not found in the specified location");
                                    flag = false;
                                }
                            }
                            if (string.Compare(applnName, "Exchange2010", true) != 0)
                            {
                                string str4 = exchEseutilPath;
                                str4 = str4 + @"\Jcb.dll";
                                if (System.IO.File.Exists(str4))
                                {
                                    Console.WriteLine("      File Name = Jcb.dll");
                                    Console.WriteLine("      File Path = {0}", str4);
                                    Trace.WriteLine("   " + str4, "Found ESE File       ");
                                }
                                else
                                {
                                    Console.WriteLine("      File Name = Jcb.dll");
                                    Console.WriteLine("      File Path = {0}", str4);
                                    Trace.WriteLine("   " + str4, "ESE File not found  ");
                                    Console.WriteLine("      [Error] File not found in the specified location");
                                    flag = false;
                                }
                            }
                            if (flag)
                            {
                                exchEseutilPath = exchEseutilPath + @"\eseutil.exe";
                            }
                        }
                        else
                        {
                            Console.WriteLine("\n[Error] Specified Directory doesn't exist to search for exchange eseutil files");
                            Trace.WriteLine("   " + exchEseutilPath, "Directory not found");
                            Console.WriteLine("      Directory = {0}", exchEseutilPath);
                            flag = false;
                        }
                    }
                    else
                    {
                        Console.WriteLine("\n[Error] : Please check whether Exchange is installed or not. If Exchange server is installed, please check the parameter(exchange version) of -app flag");
                        Trace.WriteLine("   Exchange is not installed", "Error               ");
                        Console.WriteLine("                If it is a backup server with exchange not installed, then do the following steps");
                        if (string.Compare(applnName, "Exchange", true) == 0)
                        {
                            Console.WriteLine("\t\t\t1. Create a new folder on the computer that does not have Exchange Server 2003 installed.");
                            Console.WriteLine("\t\t\t2. Copy the Eseutil.exe, Ese.dll, Jcb.dll, Exosal.dll and Exchmem.dll files from the Exchange Server 2003 installation folder.");
                            Console.WriteLine("\t\t\t3. Provide \"-eseutilpath\" flag with the path where the above files are copied and reattempt exchange validation.");
                            Console.WriteLine("\t\t\t   If eseutil files are copied in the folder \"C:\\ExchangeEseutilFiles\" then the command should look like the following example");
                            Console.WriteLine("\t\t\t      Eg., ExchangeValidation.exe -app exchange -eseutilpath \"C:\\ExchangeEseutilFiles\"");
                        }
                        else if (string.Compare(applnName, "Exchange2007", true) == 0)
                        {
                            Console.WriteLine("\t\t\t1. Create a new folder on the computer that does not have Exchange Server 2007 installed.");
                            Console.WriteLine("\t\t\t2. Copy the Eseutil.exe, Ese.dll, Jcb.dll and Exchmem.dll files from the Exchange Server 2007 installation folder.");
                            Console.WriteLine("\t\t\t3. Provide \"-eseutilpath\" flag with the path where the above files are copied and reattempt exchange validation.");
                            Console.WriteLine("\t\t\t   If eseutil files are copied in the folder \"C:\\ExchangeEseutilFiles\" then the command should look like the following example");
                            Console.WriteLine("\t\t\t      Eg., ExchangeValidation.exe -app exchange2007 -eseutilpath \"C:\\ExchangeEseutilFiles\"");
                        }
                        else if (string.Compare(applnName, "Exchange2010", true) == 0)
                        {
                            Console.WriteLine("\t\t\t1. Create a new folder on the computer that does not have Exchange Server 2010 installed.");
                            Console.WriteLine("\t\t\t2. Copy the Eseutil.exe, Ese.dll and Exchmem.dll files from the Exchange Server 2010 installation folder.");
                            Console.WriteLine("\t\t\t3. Provide \"-eseutilpath\" flag with the path where the above files are copied and reattempt exchange validation.");
                            Console.WriteLine("\t\t\t   If eseutil files are copied in the folder \"C:\\ExchangeEseutilFiles\" then the command should look like the following example");
                            Console.WriteLine("\t\t\t      Eg., ExchangeValidation.exe -app exchange2010 -eseutilpath \"C:\\ExchangeEseutilFiles\"");
                        }
                        flag = false;
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in IsExchangeServer(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in IsExchangeServer(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED IsExchangeServer()", DateTime.Now.ToString());
            return flag;
        }

        public bool ParseBatchScript(HashSet<string> commandList)
        {
            Trace.WriteLine("   DEBUG   ENTERED ParseBatchScript()", DateTime.Now.ToString());
            bool flag = true;
            bool flag2 = true;
            bool flag3 = true;
            char[] separator = new char[] { '/' };
            try
            {
                foreach (string str in commandList)
                {
                    string str2 = "";
                    string str3 = "";
                    string str4 = "";
                    string str5 = "";
                    string str6 = "";
                    string key = "";
                    foreach (string str8 in str.Split(separator))
                    {
                        string strA = str8.Split(new char[] { ':' })[0];
                        if (string.Compare(strA, "exchangeLogFilePath", true) == 0)
                        {
                            str2 = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (string.Compare(strA, "exchangeDBFilePath", true) == 0)
                        {
                            str3 = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (string.Compare(strA, "targetVolumeDB", true) == 0)
                        {
                            str4 = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (string.Compare(strA, "targetVolumeLOG", true) == 0)
                        {
                            str5 = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (flag3 && (string.Compare(strA, "eventTag", true) == 0))
                        {
                            flag3 = false;
                            this.eventTag = this.TrimData(str8, ":", 1, 1);
                        }
                        else if (flag2 && (string.Compare(strA, "seq", true) == 0))
                        {
                            flag2 = false;
                            this.seqNum = this.TrimData(str8, ":", 1, 1);
                        }
                        else if (string.Compare(strA, "chkFilePrefix", true) == 0)
                        {
                            str6 = this.TrimData(str8, ":", 1, 1);
                        }
                        else if (string.Compare(strA, "sourceLogFilePath", true) == 0)
                        {
                            key = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (string.Compare(strA, "sourceServer", true) == 0)
                        {
                            this.srcHostName = this.TrimData(str8, ":", 2, 2);
                        }
                        else if (string.Compare(strA, "virtualServer", true) == 0)
                        {
                            this.virtualServerName = this.TrimData(str8, ":", 2, 2);
                        }
                    }
                    this.srcLOGPath.Add(new KeyValuePair<string, string>(key, str2));
                    SgDbMap item = new SgDbMap();
                    item.tgtSgPath = str2;
                    item.tgtDbPath = str3;
                    item.tgtLogDrive = str5;
                    item.tgtDbDrive = str4;
                    this.sgdbPathDrive.Add(item);
                    this.sgPathChkPrefix.Add(new KeyValuePair<string, string>(str2, str6));
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ParseBatchScript(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in ParseBatchScript(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   INFO    Parsing Data from Batch script file completed", DateTime.Now.ToString());
            Trace.WriteLine("   DEBUG   EXITED ParseBatchScript()", DateTime.Now.ToString());
            return flag;
        }

        public int RandomNumber()
        {
            Random random = new Random();
            return random.Next();
        }

        public HashSet<string> ReadDataFromBatchFile(string batFilePath)
        {
            Trace.WriteLine("   DEBUG   ENTERED ReadDataFromBatchFile()", DateTime.Now.ToString());
            Console.WriteLine("\nReading Exchange configuration details from Batch script file \"{0}\"", batFilePath);
            HashSet<string> set = new HashSet<string>();
            char[] trimChars = new char[] { 'C', 'S', 'c', 'r', 'i', 'p', 't' };
            try
            {
                Trace.WriteLine("    " + batFilePath, "Batch script file   ");
                if (System.IO.File.Exists(batFilePath))
                {
                    StreamReader reader = new StreamReader(batFilePath);
                    while (!reader.EndOfStream)
                    {
                        string str = reader.ReadLine();
                        if (str.Contains("CScript"))
                        {
                            set.Add(str.TrimStart(trimChars));
                            Trace.WriteLine("    " + str, "Batch script command");
                        }
                    }
                    reader.Close();
                }
                else
                {
                    Console.WriteLine("[Error] File doesn't exists in the specified path \"{0}\"", batFilePath);
                    Trace.WriteLine("    " + batFilePath, "Batch file not found");
                    set.Clear();
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ReadDataFromBatchFile(). Message = {0}", exception.Message);
                Trace.WriteLine("   " + exception.Message, "Exception in Reading exchange configuration info from batch file");
                Console.WriteLine("\n[Error] : Failed to read data from the file : {0}", batFilePath);
            }
            Trace.WriteLine("   DEBUG   EXITED ReadDataFromBatchFile()", DateTime.Now.ToString());
            return set;
        }

        public string ReadRegistryEntry(string regPath, string regSubKey)
        {
            try
            {
                return Registry.LocalMachine.OpenSubKey(regPath).GetValue(regSubKey).ToString();
            }
            catch (Exception)
            {
                return "";
            }
        }

        public bool ReadResultFromFile(string seqNumber, out List<IntegrityResult> resultList)
        {
            Trace.WriteLine("   DEBUG   ENTERED ReadResultFromFile()", DateTime.Now.ToString());
            List<FileInfo> items = new List<FileInfo>();
            StringBuilder builder = new StringBuilder();
            string str = seqNumber;
            bool flag = true;
            resultList = new List<IntegrityResult>();
            try
            {
                builder.Append(this.inMageInstallPath);
                builder.Append(@"\Application Data\");
                builder.Append(@"ExchangeConsistency\");
                builder.Append(str);
                builder.Append(@"\");
                Trace.WriteLine("   " + builder.ToString(), "ResultFile Path     ");
                if (Directory.Exists(builder.ToString()))
                {
                    if (!this.FindFile(builder.ToString(), "*.txt", ref items, ""))
                    {
                        Console.WriteLine("[Warning] : Unable to read the result file");
                        Trace.WriteLine("    WARNING Unable to read the result file", DateTime.Now.ToString());
                        Console.WriteLine("Please verify the Exchange Integrity results from the location : {0}", builder);
                        return false;
                    }
                }
                else
                {
                    Console.WriteLine("\"{0}\" Directory doesn't exists", builder.ToString());
                    return false;
                }
                foreach (FileInfo info in items)
                {
                    string str2 = "";
                    string str3 = "";
                    string str4 = "";
                    string str5 = "";
                    try
                    {
                        StreamReader reader = new StreamReader(info.FullName);
                        Trace.WriteLine("   " + info.FullName, "Reading File        ");
                        while (!reader.EndOfStream)
                        {
                            string data = reader.ReadLine();
                            if (data.Contains("Exchange DB Integrity"))
                            {
                                str4 = this.TrimData(data, "Exchange DB Integrity", 0x18, 0);
                            }
                            else
                            {
                                if (data.Contains("Exchange Log Integrity"))
                                {
                                    str5 = this.TrimData(data, "Exchange Log Integrity", 0x19, 0);
                                    continue;
                                }
                                if (data.Contains("DB File"))
                                {
                                    str2 = this.TrimData(data, "DB File", 9, 0);
                                    continue;
                                }
                                if (data.Contains("Log Path"))
                                {
                                    str3 = this.TrimData(data, "Log Path", 10, 0);
                                }
                            }
                        }
                        reader.Close();
                        IntegrityResult item = new IntegrityResult();
                        item.logFile = str3;
                        item.logIntegrity = str5;
                        item.dbFile = str2;
                        item.dbIntegrity = str4;
                        resultList.Add(item);
                        continue;
                    }
                    catch (Exception exception)
                    {
                        Console.WriteLine("[Warning] : {0}", exception.Message);
                        Trace.WriteLine("   [Warning] " + exception.Message, DateTime.Now.ToString());
                        Console.WriteLine("Please verify the Exchange Integrity results from the file : {0}", builder);
                        flag = false;
                        continue;
                    }
                }
            }
            catch (Exception exception2)
            {
                Console.WriteLine("Exception in ReadResultFromFile(). Message = {0}", exception2.Message);
                Trace.WriteLine(string.Format("Exception in ReadResultFromFile(). Message = {0}", exception2.Message));
                resultList.Clear();
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED ReadResultFromFile()", DateTime.Now.ToString());
            return flag;
        }

        public List<KeyValuePair<string, string>> SortDatabase(HashSet<KeyValuePair<string, string>> dbFilePath)
        {
            List<FileInfo> list = new List<FileInfo>();
            List<KeyValuePair<string, string>> list2 = new List<KeyValuePair<string, string>>();
            try
            {
                string str;
                foreach (KeyValuePair<string, string> pair in dbFilePath)
                {
                    list.Add(new FileInfo(pair.Key));
                }
                HashSet<string> set = new HashSet<string>();
                foreach (FileInfo info in list)
                {
                    str = this.GetVolPathName(info.FullName).ToString();
                    set.Add(str);
                }
                int count = list.Count;
                do
                {
                    foreach (string str2 in set)
                    {
                        foreach (FileInfo info2 in list)
                        {
                            str = this.GetVolPathName(info2.FullName).ToString();
                            if (string.Compare(str, str2, true) == 0)
                            {
                                list2.Add(new KeyValuePair<string, string>(str, info2.FullName));
                                list.Remove(info2);
                                break;
                            }
                        }
                    }
                }
                while (list2.Count != count);
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in SortDatabase(). Message = {0}", exception.Message);
                list2.Clear();
            }
            return list2;
        }

        public List<FileInfo> SortFiles(List<FileInfo> fileList)
        {
            Trace.WriteLine("   DEBUG   ENTERED SortFiles()", DateTime.Now.ToString());
            List<FileInfo> list = new List<FileInfo>(fileList);
            for (int i = 0; i < list.Count; i++)
            {
                for (int j = i + 1; j < list.Count; j++)
                {
                    FileInfo info = new FileInfo(fileList[i].ToString());
                    FileInfo info2 = new FileInfo(fileList[j].ToString());
                    if (info.Length > info2.Length)
                    {
                        string fileName = list[j].ToString();
                        list[j] = list[i];
                        list[i] = new FileInfo(fileName);
                    }
                }
            }
            Trace.WriteLine("   DEBUG   EXITED SortFiles()", DateTime.Now.ToString());
            return list;
        }

        public int StartProcess(ProcessStartInfo psInfo, bool redirectOutput, string redirectFileName)
        {
            Trace.WriteLine("   DEBUG   ENTERED StartProcess()", DateTime.Now.ToString());
            Process processConsole = new Process();
            int exitCode = -1;
            try
            {
                processConsole.StartInfo = psInfo;
                processConsole.Start();
                if (redirectOutput)
                {
                    if (!this.CreateFileAndRedirectOutputToFile(redirectFileName, processConsole))
                    {
                        return -1;
                    }
                }
                else
                {
                    Console.WriteLine(processConsole.StandardOutput.ReadToEnd());
                }
                processConsole.WaitForExit();
                exitCode = processConsole.ExitCode;
                processConsole.Close();
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in StartProcess(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in StartProcess(). Message = {0}", exception.Message));
            }
            Trace.WriteLine("   DEBUG   EXITED StartProcess()", DateTime.Now.ToString());
            return exitCode;
        }

        public string TrimData(string data, string ch, int trimStartNchar, int trimEndNchar)
        {
            int index = data.IndexOf(ch);
            data = data.Substring(index + trimStartNchar);
            data = data.Substring(0, data.Length - trimEndNchar);
            return data;
        }

        public bool ValidateCmdLineArgs(string[] args)
        {
            Trace.WriteLine("   DEBUG   ENTERED ValidateCmdLineArgs()", DateTime.Now.ToString());
            bool flag = false;
            bool flag2 = false;
            bool flag3 = true;
            HashSet<string> set = new HashSet<string>();
            try
            {
                set.Add("-app");
                set.Add("-thread");
                set.Add("-eseutilpath");
                set.Add("-useexistingvsnap");
                if (args.Length == 0)
                {
                    return false;
                }
                for (int i = 0; i < args.Length; i++)
                {
                    if (string.Compare(args[i], "-eseutilpath", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            Console.WriteLine("Missing Parameter for the flag : -eseutilpath");
                            return false;
                        }
                        this.bEseutil = true;
                        this.eseutilPath = args[i];
                    }
                    else if (string.Compare(args[i], "-app", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            Console.WriteLine("Missing Parameter for the flag : -app");
                            return false;
                        }
                        flag = true;
                        this.applicationName = args[i];
                    }
                    else
                    {
                        if (string.Compare(args[i], "-thread", true) == 0)
                        {
                            i++;
                            if ((i == args.Length) || (args[i][0] == '-'))
                            {
                                Console.WriteLine("Missing paramenter for the flag : -thread");
                                return false;
                            }
                            flag2 = true;
                            try
                            {
                                this.NumThreads = Convert.ToInt32(args[i]);
                                goto Label_01EB;
                            }
                            catch (Exception exception)
                            {
                                Console.WriteLine("Exception in ValidateCmdLineArgs(). Message = {0}", exception.Message);
                                return false;
                            }
                        }
                        if (string.Compare(args[i], "-useexistingvsnap", true) == 0)
                        {
                            this.bUseExistingVsnap = true;
                        }
                        else
                        {
                            foreach (string str in set)
                            {
                                if (string.Compare(args[i], str, true) != 0)
                                {
                                    Console.WriteLine("Unsupported flag : {0}", args[i]);
                                    return false;
                                }
                            }
                        }
                    Label_01EB:;
                    }
                }
                if (flag && (args.Length < 2))
                {
                    return false;
                }
                if (flag2 && (args.Length < 4))
                {
                    return false;
                }
                if (this.bEseutil && (args.Length < 4))
                {
                    return false;
                }
                if (this.bUseExistingVsnap && (args.Length < 3))
                {
                    return false;
                }
            }
            catch (Exception exception2)
            {
                Console.WriteLine("Exception in ValidateCmdLineArgs(). Message = {0}", exception2.Message);
                Trace.WriteLine(string.Format("Exception in ValidateCmdLineArgs(). Message = {0}", exception2.Message));
                flag3 = false;
            }
            Trace.WriteLine("   DEBUG   EXITED ValidateCmdLineArgs()", DateTime.Now.ToString());
            return flag3;
        }

        public void ValidationUsage()
        {
            Console.WriteLine("\nUSAGE");
            Console.WriteLine("=====");
            Console.WriteLine("\n\tSYNTAX : ExchangeValidation.exe -app <Application Name> [-thread <Number of Threads>] [-eseutilpath <Path to eseutil files>] [-useexistingvsnap]");
            Console.WriteLine("\t\t-app             ==> Specify the application name");
            Console.WriteLine("\t\t-thread          ==> Specify number of threads to spawn for Exchange Validation");
            Console.WriteLine("\t\t                 ==> Default value set to \"2\"");
            Console.WriteLine("\t\t-eseutilpath     ==> Specify the directory path to eseutil");
            Console.WriteLine("\t\t                 ==> This flag is mandatory if Exchange is not installed on target server");
            Console.WriteLine("\t\t-useexistingvsnap==> Specify the path to existing mounted vsnap");
            Console.WriteLine("\t\t                 ==> This flag is mandatory if ExchangeValidation.exe is invoked using backup scenario");
            Console.WriteLine("\n\tExample for application Exchange2007");
            Console.Write("\n\t\tEXAMPLE 1: ExchangeValidation.exe -app Exchange2007 \n");
            Console.Write("\n\t\tEXAMPLE 2: ExchangeValidation.exe -app Exchange2007 -thread 2\n");
            Console.WriteLine("\n\tExample for application Exchange2010");
            Console.Write("\n\t\tEXAMPLE 1: ExchangeValidation.exe -app Exchange2010 -eseutilpath \"C:\\Exchange\"\n");
            Console.Write("\n\t\tEXAMPLE 2: ExchangeValidation.exe -app Exchange2010 -useexistingvsnap\n");
        }

        public bool WriteIniThreadCount(string appName, string key, string value)
        {
            Trace.WriteLine("   DEBUG   ENTERED WriteIniThreadCount()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                StringBuilder builder = new StringBuilder();
                builder.Append(this.inMageInstallPath);
                builder.Append(@"\Application Data");
                builder.Append(@"\etc");
                builder.Append(@"\drscout.conf");
                flag = NativeMethods.WritePrivateProfileString(appName, key, value, builder.ToString());
            }
            catch (Exception exception)
            {
                Trace.WriteLine("   " + exception.Message, "[Warning] : Unable to persist thread count value in drscout.conf");
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED WriteIniThreadCount()", DateTime.Now.ToString());
            return flag;
        }

        
    }
}

