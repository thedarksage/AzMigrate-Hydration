//---------------------------------------------------------------
//  <copyright file="ConfVariables.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements variables to be read from config file.
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
    public class ConfVariables
    {
        /// <summary>
        /// Default log file path to be read from config file.
        /// </summary>
        public static string VerboseLogFilePath = @"C:\ProgramData\ASR\home\svsystems\pushinstallsvc\VMwareLogs.log";

        /// <summary>
        /// Interval between retries for a failed operation.
        /// </summary>
        public static TimeSpan RetryInterval = TimeSpan.FromSeconds(10);

        /// <summary>
        /// Number of retries for retryable failures.
        /// </summary>
        public static int Retries = 5;

        /// <summary>
        /// Timeout for Invoking script on VM.
        /// </summary>
        public static TimeSpan ScriptInvocationTimeout = TimeSpan.FromMinutes(15);

        /// <summary>
        /// Timeout for pushing a file to VM.
        /// </summary>
        public static TimeSpan CopyFileTimeout = TimeSpan.FromMinutes(5);

        /// <summary>
        /// Timeout for fetching a file from VM.
        /// </summary>
        public static TimeSpan FetchFileTimeout = TimeSpan.FromMinutes(1);
    }
}
