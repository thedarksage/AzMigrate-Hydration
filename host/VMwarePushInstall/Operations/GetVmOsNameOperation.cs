//---------------------------------------------------------------
//  <copyright file="GetVmOsNameOperation.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements getting os name for a VM via VMware Vim SDK.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using InMage.APIHelpers;

namespace VMwareClient
{
    class GetVmOsNameOperation : IOperation
    {
        /// <summary>
        /// IP of the CS machine to get the accounts.
        /// </summary>
        public string csIP = String.Empty;

        /// <summary>
        /// Port of the CS machine to connect.
        /// </summary>
        public int csPort = 0;

        /// <summary>
        /// Specifies to use https/http for communication with CS.
        /// </summary>
        public bool useHttpsForCS = true;

        /// <summary>
        /// IP of the vcenter to connect.
        /// </summary>
        public string vcenterIP = String.Empty;

        /// <summary>
        /// vcenter creds account id in CS.
        /// </summary>
        public string vcenterAccountId = String.Empty;

        /// <summary>
        /// IP of the VM to perform the operations.
        /// </summary>
        public string[] vmIPList = new string[] { };

        /// <summary>
        /// Name of the VM.
        /// </summary>
        public string vmName = String.Empty;

        /// <summary>
        /// VM creds account id in CS.
        /// </summary>
        public string vmAccountId = String.Empty;

        /// <summary>
        /// Paths of files to be copied to CS.
        /// </summary>
        public List<string> filesToCopy = new List<string>();

        /// <summary>
        /// Destination in VM to copy the files.
        /// </summary>
        public string copyFileDestination = String.Empty;

        /// <summary>
        /// Scripts to be invoked on VM.
        /// </summary>
        public string[] scriptsToInvoke = new string[] { };

        /// <summary>
        /// File which contains the output of the script invocation.
        /// </summary>
        public string scriptOutputFile = String.Empty;

        /// <summary>
        /// Stack type (RCM or CS). Default value would be CS.
        /// </summary>
        public string stackType = Constants.PushInstallJobConstants.StackType.Cs;

        /// <summary>
        /// Credential store creds path for RCM stack.
        /// </summary>
        public string credentialStoreFilePath = String.Empty;

        /// <summary>
        /// Source machine bios id to be verified for RCM stack.
        /// </summary>
        public string sourceMachineBiosId = string.Empty;

        /// <summary>
        /// Initializes new vm os name operation.
        /// </summary>
        /// <param name="inputs">Input command line arguments.</param>
        public GetVmOsNameOperation(Dictionary<string, string> inputs)
        {
            ReadOperationInputs(inputs);
        }

        /// <summary>
        /// Reads operation inputs from arguments.
        /// </summary>
        /// <param name="args">Key value inputs.</param>
        public void ReadOperationInputs(Dictionary<string, string> inputs)
        {
            Logger.LogVerbose("Reading arguments ..\n");
            foreach (var key in inputs.Keys)
            {
                switch (key)
                {
                    case "--csip":
                        csIP = inputs[key];
                        break;
                    case "--csport":
                        csPort = Convert.ToInt32(inputs[key]);
                        break;
                    case "--usehttps":
                        if (inputs[key] == "false")
                        {
                            useHttpsForCS = false;
                        }
                        else
                        {
                            useHttpsForCS = true;
                        }
                        break;
                    case "--vcenterip":
                        vcenterIP = inputs[key];
                        
                        break;
                    case "--vcenteraccount":
                        vcenterAccountId = inputs[key];
                        break;
                    case "--vmipaddresses":
                        vmIPList = inputs[key].Split(',');
                        break;
                    case "--vmname":
                        vmName = inputs[key];
                        break;
                    case "--vmaccount":
                        vmAccountId = inputs[key];
                        break;
                    case "--filestocopy":
                        string[] filesList = inputs[key].Split(';');
                        if (filesList != null && filesList.Length > 0)
                        {
                            foreach (var file in filesList)
                            {
                                filesToCopy.Add(file.Replace("/", "\\"));
                            }
                        }
                        break;
                    case "--folderonremotemachine":
                        copyFileDestination = inputs[key];
                        break;
                    case "--cmdtorun":
                        scriptsToInvoke = inputs[key].Split(';');
                        break;
                    case "--outputfilepath":
                        scriptOutputFile = inputs[key];
                        if (scriptOutputFile != null)
                        {
                            scriptOutputFile = scriptOutputFile.Replace("/", "\\");
                        }
                        break;
                    case "--stacktype":
                        stackType = inputs[key];
                        break;
                    case "--credentialstorefilepath":
                        credentialStoreFilePath = inputs[key];
                        break;
                    case "--sourcemachinebiosid":
                        sourceMachineBiosId = inputs[key];
                        break;
                    default:
                        Logger.LogWarning(
                            String.Format(
                            "Skipping {0} command line argument.\n",
                            key));
                        break;
                }
            }

            Logger.LogVerbose("Completed reading arguments.\n");
        }

        /// <summary>
        /// Validates required operation inputs.
        /// </summary>
        public void ValidateOperationInputs()
        {
            string errorMessage = String.Empty;
            Logger.LogVerbose("Validating inputs..\n");

            bool isValidationSuccessful = true;

            if (String.IsNullOrEmpty(csIP))
            {
                isValidationSuccessful = false;
                errorMessage = "CS IP is missing.";
            }

            if (csPort == 0)
            {
                isValidationSuccessful = false;
                errorMessage = "CS port is missing or not valid.";
            }

            if (String.IsNullOrEmpty(vcenterAccountId))
            {
                isValidationSuccessful = false;
                errorMessage = "VCenter account id is missing.";
            }

            if (String.IsNullOrEmpty(vcenterIP))
            {
                isValidationSuccessful = false;
                errorMessage = "VCenter IP is missing.";
            }

            if (String.IsNullOrEmpty(vmAccountId))
            {
                isValidationSuccessful = false;
                errorMessage = "VM account id is missing.";
            }

            if (String.IsNullOrEmpty(vmName))
            {
                isValidationSuccessful = false;
                errorMessage = "VM name is missing.";
            }

            if (vmIPList == null ||
                vmIPList.Length == 0)
            {
                isValidationSuccessful = false;
                errorMessage = "VM IPs are missing.";
            }

            if (scriptsToInvoke == null ||
                scriptsToInvoke.Length == 0)
            {
                isValidationSuccessful = false;
                errorMessage = "Scripts to be invoked are missing.";
            }

            if (String.IsNullOrEmpty(scriptOutputFile))
            {
                isValidationSuccessful = false;
                errorMessage = "Scripts output file is missing.";
            }

            if (filesToCopy == null ||
                filesToCopy.Count == 0)
            {
                isValidationSuccessful = false;
                errorMessage = "File copy info is missing.";
            }

            if (String.IsNullOrEmpty(copyFileDestination))
            {
                isValidationSuccessful = false;
                errorMessage = "Destination folder on remote machine to copy files is mising.";
            }

            if (!isValidationSuccessful)
            {
                Logger.LogError(
                    String.Format(
                        "Validating operation inputs failed with error: {0}.\n",
                        errorMessage));
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.ValidatingOperationInputsFailed.ToString(),
                    VMwarePushInstallErrorCode.ValidatingOperationInputsFailed,
                    errorMessage);
            }

            Logger.LogVerbose("Validated inputs successfully.\n");
        }

        /// <summary>
        /// Execute the operation.
        /// </summary>
        public void Execute()
        {
            ValidateOperationInputs();

            Logger.LogVerbose("Getting account list from CS..\n");

            List<LogonAccount> logonAccounts = null;
            LogonAccount vcenterAccount = null;
            LogonAccount vmAccount = null;
            try
            {
                if (stackType == Constants.PushInstallJobConstants.StackType.Rcm)
                {
                    logonAccounts =
                        CredentialHelper.getAccountsFromCredStore(
                            credentialStoreFilePath);
                }
                else
                {
                    logonAccounts =
                        CredentialHelper.getCSAccounts(
                            csIP,
                            csPort,
                            useHttpsForCS);
                }
            }
            catch (Exception ex)
            {
                Logger.LogError(
                    String.Format(
                    "Getting cs accounts failed with error {0}.\n",
                    ex.ToString()));
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.GetAccountsFromCSFailed.ToString(),
                    VMwarePushInstallErrorCode.GetAccountsFromCSFailed,
                    ex.ToString());
            }

            Logger.LogVerbose("Finding vcenter account..\n");

            try
            {
                vcenterAccount = logonAccounts.Single(acc => acc.AccountId == vcenterAccountId);
            }
            catch (Exception ex)
            {
                Logger.LogError(
                    String.Format(
                    "Getting vcenter creds failed with error {0}.\n",
                    ex.ToString()));
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.GetVcenterAccountFromCSFailed.ToString(),
                    VMwarePushInstallErrorCode.GetVcenterAccountFromCSFailed,
                    ex.ToString());
            }

            Logger.LogVerbose("Finding vm account..\n");

            try
            {
                vmAccount = logonAccounts.Single(acc => acc.AccountId == vmAccountId);
            }
            catch (Exception ex)
            {
                Logger.LogError(
                    String.Format(
                    "Getting vm creds failed with error {0}.\n",
                    ex.ToString()));
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.GetVmAccountFromCSFailed.ToString(),
                    VMwarePushInstallErrorCode.GetVmAccountFromCSFailed,
                    ex.ToString());
            }

            VMwareClient client = new VMwareClient(
                vcenterIP,
                vcenterAccount.GetUsername(),
                vcenterAccount.Password,
                vmName,
                vmIPList,
                vmAccount.GetUsername(),
                vmAccount.Password);

            Logger.LogVerbose("Connecting to vcenter..\n");

            client.ConnectToVCenter();

            Logger.LogVerbose("Finding vm in vcenter..\n");

            VMware.Interfaces.Models.IVirtualMachine vm = client.FindVmInVCenter();

            if (vm.Summary.Config.GuestFullName.Contains("Windows"))
            {
                System.IO.File.WriteAllText(scriptOutputFile, "Win");
                return;
            }

            bool done = false;
            int attempts = 0;
            while (!done)
            {
                try
                {
                    Logger.LogVerbose("Copying files to the remote machine.\n");

                    bool createDirectoryOnRemoteMachine = true;
                    foreach (string fileToCopy in filesToCopy)
                    {
                        string fileName = String.Empty;
                        int index = fileToCopy.LastIndexOf("\\");
                        fileName = fileToCopy.Substring(index + 1);

                        client.CopyFileToVm(
                            fileToCopy,
                            copyFileDestination,
                            fileName,
                            createDirectoryOnRemoteMachine);

                        createDirectoryOnRemoteMachine = false;
                    }

                    Logger.LogVerbose("Copied files to the remote machine.\n");

                    string tempRemoteFolder = copyFileDestination;

                    if (copyFileDestination.EndsWith("\\") ||
                        copyFileDestination.EndsWith("/"))
                    {
                        tempRemoteFolder =
                            copyFileDestination.Substring(0, copyFileDestination.Length - 1);
                    }

                    Logger.LogVerbose("Invoking scripts on the remote machine.\n");

                    foreach (string scriptToInvoke in scriptsToInvoke)
                    {
                        client.InvokeScriptOnVm(
                            scriptToInvoke,
                            scriptOutputFile,
                            tempRemoteFolder);
                    }

                    Logger.LogVerbose("Invoked scripts to the remote machine.\n");

                    done = true;
                }
                catch (VMwarePushInstallException vmex)
                {
                    if (vmex.errorCode != VMwarePushInstallErrorCode.ScriptInvocationInvalidExitCode)
                    {
                        throw;
                    }

                    Logger.LogWarning(
                        "Received invalid exit code while invoking script on vm. Retrying ..\n");
                    attempts++;
                    System.Threading.Thread.Sleep(ConfVariables.RetryInterval);
                    if (attempts > ConfVariables.Retries)
                    {
                        Logger.LogError(
                            "Obtained invalid exit code while invoking script. Retry limit exceeded on vm.\n");

                        throw;
                    }
                }
            }
        }

        /// <summary>
        /// Returns a string representation of the object for debugging/tracing purposes..
        /// </summary>
        /// <returns>String representation of the object.</returns>
        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("CS IP : " + this.csIP);
            sb.AppendLine("CS port : " + this.csPort);
            sb.AppendLine("Is Https communication : " + this.useHttpsForCS);
            sb.AppendLine("vcenter IP : " + this.vcenterIP);
            sb.AppendLine("vcenter account id : " + this.vcenterAccountId);
            sb.AppendLine("VM IPs : " + this.vmIPList.Concat());
            sb.AppendLine("VM account id : " + this.vmAccountId);
            sb.AppendLine("VM name : " + this.vmName);
            sb.AppendLine("Files to be copied : " + this.filesToCopy.Concat());
            sb.AppendLine("Remote folder on machine : " + this.copyFileDestination);
            sb.AppendLine("Scripts to be invoked : " + this.scriptsToInvoke.Concat());
            sb.AppendLine("Script output file : " + this.scriptOutputFile);
            return sb.ToString();
        }
    }
}
