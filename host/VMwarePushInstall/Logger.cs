//---------------------------------------------------------------
//  <copyright file="Logger.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Logger module.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VMwareClient
{
    public class Logger
    {
        /// <summary>
        /// Log file to which log is to be written.
        /// </summary>
        private static string verboseLogFile = String.Empty;

        /// <summary>
        /// Sets the private variable verboseLogFile.
        /// </summary>
        /// <param name="filePath">File to be set for logging.</param>
        public static void SetLogFile(string filePath)
        {
            verboseLogFile = filePath;
        }

        /// <summary>
        /// Appends a verbose log to log file.
        /// </summary>
        /// <param name="data">Log string to be appended to the log file.</param>
        public static void LogVerbose(string data)
        {
            if (String.IsNullOrEmpty(verboseLogFile))
            {
                return;
            }

            DateTime currentTime = DateTime.Now;

            String logString = currentTime.ToString() + " VERBOSE " + data;

            System.IO.File.AppendAllText(verboseLogFile, logString);
        }

        /// <summary>
        /// Appends a warning log to log file.
        /// </summary>
        /// <param name="data">Log string to be appended to the log file.</param>
        public static void LogWarning(string data)
        {
            if (String.IsNullOrEmpty(verboseLogFile))
            {
                return;
            }

            DateTime currentTime = DateTime.Now;

            String logString = currentTime.ToString() + " WARNING " + data;

            System.IO.File.AppendAllText(verboseLogFile, logString);
        }

        /// <summary>
        /// Appends a error log to log file.
        /// </summary>
        /// <param name="data">Log string to be appended to the log file.</param>
        public static void LogError(string data)
        {
            if (String.IsNullOrEmpty(verboseLogFile))
            {
                return;
            }

            DateTime currentTime = DateTime.Now;

            String logString = currentTime.ToString() + " ERROR " + data;

            System.IO.File.AppendAllText(verboseLogFile, logString);
        }
    }
}
