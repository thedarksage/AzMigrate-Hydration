//---------------------------------------------------------------
//  <copyright file="Summary.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Wrapper class for summary file operations.
//  </summary>
//
//  History:     28-Aug-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace ASRSetupFramework
{
    public class SummaryFileWrapper
    {
        #region Public methods
        /// <summary>
        /// Intialize OpLogger with the type of installation and fabirc adapter.
        /// </summary>
        /// <param name="componentName">Component name.</param>
        /// <param name="componentVersion">Component version.</param>
        /// <param name="installationType">Type of the installation.</param>
        /// <param name="fabricAdapter">Fabric adapater type for the installation.</param>
        public static void StartSummary(
            string componentName,
            Version componentVersion,
            InstallType installationType,
            AdapterType fabricAdapter)
        {
            try
            {
                OpLogger.Initialize(
                    componentName,
                    componentVersion,
                    installationType,
                    fabricAdapter);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception at StartSummary - {0}", 
                    ex);
            }
        }

        /// <summary>
        /// Record the executed operation
        /// </summary>
        /// <param name="scenario">Type of the scenario executed.</param>
        /// <param name="operationName">Name of the operation</param>
        /// <param name="status">Status of the operation</param>
        /// <param name="errorMessage">Error message if failed</param>
        /// <param name="possibleCauses">Possible caused on failure</param>
        /// <param name="recommemdationMessage">Recommended action on failure</param>
        /// <param name="exceptionMessage">Exception message to be recorded if any</param>
        /// <param name="errorCode">Error code of the operation.</param>
        /// <param name="errorCodeName">Friendly name of the error code of the operation.</param>
        public static void RecordOperation(
            Scenario scenario,
            OpName operationName,
            OpResult status,
            int errorCode = 0,
            string errorMessage = null,
            string possibleCauses = null,
            string recommemdationMessage = null,
            string exceptionMessage = null,
            string errorCodeName = null)
        {
            try
            {
                if (OpLogger.Instance != null)
                {
                    OpLogger.Instance.RecordOperationResult(
                        scenario,
                        operationName,
                        status,
                        errorMessage,
                        possibleCauses,
                        recommemdationMessage,
                        exceptionMessage,
                        errorCode,
                        errorCodeName);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception at RecordOperation - {0}",
                    ex);
            }
        }

        /// <summary>
        /// End summary with the final result.
        /// </summary>
        /// <param name="finalResult">Final result of the installation.</param>
        public static void EndSummary(
            OpResult finalResult,
            Guid machineIdentifier,
            string osType,
            string osDistro,
            string hostName,
            string assetTag,
            string installScenario,
            string installRole,
            double totalMemoryInGB,
            double freeMemoryInGB,
            string existingVersion,
            bool silentInstall,
            int exitCode,
            string invokerType,
            string biosId,
            string drscoutHostId,
            string runId)
        {
            try
            {
                OpLogger.Instance.End(
                    finalResult,
                    machineIdentifier,
                    osType,
                    osDistro,
                    hostName,
                    assetTag,
                    installScenario,
                    installRole,
                    totalMemoryInGB,
                    freeMemoryInGB,
                    existingVersion,
                    silentInstall,
                    exitCode,
                    invokerType,
                    biosId,
                    drscoutHostId,
                    runId);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception at EndSummary - {0}",
                    ex);
            }
        }

        /// <summary>
        /// Disposes the OpLogger instance.
        /// </summary>
        public static void DisposeSummary()
        {
            try
            {
                OpLogger.Instance.Dispose();
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception at DisposeSummary - {0}",
                    ex);
            }
        }

        #endregion Public methods

    }
}
