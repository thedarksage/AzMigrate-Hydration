//---------------------------------------------------------------
//  <copyright file="VMwareClient.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements commands for push installation using VMware Management SDK.
//  </summary>
//
//  History:     10-Sep-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Cryptography.X509Certificates;
using System.Threading;
using System.Threading.Tasks;

using InMage.APIHelpers;
using VMware.VSphere.Management;
using VMware.VSphere.Management.Operations;
using VMware.Interfaces;
using VMware.Interfaces.ExceptionHandling;
using VMware.Interfaces.Contracts.Generated;
using VMware.Interfaces.Models;
using VMware.VSphere.Management.Common;
using Vim25Api;

using VMwareContracts = VMware.Interfaces.Contracts.Generated;

namespace VMwareClient
{
    [System.ComponentModel.DesignerCategory("Code")]

    /// <summary>
    /// Extended web client to overwrite the web request timeout.
    /// </summary>
    public class ExtendedWebClient : WebClient
    {
        public TimeSpan Timeout { get; set; }
        public new bool AllowWriteStreamBuffering { get; set; }

        protected override WebRequest GetWebRequest(Uri address)
        {
            var request = base.GetWebRequest(address);
            if (request != null)
            {
                request.Timeout = Convert.ToInt32(Timeout.TotalMilliseconds);
                var httpRequest = request as HttpWebRequest;
                if (httpRequest != null)
                {
                    httpRequest.AllowWriteStreamBuffering = AllowWriteStreamBuffering;
                }
            }
            return request;
        }

        public ExtendedWebClient()
        {
            Timeout = ConfVariables.CopyFileTimeout;
        }
    }

    /// <summary>
    /// Enum to define the types of operation
    /// </summary>
    public enum VMwareClientOperationType
    {
        ConnectToVCenter,
        FindVmInVCenter,
        CopyFileToVm,
        FetchFileFromVm,
        InvokeScriptOnVm
    }

    /// <summary>
    /// Helper class to implement commands for push installation using VMware VIM SDK.
    /// </summary>
    public class VMwareClient
    {
        /// <summary>
        /// No.of retries for each operation.
        /// </summary>
        public int retries = ConfVariables.Retries;

        /// <summary>
        /// Time interval to wait until next retry.
        /// </summary>
        public TimeSpan retryInterval = ConfVariables.RetryInterval;

        /// <summary>
        /// VSphereManagementClient object used to connect to vcenter and perform operations.
        /// </summary>
        IVMwareClient vmwareClient = null;

        /// <summary>
        /// IP of the vcenter.
        /// </summary>
        string vcenterIP = String.Empty;

        /// <summary>
        /// Username to connect to vcenter.
        /// </summary>
        string vcenterUsername = String.Empty;

        /// <summary>
        /// Password to connect to vcenter.
        /// </summary>
        string vcenterPassword = String.Empty;

        /// <summary>
        /// Name of the vm on which push installation is to be done.
        /// </summary>
        string vmName = String.Empty;

        /// <summary>
        /// set of IPs of the VM.
        /// </summary>
        string[] vmIPs = { };

        /// <summary>
        /// Username to login to vm.
        /// </summary>
        string vmUsername = String.Empty;

        /// <summary>
        /// Password to login to vm.
        /// </summary>
        string vmPassword = String.Empty;

        /// <summary>
        /// VM on which the operation is to be performed.
        /// </summary>
        IVirtualMachine vm = null;

        /// <summary>
        /// Initializes a new instance of the <see cref="VMwareClient" /> class.
        /// </summary>
        /// <param name="vcenterIP">IP of the vcenter to connect to.</param>
        /// <param name="vcenterUsername">Username to connect to the vcenter.</param>
        /// <param name="vcenterPassword">Password to connect to the vcenter.</param>
        /// <param name="vmName">Name of the vm to perform the operations on.</param>
        /// <param name="vmIPs">IP addresses of vm.</param>
        /// <param name="vmUsername">Username to connect to the vm.</param>
        /// <param name="vmPassword">Password to connect to the vm.</param>
        public VMwareClient(
            string vcenterIP,
            string vcenterUsername,
            string vcenterPassword,
            string vmName,
            string[] vmIPs,
            string vmUsername,
            string vmPassword)
        {
            this.vcenterIP = vcenterIP;
            this.vcenterUsername = vcenterUsername;
            this.vcenterPassword = vcenterPassword;
            this.vmName = vmName;
            this.vmIPs = vmIPs;
            this.vmUsername = vmUsername;
            this.vmPassword = vmPassword;
        }

        /// <summary>
        /// Get the operation common error code from operation types.
        /// </summary>
        /// <param name="opType">Type for which error code is required.</param>
        /// <returns>Returns the corrsponding error code for operation.</returns>
        VMwarePushInstallErrorCode GetErrorCodeFromOperationType(VMwareClientOperationType opType)
        {
            VMwarePushInstallErrorCode errCode;
            switch (opType)
            {
                case VMwareClientOperationType.ConnectToVCenter:
                    errCode = VMwarePushInstallErrorCode.ConnectToVCenterFailed;
                    break;
                case VMwareClientOperationType.FindVmInVCenter:
                    errCode = VMwarePushInstallErrorCode.FindVmInVCenterFailed;
                    break;
                case VMwareClientOperationType.CopyFileToVm:
                    errCode = VMwarePushInstallErrorCode.CopyFileToVmFailed;
                    break;
                case VMwareClientOperationType.FetchFileFromVm:
                    errCode = VMwarePushInstallErrorCode.FetchFileFromVmFailed;
                    break;
                case VMwareClientOperationType.InvokeScriptOnVm:
                    errCode = VMwarePushInstallErrorCode.InvokeScriptOnVmFailed;
                    break;
                default:
                    Logger.LogError(String.Format("Unknown operation type {0}.\n", opType.ToString()));
                    errCode = VMwarePushInstallErrorCode.OperationTypeInvalid;
                    break;
            }

            return errCode;
        }

        /// <summary>
        /// Handle any exception caught while executing vmware client operations.
        /// </summary>
        /// <param name="ex">Exception caught.</param>
        /// <param name="opType">Operation type in which exception was encountered.</param>
        public void HandleException(Exception ex, VMwareClientOperationType opType)
        {
            if (ex is VimException)
            {
                VimException vex = ex as VimException;

                Logger.LogError(
                    String.Format(
                        "Received vim exception of type {0}.\n",
                        vex.MethodFault.GetType().ToString()));

                if (vex.MethodFault is InvalidLogin)
                {
                    throw new VMwarePushInstallException(
                        VMwarePushInstallErrorCode.VCenterInvalidLoginCreds.ToString(),
                        VMwarePushInstallErrorCode.VCenterInvalidLoginCreds,
                        vex.ToString());
                }
                else if (vex.MethodFault is GuestComponentsOutOfDate)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmGuestComponentsOutOfDate,
                        vex.ToString());
                }
                else if (vex.MethodFault is GuestOperationsUnavailable)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmGuestComponentsNotRunning,
                        vex.ToString());
                }
                else if (vex.MethodFault is GuestPermissionDenied ||
                    vex.MethodFault is NoPermission)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmGuestCredsNoPermission,
                        vex.ToString());
                }
                else if (vex.MethodFault is InvalidGuestLogin)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmInvalidGuestLoginCreds,
                        vex.ToString());
                }
                else if (vex.MethodFault is InvalidPowerState)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmPoweredOff,
                        vex.ToString());
                }
                else if (vex.MethodFault is InvalidState)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmInvalidState,
                        vex.ToString());
                }
                else if (vex.MethodFault is OperationDisabledByGuest)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmOperationDisabledByGuest,
                        vex.ToString());
                }
                else if (vex.MethodFault is OperationNotSupportedByGuest)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmOperationUnsupportedOnGuest,
                        vex.ToString());
                }
                else if (vex.MethodFault is TaskInProgress)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmBusy,
                        vex.ToString());
                }
                else if (vex.MethodFault is CannotAccessFile)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VmFileCannotBeAccessed,
                        vex.ToString());
                }
                /*else if (vex.MethodFault is ConverterFileNotFound)
                {

                }*/
                else if (vex.MethodFault is RestrictedVersion)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.VCenterLicenseRestrictedVersion,
                        vex.ToString());
                }
                else if (vex.MethodFault is FileNotFound)
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        VMwarePushInstallErrorCode.FileToBeFetchedNotAvailableInVm,
                        vex.ToString());
                }
                else
                {
                    throw new VMwarePushInstallException(
                        vex.MethodFault.GetType().ToString(),
                        GetErrorCodeFromOperationType(opType),
                        vex.ToString());
                }
            }
            else if (ex is VMwareClientException)
            {
                VMwareClientException cliex = ex as VMwareClientException;

                Logger.LogError(
                    String.Format(
                        "Received vmware client exception with error code {0}.\n",
                        cliex.ErrorCode.ToString()));

                if (cliex.ErrorCode == VMwareClientErrorCode.VcenterConnectionFailedInvalidCredentials)
                {
                    throw new VMwarePushInstallException(
                        VMwareClientErrorCode.VcenterConnectionFailedInvalidCredentials.ToString(),
                        VMwarePushInstallErrorCode.VCenterInvalidLoginCreds,
                        cliex.ToString());
                }
                else if (cliex.ErrorCode == VMwareClientErrorCode.VcenterConnectionFailedIpNotReachable)
                {
                    throw new VMwarePushInstallException(
                        VMwareClientErrorCode.VcenterConnectionFailedIpNotReachable.ToString(),
                        VMwarePushInstallErrorCode.VCenterIpNotReachable,
                        cliex.ToString());
                }
                else if (cliex.ErrorCode == VMwareClientErrorCode.UnableToConnectTovCenter)
                {
                    throw new VMwarePushInstallException(
                        VMwareClientErrorCode.UnableToConnectTovCenter.ToString(),
                        VMwarePushInstallErrorCode.UnableToConnectTovCenter,
                        cliex.ToString());
                }
                else
                {
                    throw new VMwarePushInstallException(
                        cliex.ErrorCode.ToString(),
                        GetErrorCodeFromOperationType(opType),
                        cliex.ToString());
                }
            }
            else if (ex is VMwarePushInstallException)
            {
                VMwarePushInstallException vmex = ex as VMwarePushInstallException;
                Logger.LogError(
                    String.Format(
                        "Received vmware push install exception with error code {0} in operation {1}.\n",
                        vmex.errorCode.ToString(),
                        opType.ToString()));

                throw ex;
            }
            else if (ex is WebException)
            {
                WebException wex = ex as WebException;
                Logger.LogError(
                    String.Format(
                        "Received web exception in operation {0}.\n",
                        opType.ToString()));

                if (wex.Response != null)
                {
                    using (var reader = wex.Response.GetResponseStream())
                    {
                        string pageContent = new StreamReader(reader).ReadToEnd();
                        Logger.LogError(
                            String.Format(
                                "Web exception details : {0}.\n",
                                pageContent));
                    }
                }
            }
            else if (ex is NullReferenceException)
            {
                Logger.LogError(
                    String.Format(
                        "Received null reference exception in operation {0}.\n",
                        opType.ToString()));
            }
            else if (ex is NotSupportedException)
            {
                Logger.LogError(
                    String.Format(
                        "Received unsupported exception in operation {0}.\n",
                        opType.ToString()));
            }
            else
            {
                Logger.LogError(
                    String.Format(
                        "Caught generic exception of type {0} in operation {1}.\n Exception details : {2}\n",
                        ex.GetType().ToString(),
                        opType.ToString(),
                        ex.ToString()));
            }

            throw new VMwarePushInstallException(
                ex.GetType().ToString(),
                GetErrorCodeFromOperationType(opType),
                ex.ToString());
        }

        /// <summary>
        /// Renews the connection to vcenter.
        /// Invoked when socket exceptions or internal server error is encountered.
        /// </summary>
        public void RenewVCenterConnection()
        {
            this.ConnectToVCenter();

            vm = this.FindVmInVCenter();
        }

        /// <summary>
        /// Create connection to specified vcenter.
        /// </summary>
        public void ConnectToVCenter()
        {
            int attempts = 0;
            bool done = false;

            while (!done)
            {
                try
                {
                    vmwareClient = VSphereManagementClient.GetClient("https://" + this.vcenterIP + "/sdk", this.vcenterUsername, this.vcenterPassword);
                    done = true;
                }
                catch (Exception ex)
                {
                    if (!ex.IsRetryableException())
                    {
                        HandleException(ex, VMwareClientOperationType.ConnectToVCenter);
                    }
                    else
                    {
                        Logger.LogWarning(
                            String.Format(
                                "Hit retryable exception in connecting to vcenter: '{0}'.\n",
                                ex.ToString()));
                    }

                    attempts++;
                    Thread.Sleep(retryInterval);
                    if (attempts > retries)
                    {
                        Logger.LogError(
                            String.Format(
                                "Retry limit exceeded while connecting to vcenter '{0}'.\n",
                                this.vcenterIP));

                        HandleException(ex, VMwareClientOperationType.ConnectToVCenter);
                    }
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Connect to vcenter '{0}' successful.\n",
                this.vcenterIP));
        }

        /// <summary>
        /// Finds the VM.
        /// </summary>
        /// <returns>Virtual machine object if found.</returns>
        public VMware.Interfaces.Models.IVirtualMachine FindVmInVCenter()
        {
            VMware.Interfaces.Models.IVirtualMachine matchingVM = null;
            int attempts = 0;
            bool done = false;

            while (!done)
            {
                try
                {
                    NameValueCollection filters = new NameValueCollection();
                    filters.Add("Name", this.vmName);

                    int count = 0;
                    List<VMware.Interfaces.Models.IVirtualMachine> matchingVms = new List<VMware.Interfaces.Models.IVirtualMachine>();
                    VirtualMachineOperations vmOpts = (VirtualMachineOperations)vmwareClient.VmOpts;
                    if (vmOpts != null)
                    {
                        Logger.LogVerbose("Getting Vms in vcenter.\n");
                        List<VMware.Interfaces.Models.IVirtualMachine> vmViews = vmOpts.GetVmViewsByProps().ToList();
                        if (vmViews != null)
                        {
                            foreach (var vm in vmViews)
                            {
                                if (vm.Name == this.vmName)
                                {
                                    matchingVms.Add(vm);
                                }
                            }
                        }

                        if (matchingVms.Count == 1)
                        {
                            Logger.LogVerbose("Only one vm found with the given name. So, skipping IP check.\n");
                            matchingVM = matchingVms[0];
                            count = 1;
                        }

                        if (matchingVms.Count > 1)
                        {
                            Logger.LogVerbose(
                                String.Format(
                                    "Found multiple VMs with the same name {0}. Filtering IPs.\n",
                                    this.vmName));

                            Logger.LogVerbose("Filtering Vms in vcenter.\n");
                            foreach (VMware.Interfaces.Models.IVirtualMachine vm in matchingVms)
                            {
                                if (vm.Guest != null && vm.Guest.Net != null)
                                {
                                    List<string> ipAddresses = new List<string>();
                                    foreach (var nicInfo in vm.Guest.Net)
                                    {
                                        if (nicInfo.IpAddress != null)
                                        {
                                            ipAddresses.AddRange(nicInfo.IpAddress);
                                        }
                                    }

                                    Logger.LogVerbose(
                                        String.Format(
                                            "Candidate VM IP addresses : {0}.\n",
                                            this.vmIPs.Concat()));

                                    if (!this.vmIPs.Except(ipAddresses).Any())
                                    {
                                        matchingVM = vm;
                                        count++;
                                    }
                                }
                            }
                        }
                    }

                    if (count == 0)
                    {
                        Logger.LogError(
                            String.Format(
                                "VM with name '{0}' and IPs '{1}' is not found in vcenter.\n",
                                this.vmName,
                                this.vmIPs.Concat()));
                        throw new VMwarePushInstallException(
                            VMwarePushInstallErrorCode.VmNotFoundInVCenter.ToString(),
                            VMwarePushInstallErrorCode.VmNotFoundInVCenter,
                            String.Format(
                                "A VM with IPs {0} and name {1} is not found in vcenter.",
                                this.vmIPs.Concat(),
                                this.vmName));
                    }

                    if (count > 1)
                    {
                        Logger.LogError(
                            String.Format(
                                "Multiple VMs with name '{0}' and IPs '{1}' are found in vcenter.\n",
                                this.vmName,
                                this.vmIPs.Concat()));
                        throw new VMwarePushInstallException(
                            VMwarePushInstallErrorCode.MultipleVmsFoundInVCenter.ToString(),
                            VMwarePushInstallErrorCode.MultipleVmsFoundInVCenter,
                            String.Format(
                                "Multiple VM with IPs {0} and name {1} are found in vcenter.",
                                this.vmIPs.Concat(),
                                this.vmName));
                    }

                    Logger.LogVerbose(
                        String.Format(
                            "Found VM matching the IPs and name : '{0}'.\n",
                            matchingVM.Name));

                    done = true;
                }
                catch (Exception ex)
                {
                    if (!ex.IsRetryableException())
                    {
                        HandleException(ex, VMwareClientOperationType.FindVmInVCenter);
                    }
                    else
                    {
                        Logger.LogWarning(
                            String.Format(
                                "Hit retryable exception while finding vm in vcenter: {0}.\n",
                                ex.ToString()));
                    }

                    attempts++;
                    Thread.Sleep(retryInterval);
                    if (attempts > retries)
                    {
                        HandleException(ex, VMwareClientOperationType.FindVmInVCenter);
                    }
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Finding VM with IPs '{0}' and vmName '{1}' succeeded.\n",
                this.vmIPs.Concat(),
                this.vmName));

            this.vm = matchingVM;
            return matchingVM;
        }

        /// <summary>
        /// Copies a file to VM.
        /// </summary>
        /// <param name="filePathToBeCopied">Full file path on the local machine.</param>
        /// <param name="destinationFolder">
        /// Folder on the VM, where the file is to be copied.
        /// </param>
        /// <param name="fileName">FileName to be copied.</param>
        /// <param name="createFolderOnRemoteMachine">
        /// Specifies if destination folder has to be created on the remote machine.
        /// </param>
        public void CopyFileToVm(
            string filePathToBeCopied,
            string destinationFolder,
            string fileName,
            bool createFolderOnRemoteMachine)
        {
            int attempts = 0;
            bool done = false;

            if (vm.Summary.Config.GuestFullName.Contains("Windows"))
            {
                if (!destinationFolder.EndsWith("\\"))
                {
                    destinationFolder = String.Concat(destinationFolder, "\\");
                }
            }
            else
            {
                if (!destinationFolder.EndsWith("/"))
                {
                    destinationFolder = String.Concat(destinationFolder, "/");
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Copying file '{0}' to '{1}' on machine {2}.\n",
                filePathToBeCopied,
                destinationFolder,
                vm.Name));

            VMwareContracts.NamePasswordAuthentication vmAuth = new VMwareContracts.NamePasswordAuthentication();
            vmAuth.username = this.vmUsername;
            vmAuth.password = this.vmPassword;
            vmAuth.interactiveSession = false;

            Logger.LogVerbose(String.Format("Connecting to vm ..\n"));

            while (!done)
            {
                try
                {
                    if (createFolderOnRemoteMachine)
                    {
                        try
                        {
                            Logger.LogVerbose(
                                String.Format(
                                    "Creating directory {0} on source machine.\n",
                                    destinationFolder));
                            vmwareClient.VmOpts.MakeDirectoryInGuest(vm, vmAuth, destinationFolder, true);
                            Logger.LogVerbose(
                                String.Format(
                                    "Created directory {0} on source machine.\n",
                                    destinationFolder));
                        }
                        catch (VimException ve)
                        {
                            if (ve.MethodFault is Vim25Api.FileAlreadyExists)
                            {
                                Logger.LogVerbose(
                                    String.Format(
                                        "Remote directory {0} already exists on remote machine.\n",
                                        destinationFolder));

                                createFolderOnRemoteMachine = false;
                            }
                            else
                            {
                                throw;
                            }
                        }
                        catch (System.ServiceModel.FaultException ex)
                        {
                            Logger.LogWarning(
                                String.Format(
                                "Folder '{0}' creation on {1} failed with exception {2}." +
                                " Treating this as ignorable error.\n",
                                destinationFolder,
                                vm.Name,
                                ex.ToString()));
                            createFolderOnRemoteMachine = false;
                        }
                    }

                    System.IO.FileInfo fileToTransfer = new System.IO.FileInfo(@filePathToBeCopied);
                    string url = String.Empty;

                    if (vm.Summary.Config.GuestFullName.Contains("Windows"))
                    {
                        url = vmwareClient.VmOpts.InitiateFileTransferToGuest(
                            vm,
                            vmAuth,
                            destinationFolder + fileName,
                            new VMwareContracts.GuestFileAttributes(),
                            fileToTransfer.Length,
                            true);
                    }
                    else
                    {
                        VMwareContracts.GuestPosixFileAttributes gpfa = new VMwareContracts.GuestPosixFileAttributes();
                        gpfa.GroupId = null;
                        gpfa.OwnerId = null;
                        gpfa.Permissions = 001;
                        url = vmwareClient.VmOpts.InitiateFileTransferToGuest(
                            vm,
                            vmAuth,
                            destinationFolder + fileName,
                            gpfa,
                            fileToTransfer.Length,
                            true);
                    }

                    url = url.Replace("*", this.vcenterIP);
                    Logger.LogVerbose(String.Format("Copying file to url {0}.\n", url));

                    using (ExtendedWebClient webClient = new ExtendedWebClient())
                    {
                        webClient.AllowWriteStreamBuffering = false;
                        webClient.UploadData(url, "PUT", File.ReadAllBytes(filePathToBeCopied));
                    }

                    done = true;
                }
                catch (Exception ex)
                {
                    if (!ex.IsRetryableException())
                    {
                        HandleException(ex, VMwareClientOperationType.CopyFileToVm);
                    }
                    else
                    {
                        Logger.LogWarning(
                            String.Format(
                                "Hit retryable exception while copying file to vm: {0}.\n",
                                ex.ToString()));

                        if (ex.IsSocketException() || ex.IsInternalServerError())
                        {
                            Logger.LogVerbose("Hit socket exception or internal server error.. Renewing vmware handle.\n");
                            this.RenewVCenterConnection();
                        }
                    }

                    attempts++;
                    Thread.Sleep(retryInterval);
                    if (attempts > retries)
                    {
                        Logger.LogError(
                            String.Format(
                                "Retry limit exceeded while copying file '{0}' to vm.\n",
                                filePathToBeCopied));

                        HandleException(ex, VMwareClientOperationType.CopyFileToVm);
                    }
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Copying file '{0}' to VM {1} successful\n",
                filePathToBeCopied,
                vm.Name));
        }

        /// <summary>
        /// Copies a file from VM to local machine.
        /// </summary>
        /// <param name="vmFilePathToBePulled">Full file path on the virtual machine.</param>
        /// <param name="destinationFolder">Folder on local machine to pull the file.</param>
        /// <param name="fileName">FileName in the local machine to be written.</param>
        public void FetchFileFromVm(
            string vmFilePathToBePulled,
            string destinationFolder,
            string fileName)
        {
            int attempts = 0;
            bool done = false;

            Logger.LogVerbose(
                String.Format(
                    "Fetching file '{0}' from vm {1} to '{2}'.\n",
                    vmFilePathToBePulled,
                    vm.Name,
                    destinationFolder));

            while (!done)
            {
                try
                {
                    VMwareContracts.NamePasswordAuthentication vmAuth = new VMwareContracts.NamePasswordAuthentication();
                    vmAuth.username = this.vmUsername;
                    vmAuth.password = this.vmPassword;
                    vmAuth.interactiveSession = false;

                    VMwareContracts.FileTransferInformation fi = vmwareClient.VmOpts.InitiateFileTransferFromGuest(
                        vm,
                        vmAuth,
                        vmFilePathToBePulled);
                    fi.url = fi.url.Replace("*", this.vcenterIP);

                    System.IO.Directory.CreateDirectory(@destinationFolder);

                    using (ExtendedWebClient webClient = new ExtendedWebClient())
                    {
                        webClient.Timeout = ConfVariables.FetchFileTimeout;
                        webClient.DownloadFile(fi.url, destinationFolder + fileName);
                    }

                    done = true;
                }
                catch (System.ServiceModel.FaultException ex)
                {
                    Logger.LogWarning(
                        String.Format(
                            "File '{0}' not present in the remote machine.\n",
                            vmFilePathToBePulled));
                    throw new VMwarePushInstallException(
                        VMwarePushInstallErrorCode.FileToBeFetchedNotAvailableInVm.ToString(),
                        VMwarePushInstallErrorCode.FileToBeFetchedNotAvailableInVm,
                        String.Format(
                            "File '{0}' to be fetched is not present in vm.",
                            vmFilePathToBePulled));
                }
                catch (Exception ex)
                {
                    if (!ex.IsRetryableException())
                    {
                        HandleException(ex, VMwareClientOperationType.FetchFileFromVm);
                    }
                    else
                    {
                        Logger.LogWarning(
                            String.Format(
                                "Hit retryable exception while pulling file from vm: {0}.\n",
                                ex.ToString()));

                        if (ex.IsSocketException() || ex.IsInternalServerError())
                        {
                            Logger.LogVerbose("Hit socket exception or internal server error.. Renewing vmware handle.\n");
                            this.RenewVCenterConnection();
                        }
                    }

                    attempts++;
                    Thread.Sleep(retryInterval);
                    if (attempts > retries)
                    {
                        HandleException(ex, VMwareClientOperationType.FetchFileFromVm);
                    }
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Pulling file '{0}' from VM {1} successful\n",
                vmFilePathToBePulled,
                vm.Name));
        }

        /// <summary>
        /// Invokes a script on remote VM.
        /// <param name="scriptToBeInvoked">Script to be invoked on the target VM.</param>
        /// <param name="outputFilePath">Script output will be written to this file.</param>
        /// <param name="tempDirInVm">Remote temporary location on Vm.</param>
        /// <returns>Script invocation output.</returns>
        /// </summary>
        public void InvokeScriptOnVm(
            string scriptToBeInvoked,
            string outputFilePath,
            string tempDirInVm)
        {
            int attempts = 0;
            bool done = false;

            Logger.LogVerbose(
                String.Format(
                    "Invoking script '{0}' on machine {1}.\n",
                    scriptToBeInvoked,
                    vm.Name));

            while (!done)
            {
                try
                {
                    VMwareContracts.NamePasswordAuthentication vmAuth = new VMwareContracts.NamePasswordAuthentication();
                    vmAuth.username = this.vmUsername;
                    vmAuth.password = this.vmPassword;
                    vmAuth.interactiveSession = false;

                    VMwareContracts.GuestProgramSpec spec = new VMwareContracts.GuestProgramSpec();
                    string outputDir = String.Empty;
                    string outputFileName = String.Empty;

                    outputDir = outputFilePath.Substring(
                                0,
                                outputFilePath.LastIndexOf("\\") + 1);
                    outputFileName = 
                        outputFilePath.Substring(outputFilePath.LastIndexOf("\\") + 1);

                    Logger.LogVerbose(
                        String.Format(
                        "Creating file ScriptOutput.txt in location {0} on machine {1}",
                        tempDirInVm,
                        vm.Name));

                    string scriptOutputFilePath =
                        vmwareClient.VmOpts.CreateTemporaryFileInGuest(
                            vm,
                            vmAuth,
                            "ScriptOutput",
                            ".txt",
                            tempDirInVm);

                    if (vm.Summary.Config.GuestFullName.Contains("Windows"))
                    {
                        int index = scriptToBeInvoked.IndexOf("\"", 1);
                        spec.programPath = scriptToBeInvoked.Substring(1, index-1);
                        spec.arguments =
                            scriptToBeInvoked.Substring(index+2);
                    }
                    else
                    {
                        int index = scriptToBeInvoked.IndexOf(" ");

                        if (index != -1)
                        {
                            spec.programPath = scriptToBeInvoked.Substring(0, index);
                            spec.arguments =
                                scriptToBeInvoked.Substring(index + 1) + " > " + scriptOutputFilePath;
                        }
                        else
                        {
                            spec.programPath = scriptToBeInvoked;
                            spec.arguments = " > " + scriptOutputFilePath;
                        }
                    }

                    Logger.LogVerbose(
                        String.Format(
                            "programpath: {0}\n arguments: {1}.\n",
                            spec.programPath,
                            spec.arguments));

                    Logger.LogVerbose(
                        String.Format(
                        "Invoking script '{0}' on VM {1}.\n",
                        spec.programPath + " " + spec.arguments,
                        vm.Name));

                    int? exitCode = null;
                    long pid = vmwareClient.VmOpts.StartProgramInGuest(
                        vm,
                        vmAuth,
                        spec);

                    Logger.LogVerbose(String.Format("Obtained process id : {0}.\n", pid));
                    
                    TimeSpan pollInterval = TimeSpan.FromSeconds(15);
                    TimeSpan timeElapsed = TimeSpan.FromSeconds(0);
                    while (true)
                    {
                        var processInfo =
                            vmwareClient.VmOpts.ListProcessesInGuest(vm, vmAuth, new[] { pid });
                        if (processInfo != null &&
                            processInfo.ReturnVal != null)
                        {
                            var processExitCodeFound = false;
                            foreach (var process in processInfo.ReturnVal)
                            {
                                if (process.ExitCodeSpecified && process.Pid == pid)
                                {
                                    exitCode = process.ExitCode;
                                    processExitCodeFound = true;
                                    break;
                                }
                            }
                            
                            if (processExitCodeFound)
                            {
                                break;
                            }
                        }

                        Thread.Sleep(pollInterval);
                        timeElapsed += pollInterval;
                        if (timeElapsed > ConfVariables.ScriptInvocationTimeout)
                        {
                            Logger.LogError(
                                "Script invocation timed out. Exiting with exitcode -1.\n");
                            throw new VMwarePushInstallException(
                                VMwarePushInstallErrorCode.ScriptInvocationTimedOutOnVm.ToString(),
                                VMwarePushInstallErrorCode.ScriptInvocationTimedOutOnVm,
                                String.Format(
                                    "Mobility agent installation script execution timed out on vm {0}.\n",
                                    vm.Name));
                        }
                    }

                    Logger.LogVerbose(
                        String.Format(
                            "Invoke script on vm exited with exit code {0}.\n",
                            exitCode.HasValue ? exitCode.ToString() : "<null>"));

                    if (exitCode != null && exitCode != 0)
                    {
                        if (exitCode != 0 && exitCode != -1 && exitCode != 255)
                        {
                            // try running a sample "dir" or "ls" command
                            try
                            {
                                if (vm.Summary.Config.GuestFullName.Contains("Windows"))
                                {
                                    spec.programPath = "C:\\Windows\\System32\\cmd.exe";
                                    spec.arguments = "/c \"dir \"C:\\ProgramData\"\"";
                                }
                                else
                                {
                                    spec.programPath = "/bin/ls";
                                    spec.arguments = String.Empty;
                                }

                                pid = vmwareClient.VmOpts.StartProgramInGuest(
                                        vm,
                                        vmAuth,
                                        spec);
                                var processExitCodeFound = false;
                                while (true)
                                {
                                    var processInfo =
                                        vmwareClient.VmOpts.ListProcessesInGuest(vm, vmAuth, new[] { pid });
                                    if (processInfo != null &&
                                        processInfo.ReturnVal != null)
                                    {
                                        foreach (var process in processInfo.ReturnVal)
                                        {
                                            if (process.ExitCodeSpecified && process.Pid == pid)
                                            {
                                                exitCode = process.ExitCode;
                                                processExitCodeFound = true;
                                                break;
                                            }
                                        }

                                        if (processExitCodeFound)
                                        {
                                            break;
                                        }
                                    }

                                    System.Threading.Thread.Sleep(pollInterval);
                                    timeElapsed += pollInterval;
                                    if (timeElapsed > TimeSpan.FromMinutes(1))
                                    {
                                        Logger.LogError("Sample script invocation timed out. Exiting with exitcode -1.\n");
                                        break;
                                    }
                                }

                                if (processExitCodeFound)
                                {
                                    Logger.LogVerbose(
                                        String.Format(
                                            "Sample command execution exited with exit code {0}.\n",
                                            exitCode));
                                }
                            }
                            catch (Exception ex)
                            {
                                Logger.LogError(
                                    String.Format(
                                        "Exception received while running sample command : {0}.\n",
                                        ex.ToString()));
                            }
                            catch 
                            {
                                Logger.LogError("Unknown exception received while executing sample command.\n");
                            }

                            throw new VMwarePushInstallException(
                                exitCode.ToString(),
                                VMwarePushInstallErrorCode.ScriptInvocationInvalidExitCode,
                                String.Format(
                                    "Invoke script on vm exited with exit code {0}.\n",
                                    exitCode));
                        }

                        // Ignoring non-zero error code since agent installer
                        // exits with 255 code in some successful scenarios as well
                        /*throw new VMwarePushInstallException(
                            VMwarePushInstallErrorCode.InvokeScriptOnVmFailed.ToString(),
                            VMwarePushInstallErrorCode.InvokeScriptOnVmFailed,
                            String.Format(
                                "Invoke script on vm exited with exit code {0}.\n",
                                exitCode));*/
                    }

                    FetchFileFromVm(
                        scriptOutputFilePath,
                        outputDir,
                        outputFileName);

                    vmwareClient.VmOpts.DeleteFileInGuest(vm, vmAuth, scriptOutputFilePath);
                    done = true;
                }
                catch (Exception ex)
                {
                    if (!ex.IsRetryableException())
                    {
                        HandleException(ex, VMwareClientOperationType.InvokeScriptOnVm);
                    }
                    else
                    {
                        Logger.LogWarning(
                            String.Format(
                                "Hit retryable exception while invoking script on vm: {0}.\n",
                                ex.ToString()));

                        if (ex.IsSocketException() || ex.IsInternalServerError())
                        {
                            Logger.LogVerbose("Hit socket exception or internal server error.. Renewing vmware handle.\n");
                            this.RenewVCenterConnection();
                        }
                    }

                    attempts++;
                    Thread.Sleep(retryInterval);
                    if (attempts > retries)
                    {
                        Logger.LogError(
                            String.Format(
                                "Retry limit exceeded while invoking script {0} on vm.\n",
                                scriptToBeInvoked));

                        HandleException(ex, VMwareClientOperationType.InvokeScriptOnVm);
                    }
                }
            }

            Logger.LogVerbose(
                String.Format(
                "Invoking script '{0}' on VM {1} successful\n",
                scriptToBeInvoked,
                vm.Name));
        }
    }
}
