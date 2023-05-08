//---------------------------------------------------------------
//  <copyright file="VMwarePushInstallStatus.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements push installation status to be written to telemetry.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Newtonsoft.Json;

namespace VMwareClient
{
    class VMwarePushInstallTelemetry
    {
        public bool Status;

        public string ComponentErrorCode;

        public string ErrorCode;

        public string ErrorMessage;

        /// <summary>
        /// Marks the job successful.
        /// </summary>
        public void RecordOperationSucceeded()
        {
            Logger.LogVerbose("Operation successful.\n");
            Status = true;
            ErrorCode = String.Empty;
            ComponentErrorCode = String.Empty;
            ErrorMessage = String.Empty;
        }

        /// <summary>
        /// Marks the job failed with the exception mentioned.
        /// </summary>
        /// <param name="ex">Exception with which the job failed.</param>
        public void RecordOperationFailed(VMwarePushInstallException ex)
        {
            Logger.LogError(String.Format("Operation failed with error : {0}.\n", ex.errorMessage));
            Status = false;
            ErrorCode = ex.errorCode.ToString();
            ComponentErrorCode = ex.componentErrorCode;
            ErrorMessage = ex.errorMessage;
        }

        /// <summary>
        /// Marks the job failed with the exception mentioned.
        /// </summary>
        /// <param name="ex">Exception with which the job failed.</param>
        public void RecordOperationFailed(Exception ex)
        {
            Logger.LogError(String.Format("Operation failed with error : {0}.\n", ex.ToString()));
            Status = false;
            ErrorCode = ex.GetType().ToString();
            ComponentErrorCode = String.Empty;
            ErrorMessage = ex.ToString();
        }

        /// <summary>
        /// Writes the operation status to telemetry file.
        /// </summary>
        /// <param name="logFilePath"></param>
        public void writeOperationStatusToTelemetryFile(
            string logFilePath)
        {
            if (!String.IsNullOrEmpty(logFilePath))
            {
                string json = JsonConvert.SerializeObject(this);
                Logger.LogVerbose(String.Format("Operation status : {0}\n", json));
                Logger.LogVerbose("Writing operation status to telemetry file..\n");
                File.WriteAllText(logFilePath, json);
            }
        }
    }
}
