//---------------------------------------------------------------
//  <copyright file="Main.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Main module invoked via exe.
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

namespace VMwareClient
{
    public class Program
    {
        public static string VMwarePushInstallConfigPath = @"C:\ProgramData\ASR\home\svsystems\pushinstallsvc\VMwarePushInstall.conf";

        public static void GetConfigVariables()
        {
            Dictionary<string, string> confVariables = new Dictionary<string, string>();

            try
            {
                if (!File.Exists(VMwarePushInstallConfigPath))
                {
                    return;
                }

                string[] lines = File.ReadAllLines(VMwarePushInstallConfigPath);

                foreach (string line in lines)
                {
                    string[] keyValuePair = line.Split('=');
                    if (keyValuePair != null && keyValuePair.Length == 2)
                    {
                        confVariables.Add(keyValuePair[0], keyValuePair[1]);
                    }
                }

                if (confVariables.Count == 0)
                {
                    return;
                }

                string logFile = String.Empty;
                string retries = String.Empty;
                string retryInterval = String.Empty;
                string scriptInvocationTimeout = String.Empty;
                string pushFileTimeout = String.Empty;
                string fetchFileTimeout = String.Empty;

                if (confVariables.TryGetValue("VerboseLogFile", out logFile))
                {
                    Logger.SetLogFile(logFile);
                }
                else
                {
                    Logger.SetLogFile(@"C:\ProgramData\ASR\home\svsystems\pushinstallsvc\VmWarePushInstall.log");
                }

                if (confVariables.TryGetValue("Retries", out retries))
                {
                    ConfVariables.Retries = Convert.ToInt32(retries);
                }

                if (confVariables.TryGetValue("RetryIntervalInSeconds", out retryInterval))
                {
                    ConfVariables.RetryInterval = 
                        TimeSpan.FromSeconds(Convert.ToInt32(retryInterval));
                }

                if (confVariables.TryGetValue("ScriptInvocationTimeoutInMinutes", out scriptInvocationTimeout))
                {
                    ConfVariables.ScriptInvocationTimeout =
                        TimeSpan.FromMinutes(Convert.ToInt32(scriptInvocationTimeout));
                }

                if (confVariables.TryGetValue("PushFileTimeoutInMinutes", out pushFileTimeout))
                {
                    ConfVariables.CopyFileTimeout =
                        TimeSpan.FromMinutes(Convert.ToInt32(pushFileTimeout));
                }

                if (confVariables.TryGetValue("FetchFileTimeoutInMinutes", out fetchFileTimeout))
                {
                    ConfVariables.FetchFileTimeout =
                        TimeSpan.FromMinutes(Convert.ToInt32(fetchFileTimeout));
                }
            }
            catch (Exception ex)
            {
                Logger.LogWarning(
                    String.Format(
                        "Reading config variables failed with error {0}",
                        ex.ToString()));
            }
        }

        public static int Main(string[] args)
        {
            int retVal = 0;
            string opType = null;
            string verboseLogFilePath = null;
            string telemetryLogFilePath = null;
            VMwarePushInstallTelemetry pushInstallTelemetry = new VMwarePushInstallTelemetry();

            try
            {
                Dictionary<string, string> commandArguments = new Dictionary<string, string>();

                string key = String.Empty;
                string value = String.Empty;

                foreach (string currentArg in args)
                {
                    if (currentArg.StartsWith("--"))
                    {
                        if (!String.IsNullOrEmpty(key))
                        {
                            // Two keys found consecutively. Value is missing for a key.
                            new VMwarePushInstallException(
                                VMwarePushInstallErrorCode.ReadingCommandArgumentsFailed.ToString(),
                                VMwarePushInstallErrorCode.ReadingCommandArgumentsFailed,
                                String.Format("Value is missing for key: {0}.", key));
                        }

                        key = currentArg;
                    }
                    else
                    {
                        if (String.IsNullOrEmpty(key) ||
                            !String.IsNullOrEmpty(value))
                        {
                            // Key is not present before value. Received two values consecutively.
                            new VMwarePushInstallException(
                                VMwarePushInstallErrorCode.ReadingCommandArgumentsFailed.ToString(),
                                VMwarePushInstallErrorCode.ReadingCommandArgumentsFailed,
                                String.Format("Key is missing for value: {0}.", currentArg));
                        }

                        value = currentArg;
                    }

                    if (!String.IsNullOrEmpty(key) && !String.IsNullOrEmpty(value))
                    {
                        commandArguments.Add(key, value);
                        key = String.Empty;
                        value = String.Empty;
                    }
                }

                GetConfigVariables();

                if (commandArguments.TryGetValue("--logfilepath", out verboseLogFilePath))
                {
                    Logger.SetLogFile(verboseLogFilePath);
                }

                commandArguments.TryGetValue("--summarylogfilepath", out telemetryLogFilePath);

                if (!commandArguments.TryGetValue("--operation", out opType))
                {
                    Logger.LogError("Did not find operation type in inputs. Aborting..\n");
                    throw new VMwarePushInstallException(
                        VMwarePushInstallErrorCode.OperationTypeNotFoundInInputs.ToString(),
                        VMwarePushInstallErrorCode.OperationTypeNotFoundInInputs,
                        "Operation type not found in input args.");
                }

                IOperation op = null;
                switch (opType)
                {
                    case "fetchosname":
                        op = new GetVmOsNameOperation(commandArguments);
                        Logger.LogVerbose("Operation type : fetchosname\n");
                        break;
                    case "installmobilityagent":
                        op = new InstallMobilityServiceOperation(commandArguments);
                        Logger.LogVerbose("Operation type : installmobilityagent\n");
                        break;
                    default:
                        Logger.LogError(
                            String.Format(
                            "Invalid operation type {0} received.\n",
                            opType));
                        break;
                }

                if (op == null)
                {
                    Logger.LogError(
                        "Could not start operation as operation type is not found. Aborting..\n");
                    throw new VMwarePushInstallException(
                        VMwarePushInstallErrorCode.OperationTypeInvalid.ToString(),
                        VMwarePushInstallErrorCode.OperationTypeInvalid,
                        "Invalid operation type.");
                }



                Logger.LogVerbose(
                    String.Format(
                    "Operation to be executed :\n {0}\n",
                    op.ToString()));

                Logger.LogVerbose("Starting operation execution..\n");
                op.Execute();

                Logger.LogVerbose("Recording operation success..\n");
                pushInstallTelemetry.RecordOperationSucceeded();
            }
            catch (VMwarePushInstallException vmex)
            {
                pushInstallTelemetry.RecordOperationFailed(vmex);
                retVal = -1;
            }
            catch (Exception ex)
            {
                pushInstallTelemetry.RecordOperationFailed(ex);
                retVal = -1;
            }

            Logger.LogVerbose("Operation execution completed. Writing telemetry file.\n");

            try
            {
                pushInstallTelemetry.writeOperationStatusToTelemetryFile(telemetryLogFilePath);
            }
            catch (Exception ex)
            {
                Logger.LogError(
                    String.Format(
                    "Writing telemetry file failed with error : {0}.\n",
                    ex.ToString()));
            }

            return retVal;
        }
    }
}
