namespace ASRSetupFramework
{
    using System;
    using System.Collections;
    using System.Collections.Generic;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Diagnostics.CodeAnalysis;
    using System.Globalization;
    using System.IO;
    using System.Management;
    using System.Security;
    using System.Text;
    using Microsoft.Win32;

    /// <summary>
    /// Report data setup class.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1600:ElementsMustBeDocumented",
        Justification = "Constants for use in Setup and Registration")]
    public static class SetupReportData
    {
        // Is DW installed?
        private const string WatsonInstalledRegKey32 = "Software\\Microsoft\\PCHealth\\ErrorReporting\\DW\\Installed";
        private const string WatsonInstalledRegKey64 = "Software\\WOW6432Node\\Microsoft\\PCHealth\\ErrorReporting\\DW\\Installed";

        // dw20.exe location
        private const string Dw20RegKeyName = "DW0200";

        // Watson group policy key
        private const string WatsonPolicyRegKey = "Software\\Policies\\Microsoft\\PCHealth\\ErrorReporting\\DW";

        private const int DwTimeOut = 300000;   // Time out for watson process to exit

        private const string ManifestFileName = "vNext2011SDManifest.txt";
        private const string ManifestVersion = "Version=131072";
        private const string ManifestEventType = "EventType=Simple";
        private const string ManifestP1Value = "P1=vNext2011";
        private const string ManifestFilesToKeep = "FilesToKeep=";
        private const string ManifestAppName = "General_AppName=Microsoft System Center Codename VMM V.Next";
        private const string ManifestQueuedEventDesc = "Queued_EventDescription=vNext Setup Data";

        private const string ManifestFileToKeepSeparator = "|";

        private static List<string> listOfLogFiles = new List<string>();

        /// <summary>
        /// Packages and send setup report data.
        /// </summary>
        [SecurityCritical]
        public static void PackageAndSendSetupReportData()
        {
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.SendSetupLogsToWatson))
            {
                ArrayList logs = new ArrayList();

                // Ok... go through and add any logs we need and add them to the array.
                if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.MachineStatusFile))
                {
                    logs.Add(PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.MachineStatusFile));
                }
                ////if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.GeneralLogFile))
                ////{
                ////    logs.Add(PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.GeneralLogFile));
                ////}
                ////if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.ErrorLogFile))
                ////{
                ////    logs.Add(PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ErrorLogFile));
                ////}

                if (logs.Count > 0)
                {
                    // Ok... we have some logs.... send them to Watson
                    SendSetupLogFiles(logs);
                }
            }
        }

        /// <summary>
        /// Logs the array list in data file.
        /// </summary>
        /// <param name="data">The data to be logged.</param>
        /// <returns>string to be logged.</returns>
        public static string LogArrayListInDataFile(ArrayList data)
        {
            string temporaryFileName = ReturnSafeTempFilename();

            using (StreamWriter file = File.AppendText(temporaryFileName))
            {
                foreach (string dataLine in data)
                {
                    file.WriteLine(dataLine);
                }

                file.Flush();
            }

            return temporaryFileName;
        }

        /// <summary>
        /// Merges multiple files into one single target file.
        /// </summary>
        /// <param name="targetFilePath">Path to target file.</param>
        public static void MergeFiles(string targetFilePath)
        {
            try
            {
                using (var targetWriter = new StreamWriter(targetFilePath, true, Encoding.Default))
                {
                    listOfLogFiles.ForEach(file =>
                        {
                            targetWriter.WriteLine("=============> START " + file + "<=============");
                            if (File.Exists(file))
                            {
                                targetWriter.WriteLine(File.ReadAllText(file));
                            }
                            targetWriter.WriteLine("=============> END " + file + "<=============");
                        });
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception at MergeFiles: {0}",
                    ex);
            }
        }

        /// <summary>
        /// Adds log file to the list of files be merged at the end.
        /// </summary>
        /// <param name="logFile">Log file to add.</param>
        public static void AddLogFilePath(string logFile)
        {
            listOfLogFiles.Add(logFile);
        }

        /// <summary>
        /// Sets the log file path.
        /// </summary>
        /// <returns> the FQN of a same temp file.</returns>
        public static string ReturnSafeTempFilename()
        {
            string folderPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "vNext");
            if (!Directory.Exists(folderPath))
            {
                Directory.CreateDirectory(folderPath);
            }

            // Add the log to the FullLogList
            return Path.Combine(folderPath, Path.GetRandomFileName());
        }

        /// <summary>
        /// Get machine data.
        /// </summary>
        /// <returns>Machine data as arraylist.</returns>
        public static ArrayList GetMachineData()
        {
            ArrayList data = new ArrayList();
            data.Add("<?xml version=\"1.0\" encoding=\"utf-8\" ?>");
            data.Add("<SetupData>");

            data.Add("<SetupRunTime>");
            if (PropertyBagDictionary.Instance.PropertyExists(PropertyBagConstants.SetupStartTime))
            {
                TimeSpan timeValue = DateTime.Now - PropertyBagDictionary.Instance.GetProperty<DateTime>(PropertyBagConstants.SetupStartTime);
                data.Add(string.Format(CultureInfo.InvariantCulture, @"<RunTime>{0}</RunTime>", timeValue.ToString()));
            }

            data.Add("</SetupRunTime>");

            data.Add("<SystemInformation>");
            data.Add(string.Format(CultureInfo.InvariantCulture, "<OSVersion>{0}</OSVersion>", System.Environment.OSVersion.ToString()));
            data.Add(string.Format(CultureInfo.InvariantCulture, "<dotNetVersion>{0}</dotNetVersion>", System.Environment.Version.ToString()));
            data.Add(string.Format(CultureInfo.InvariantCulture, "<ProcessorCount>{0}</ProcessorCount>", System.Environment.ProcessorCount.ToString(CultureInfo.InvariantCulture)));
            data.Add(string.Format(CultureInfo.InvariantCulture, "<TickCount>{0}</TickCount>", System.Environment.TickCount.ToString(CultureInfo.InvariantCulture)));
            data.Add("</SystemInformation>");

            data.Add("<Hardware>");
            ManagementObjectSearcher searcher = new ManagementObjectSearcher("Select * from Win32_OperatingSystem");
            searcher = new ManagementObjectSearcher("SELECT * FROM Win32_ComputerSystem");
            foreach (ManagementObject systemObject in searcher.Get())
            {
                data.Add(string.Format(CultureInfo.InvariantCulture, "<{0}>{1}</{0}>", "manufacturer", systemObject["manufacturer"].ToString()));
                data.Add(string.Format(CultureInfo.InvariantCulture, "<{0}>{1}</{0}>", "model", systemObject["model"].ToString()));
                data.Add(string.Format(CultureInfo.InvariantCulture, "<{0}>{1}</{0}>", "systemtype", systemObject["systemtype"].ToString()));
                data.Add(string.Format(CultureInfo.InvariantCulture, "<{0}>{1}</{0}>", "totalphysicalmemory", systemObject["totalphysicalmemory"].ToString()));
            }

            data.Add("</Hardware>");
            
            data.Add("<LocalDriveFreeSpace>");
            searcher = new ManagementObjectSearcher("SELECT FreeSpace,Name from Win32_LogicalDisk where DriveType=3");
            foreach (ManagementObject systemObject in searcher.Get())
            {
                try
                {
                    data.Add(string.Format(CultureInfo.InvariantCulture, "<{0}>{1}</{0}>", systemObject["Name"].ToString().TrimEnd(Path.VolumeSeparatorChar), systemObject["FreeSpace"].ToString()));
                }
                catch (Exception)
                {
                    // ignore the exception
                }
            }

            data.Add("</LocalDriveFreeSpace>");
            data.Add("</SetupData>");
            return data;
        }

        /// <summary>
        /// Generates the manifest.
        /// </summary>
        /// <param name="manifestFilePath">The manifest file path.</param>
        /// <param name="logList">The log list.</param>
        private static void GenerateManifest(string manifestFilePath, ArrayList logList)
        {
            StringBuilder manifestContents = new StringBuilder();

            manifestContents.AppendLine(); // First line is an empty line

            manifestContents.AppendLine(SetupReportData.ManifestAppName);

            // Write the version
            manifestContents.AppendLine(SetupReportData.ManifestVersion);
            manifestContents.AppendLine(SetupReportData.ManifestQueuedEventDesc);
            manifestContents.AppendLine(SetupReportData.ManifestEventType);
            manifestContents.AppendLine(SetupReportData.ManifestP1Value);

            // Files that need to be uploaded
            manifestContents.Append(SetupReportData.ManifestFilesToKeep);

            bool addSeparator = false;
            for (int i = 0; i < logList.Count; i++)
            {
                if (addSeparator)
                {
                    manifestContents.Append(SetupReportData.ManifestFileToKeepSeparator);
                }
                else
                {
                    addSeparator = true;
                }

                manifestContents.Append(logList[i]);
            }

            // End of files to keep
            manifestContents.AppendLine();

            ////manifestContents.AppendLine(SetupReportData.ManifestUIFlags);

            try
            {
                using (StreamWriter fs = new StreamWriter(manifestFilePath, false, System.Text.Encoding.Unicode))
                {
                    fs.Write(manifestContents.ToString());
                }
            }
            catch (Exception)
            {
                throw;
            }

            return;
        }

        /// <summary>
        /// Gets the manifest file path.
        /// </summary>
        /// <returns>Manifest path.</returns>
        private static string GetManifestFilePath()
        {
            StringBuilder manifestFilePath = new StringBuilder();

            try
            {
                manifestFilePath.Append(Path.GetTempPath());
            }
            catch (SecurityException)
            {
                throw;
            }

            manifestFilePath.Append(Path.DirectorySeparatorChar);

            manifestFilePath.Append(SetupReportData.ManifestFileName);

            return manifestFilePath.ToString();
        }

        /// <summary>
        /// Gets the path to watson executable.
        /// </summary>
        /// <returns>Watson executable path</returns>
        private static string GetPathToWatsonExecutable()
        {
            string dwLocation = string.Empty;

            try
            {
                // Find Watson
                RegistryKey dwKey = Registry.LocalMachine.OpenSubKey(SetupReportData.WatsonInstalledRegKey32);
                if (dwKey == null)
                {
                    dwKey = Registry.LocalMachine.OpenSubKey(SetupReportData.WatsonInstalledRegKey64);
                }

                dwLocation = (string)dwKey.GetValue(SetupReportData.Dw20RegKeyName);
            }
            catch (SecurityException)
            {
                throw;
            }

            return dwLocation;
        }

        /// <summary>
        /// Sends the setup log files.
        /// </summary>
        /// <param name="logFiles">The log files.</param>
        [SecurityCritical]
        private static void SendSetupLogFiles(ArrayList logFiles)
        {
            // Check if there is anything to send
            if (logFiles.Count == 0)
            {
                ////Console.WriteLine(string.Format(Resources.NoReportsToSend));
                return;
            }

            string manifestFilePath = GetManifestFilePath();

            // Generate the manifest file
            GenerateManifest(manifestFilePath, logFiles);

            // Get Path the watson executable
            string dwLocation = GetPathToWatsonExecutable();

            string dwArgs = "-d " + manifestFilePath;

            Process dwExe = null;

            try
            {
                dwExe = Process.Start(dwLocation, dwArgs);
            }
            catch (FileNotFoundException e)
            {
                Trc.LogException(LogLevel.Always, "SetupReportData.SendSetupLogFiles()", e);
            }
            catch (Win32Exception e)
            {
                Trc.LogException(LogLevel.Always, "SetupReportData.SendSetupLogFiles()", e);
            }

            // Time out for process
            if (dwExe != null)
            {
                if (dwExe.WaitForExit(SetupReportData.DwTimeOut))
                {
                    // Check the return code and log error if neccessary
                    if (dwExe.ExitCode != 0)
                    {
                        Trc.Log(LogLevel.Error, "Watson Failed with error: {0}", dwExe.ExitCode.ToString(CultureInfo.InvariantCulture));
                        //// new OperationalDataReportingException(errMesg);
                    }
                    else
                    {
                        // Successfully exited
                        // Generate Summary Event
                        StringBuilder summary = new StringBuilder();

                        for (int i = 0; i < logFiles.Count; i++)
                        {
                            summary.Append(Environment.NewLine);
                            summary.Append(logFiles[i]);
                        }

                        Trc.Log(LogLevel.Debug, "Watson summary: ", summary.ToString());
                    }
                }
            }
        }

        /// <summary>
        /// Append the log file content.
        /// </summary>
        /// <param name="logFileName">The log file name.</param>
        public static void AppendLogFileContent(string logFileName)
        { 
            if (File.Exists(logFileName))
            {
                using (StreamReader sr = new StreamReader("" + logFileName + ""))
                {
                    string LogContent = sr.ReadToEnd();
                    Trc.Log(LogLevel.Always, "" + logFileName + " output start here.");
                    Trc.Log(LogLevel.Always, "--------------------------------------");
                    Trc.Log(LogLevel.Always, LogContent);
                    Trc.Log(LogLevel.Always, "" + logFileName + " output end here.");
                    Trc.Log(LogLevel.Always, "------------------------------------");
                }
            }
            else
            {
                Trc.Log(LogLevel.Error, "" + logFileName + " file doesn't exist.");
            }
        }
    }
}
