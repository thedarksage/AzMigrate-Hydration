//---------------------------------------------------------------
//  <copyright file="PSVMwareClient.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter via PowerCLI cmdlets.
//  </summary>
//
//  History:     10-Sep-2017   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Management.Automation;
using System.Net;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Cryptography.X509Certificates;

namespace VmWareClient
{
    /// <summary>
    /// Helper class for managing VMWare ESX/vCenter via PowerCLI cmdlets.
    /// </summary>
    public class PSVMwareClient : IDisposable
    {
        /// <summary>
        /// The PowerShell session under which PowerCLI cmdlets are being executed.
        /// </summary>
        private PowerShell psClient;

        /// <summary>
        /// To detect redundant calls.
        /// </summary>
        private bool disposedValue = false;

        /// <summary>
        /// Converts string to a secure string.
        /// </summary>
        /// <param name="value">Value of the string to be converted.</param>
        /// <returns>The encrypted string.</returns>
        public static SecureString StringToSecureString(string value)
        {
            SecureString secureString = new SecureString();
            char[] valueChars = value.ToCharArray();

            foreach (char c in valueChars)
            {
                secureString.AppendChar(c);
            }

            return secureString;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PSVMwareClient" /> class.
        /// </summary>
        /// <param name="ip">The ESX/vCenter IP address.</param>
        /// <param name="username">The user name.</param>
        /// <param name="password">The password.</param>
        public PSVMwareClient(string ip, string username, string password)
            : this(ip, username, StringToSecureString(password))
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PSVMwareClient" /> class.
        /// </summary>
        /// <param name="ip">The ESX/vCenter IP address.</param>
        /// <param name="username">The user name.</param>
        /// <param name="password">The password.</param>
        public PSVMwareClient(string ip, string username, SecureString password)
        {
            this.psClient = PowerShell.Create();
            var vCenterCreds = new PSCredential(username, password);

            this.psClient.Commands.Clear();
            this.psClient.AddCommand("Invoke-Command");
            string scriptContent = "Add-PSSnapin -Name VMWare.VimAutomation.Core;";
            ScriptBlock block = ScriptBlock.Create(scriptContent);
            this.psClient.AddParameter("ScriptBlock", block);
            this.psClient.Invoke();

            string connectCmd =
                    "$x = Connect-VIServer -Server $vCenterIP -Credential $vCenterCreds " +
                    "-Protocol https -Force;";

            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$vCenterIP," +
                "[parameter(Mandatory =$true)][PSCredential]$vCenterCreds" +
                ")" +
                connectCmd;

            object[] commandParameters = new object[] { ip, vCenterCreds };

            this.RunPowerShellCommand(command, commandParameters);
        }

        /// <summary>
        /// Finds the VM.
        /// </summary>
        /// <param name="vmIP">IP of the VM.</param>
        /// <returns>VM Object.</returns>
        public Object GetVmByIP(string vmIP)
        {
            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$vmIP" +
                ")" +
                "Get-VM | Where-Object -FilterScript { $_.Guest.Nics.IPAddress -contains $vmIP };";
            object[] commandParameters = new object[] { vmIP };

            Collection<PSObject> outputObjects = new Collection<PSObject>();
            outputObjects = this.RunPowerShellCommand(command, commandParameters);
            if (!outputObjects.Any())
            {
                throw new Exception("VM not found in the vCenter");
            }

            if (outputObjects.Count > 1)
            {
                throw new Exception("More than one machine found with the same IP in vcenter");
            }

            return outputObjects[0];
        }

        /// <summary>
        /// Finds the VM.
        /// </summary>
        /// <param name="vmName">Name of the VM.</param>
        /// <returns>VM object.</returns>
        public Object GetVmByName(string vmName)
        {
            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$vmName" +
                ")" +
                "Get-VM -Name $vmName;";
            object[] commandParameters = new object[] { vmName };

            Collection<PSObject> outputObjects = new Collection<PSObject>();
            outputObjects = this.RunPowerShellCommand(command, commandParameters);
            if (!outputObjects.Any())
            {
                throw new Exception("VM not found in the vCenter");
            }

            if (outputObjects.Count > 1)
            {
                throw new Exception("More than one machine found with the same IP in vcenter");
            }

            return outputObjects[0];
        }

        /// <summary>
        /// Copies a file to VM.
        /// </summary>
        /// <param name="filePathToBeCopied">Full file path on the local machine.</param>
        /// <param name="destinationFolder">Folder on the VM, where the file is to be copied.
        /// </param>
        /// <param name="guestUserName">User name for VM's guest user.</param>
        /// <param name="guestPassword">Password for VM's guest user.</param>
        /// <param name="vm">VM object obtained from get-VM API.</param>
        public void CopyFileToVm(
            string filePathToBeCopied,
            string destinationFolder,
            string guestUserName,
            string guestPassword,
            object vm)
        {
            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$sourceFilePath," +
                "[parameter(Mandatory =$true)][String]$destFolder," +
                "[parameter(Mandatory =$true)][String]$vmUserName," +
                "[parameter(Mandatory =$true)][String]$vmPassword," +
                "[parameter(Mandatory =$true)][Object]$vm" +
                ")" +
                "Copy-VMGuestFile -Source $sourceFilePath -Destination $destFolder " +
                "-LocalToGuest -GuestUser $vmUserName -GuestPassword $vmPassword -VM $vm -Force;";
            object[] commandParameters = new object[]
            {
                filePathToBeCopied,
                destinationFolder,
                guestUserName,
                guestPassword,
                vm
            };

            this.RunPowerShellCommand(command, commandParameters);
        }

        /// <summary>
        /// Copies a file from VM to local machine.
        /// </summary>
        /// <param name="vmFilePathToBePulled">Full file path on the virtual machine.</param>
        /// <param name="localDestinationFolder">Folder on the local machine, where the file is to be copied.</param>
        /// <param name="guestUserName">Username for VM's guest user.</param>
        /// <param name="guestPassword">Password for VM's guest user.</param>
        /// <param name="vm">VM object obtained from get-VM API.</param>
        public void PullFileFromVm(
            string vmFilePathToBePulled,
            string localDestinationFolder,
            string guestUserName,
            string guestPassword,
            object vm)
        {
            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$sourceFilePath," +
                "[parameter(Mandatory =$true)][String]$destFolder," +
                "[parameter(Mandatory =$true)][String]$vmUserName," +
                "[parameter(Mandatory =$true)][String]$vmPassword," +
                "[parameter(Mandatory =$true)][Object]$vm" +
                ")" +
                "Copy-VMGuestFile -Source $sourceFilePath -Destination $destFolder " + 
                "-GuestToLocal -GuestUser $vmUserName -GuestPassword $vmPassword -VM $vm -Force;";

            object[] commandParameters = new object[]
            {
                vmFilePathToBePulled,
                localDestinationFolder,
                guestUserName,
                guestPassword,
                vm
            };

            this.RunPowerShellCommand(command, commandParameters);
        }

        /// <summary>
        /// Invokes a script on remote VM.
        /// </summary>
        /// <param name="scriptToBeInvoked">Script to be invoked on the target VM.</param>
        /// <param name="guestUserName">User name for VM's guest user.</param>
        /// <param name="guestPassword">Password for VM's guest user.</param>
        /// <param name="vm">VM object obtained from get-VM API.</param>
        /// <returns>Script invocation output.</returns>
        public String InvokeScriptOnVm(
            string scriptToBeInvoked,
            string guestUserName,
            string guestPassword,
            object vm)
        {
            string command = "param(" +
                "[parameter(Mandatory =$true)][String]$script," +
                "[parameter(Mandatory =$true)][String]$vm," +
                "[parameter(Mandatory =$true)][String]$vmUserName," +
                "[parameter(Mandatory =$true)][String]$vmPassword" +
                ")" +
                "Invoke-VMScript -ScriptText $script -VM $vm -GuestUser $vmUserName " +
                "-GuestPassword $vmPassword;";
            object[] commandParameters =
                new object[] { scriptToBeInvoked, vm, guestUserName, guestPassword };

            Collection<PSObject> outputObjects = new Collection<PSObject>();
            outputObjects = this.RunPowerShellCommand(command, commandParameters);
            if (outputObjects.Any())
            {
                Object invocationOutput = outputObjects[0].Properties["ScriptOutput"].Value;
                if (invocationOutput != null)
                {
                    return invocationOutput.ToString();
                }
            }
            return "";
        }

        /// <summary>
        /// This code added to correctly implement the disposable pattern.
        /// </summary>
        public void Dispose()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            this.Dispose(true);
        }

        /// <summary>
        /// The dispose method.
        /// </summary>
        /// <param name="disposing">True if invoked via Dispose(). False if invoked via finalizer.
        /// </param>
        protected virtual void Dispose(bool disposing)
        {
            if (!this.disposedValue)
            {
                if (disposing)
                {
                    this.psClient.Dispose();
                }

                this.disposedValue = true;
            }
        }

        /// <summary>
        /// Runs PowerShell command and returns PSObject collection.
        /// </summary>
        /// <param name="psCommand">PS Command to run.</param>
        /// <param name="commandParameters">Any parameters to be passed</param>
        /// <returns>Returns a PSObject collection of the command output.</returns>
        private Collection<PSObject> RunPowerShellCommand(
            string psCommand,
            object[] commandParameters)
        {
            Collection<PSObject> outputObjects = new Collection<PSObject>();
            Collection<ErrorRecord> errorResult = new Collection<ErrorRecord>();

            this.ExecutePowerShell(
                psCommand,
                commandParameters,
                out outputObjects,
                out errorResult);

            List<Exception> exceptions = new List<Exception>();
            foreach (ErrorRecord error in errorResult)
            {
                exceptions.Add(new Exception(error.Exception.ToString()));
            }

            if (exceptions.Any())
            {
                throw new AggregateException(exceptions);
            }

            return outputObjects;
        }

        /// <summary>
        /// Executes the specified PowerShell locally.
        /// </summary>
        /// <param name="scriptContent">The script.</param>
        /// <param name="argumentList">The list of arguments.</param>
        /// <param name="outputObjects">The output object.</param>
        /// <param name="errorResult">The error object.</param>
        private void ExecutePowerShell(
            string scriptContent,
            object[] argumentList,
            out Collection<PSObject> outputObjects,
            out Collection<ErrorRecord> errorResult)
        {
            PSDataCollection<ErrorRecord> errors;
            errorResult = new Collection<ErrorRecord>();

            // Clear any existing commands before reusing psClient object.
            this.psClient.Commands.Clear();

            this.psClient.AddCommand("Invoke-Command");

            if (argumentList != null && argumentList.Count() != 0)
            {
                this.psClient.AddParameter("ArgumentList", argumentList);
            }

            ScriptBlock block = ScriptBlock.Create(scriptContent);
            this.psClient.AddParameter("ScriptBlock", block);

            outputObjects = this.psClient.Invoke();
            errors = this.psClient.Streams.Error;
            foreach (ErrorRecord error in errors)
            {
                errorResult.Add(error);
            }
        }
    }
}
