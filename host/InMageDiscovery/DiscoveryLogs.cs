//---------------------------------------------------------------
//  <copyright file="DiscoveryLogs.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Tool used for periodic discovery triggered by Process Server in InMage.
//  </summary>
//
//  History:     30-Oct-2015   Prmyaka   Created
// -----------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Xml.Serialization;
using VMware.Client;

namespace InMageDiscovery
{
    /// <summary>
    /// Creating log files and maintaining
    /// </summary>
    public class DiscoveryLogs
    {
        /// <summary>
        /// Log files path.
        /// </summary>
        private static string logsPath;

        /// <summary>
        /// Initializes a new instance of the DiscoveryLogs class.
        /// </summary>
        /// <param name="fileName">Debug log file name.</param>
        /// <param name="errorLogFile">Error log file name.</param>
        public DiscoveryLogs(string fileName, string errorLogFile)
        {
            this.SetLogPaths();
            this.CreateLogFile(fileName, errorLogFile);
        }

        /// <summary>
        /// Gets the debug log writer.
        /// </summary>
        public static StreamWriter LogWriter { get; private set; }

        /// <summary>
        /// Gets the error log writer.
        /// </summary>
        public static StreamWriter ErrorLogWriter { get; private set; }

        /// <summary>
        /// Closing the log files and merging to main log files.
        /// </summary>
        /// <param name="fileName">Debug log file name.</param>
        /// <param name="errorFile">Error log file name.</param>
        public void CloseLogs(string fileName, string errorFile)
        {
            LogWriter.WriteLine(
                "--------------------------------------------------");
            LogWriter.Close();
            ErrorLogWriter.Close();
            string mainLogFile = logsPath + "\\vContinuum_Scripts.log";
            string errorLogFile = logsPath + "\\vContinuum_ESX.log";
            fileName = logsPath + "\\" + fileName;
            errorFile = logsPath + "\\" + errorFile;

            try
            {
                Stream mainStream = File.Open(mainLogFile, FileMode.Append);
                Stream tempStream = File.OpenRead(fileName);
                tempStream.CopyTo(mainStream);
                mainStream.Close();
                tempStream.Close();
                File.Delete(fileName);

                if (File.Exists(errorLogFile))
                {
                    File.Delete(errorLogFile);
                }

                File.Move(errorFile, errorLogFile);
                this.LogRotation(mainLogFile);
            }
            catch (Exception exception)
            {
                Console.WriteLine("Failed to merge the log files " + exception.Message);
            }
        }

        /// <summary>
        /// Write into log file.
        /// </summary>
        /// <param name="type">Trace event type.</param>
        /// <param name="message">Log message to write.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public void LogMessage(TraceEventType type, string message, params object[] args)
        {
            string timeStamp = DateTime.Now.ToString();
            message = string.Format(message, args);
            string logMessage;
            switch (type)
            {
                case TraceEventType.Critical:
                    logMessage = timeStamp + ": CRITICAL :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Error:
                    logMessage = timeStamp + ": ERROR :: " + message;
                    LogWriter.WriteLine(logMessage);
                    ErrorLogWriter.WriteLine(message);
                    break;

                case TraceEventType.Warning:
                    logMessage = timeStamp + ": WARNING :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Information:
                    logMessage = timeStamp + ": INFO :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Verbose:
                    logMessage = timeStamp + ": DEBUG :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                default:
                    logMessage = timeStamp + ": EXCEPTION :: " + message;
                    LogWriter.WriteLine(logMessage);
                    LogWriter.WriteLine(
                        string.Format("Unexpected error of type {0}", type));
                    ErrorLogWriter.WriteLine(message);
                    ErrorLogWriter.WriteLine(
                       string.Format("Unexpected error of type {0}", type));
                    break;
            }
        }

        /// <summary>
        /// Write the infrastructure information into xml file.
        /// </summary>
        /// <param name="esxInfo">infrastructure view.</param>
        /// <param name="serverIP">vCenter server Ipaddress/hostname.</param>
        public void WriteIntoXml(InfrastructureView esxInfo, string serverIP)
        {
            // ipv6 need to handle, currently not accepting ipv6
            string[] splitIp = serverIP.Split(':');
            string xmlFileName = logsPath + "\\" + splitIp[0] + "_discovery.xml";
            XmlSerializer serializer = new XmlSerializer(typeof(InfrastructureView));
            TextWriter textWriter = new StreamWriter(xmlFileName);
            serializer.Serialize(textWriter, esxInfo);
            textWriter.Close();
        }

        /// <summary>
        /// Setting log files.
        /// </summary>
        private void SetLogPaths()
        {
            string currentDirectory = Path.GetDirectoryName(
                Assembly.GetExecutingAssembly().Location);
            string svsDirectory = Directory.GetParent(currentDirectory).FullName;
            logsPath = svsDirectory + "\\var\\vcon\\logs";
        }

        /// <summary>
        /// Creating log file.
        /// </summary>
        /// <param name="fileName">Debug log file.</param>
        /// <param name="errorLogFile">Error log file.</param>
        private void CreateLogFile(string fileName, string errorLogFile)
        {
            if (!Directory.Exists(logsPath))
            {
                Directory.CreateDirectory(logsPath);
            }

            string logFile = logsPath + "\\" + fileName;
            LogWriter = File.CreateText(logFile);

            string errorTempLogFile = logsPath + "\\" + errorLogFile;
            ErrorLogWriter = File.CreateText(errorTempLogFile);

            LogWriter.AutoFlush = true;

            ErrorLogWriter.AutoFlush = true;

            LogWriter.WriteLine(
                "--------------------------------------------------");
        }

        /// <summary>
        /// Log rotation, if log file is more than 10MB, archiving the file.
        /// If archived file is more than 30 days, deleting the archived file.
        /// </summary>
        /// <param name="fileName">Log file name.</param>
        private void LogRotation(string fileName)
        {
            long fileSize = new System.IO.FileInfo(fileName).Length;

            // if file size is less than 10mb
            if (fileSize < 10485760)
            {
                return;
            }

            string zipFolder = logsPath + "\\" + "vContinuum_Scripts.zip";

            // keeping logs for 30days
            int daysLimit = 86400 * 30;

            string timeStamp =
                ((int)DateTime.Now.Subtract(new DateTime(1970, 1, 1)).
                TotalSeconds).ToString();

            try
            {
                if (!File.Exists(zipFolder))
                {
                    FileStream zipFileCreation = File.Create(zipFolder);
                    zipFileCreation.Close();
                }

                using (FileStream zipToOpen = new FileStream(zipFolder, FileMode.Open))
                {
                    using (ZipArchive archive =
                        new ZipArchive(zipToOpen, ZipArchiveMode.Update))
                    {
                        for (int i = 0; i < archive.Entries.Count; i++)
                        {
                            string zipMember = archive.Entries[i].FullName;
                            string[] splitFileName = zipMember.Split('.');
                            int fileTimestamp = int.Parse(splitFileName[0]);
                            int currentTimeStamp = int.Parse(timeStamp);
                            if ((currentTimeStamp - fileTimestamp) > daysLimit)
                            {
                                archive.GetEntry(zipMember).Delete();
                            }
                        }

                        archive.CreateEntryFromFile(fileName, timeStamp + ".log");
                    }

                    zipToOpen.Close();
                }

                File.Delete(fileName);
            }
            catch (Exception e)
            {
                Console.WriteLine("Failed to archive the logs due to error: " + e.Message);
            }
        }
    }
}
