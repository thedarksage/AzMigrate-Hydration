//---------------------------------------------------------------
//  <copyright file="FolderOpts.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     07-May-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware data center operations.
    /// </summary>
    public class FolderOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// The resource pool operations helper.
        /// </summary>
        private ResourcePoolOpts resourcePoolOpts;

        /// <summary>
        /// The data store operations helper.
        /// </summary>
        private DatastoreOpts datastoreOpts;

        /// <summary>
        /// The VM operations helper.
        /// </summary>
        private VmOpts vmOpts;

        /// <summary>
        /// Initializes a new instance of the <see cref="FolderOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        /// <param name="resourcePoolOpts">The resource pool operations helper.</param>
        /// <param name="datastoreOpts">The data store operations helper.</param>
        /// <param name="vmOpts">The VM operations helper.</param>
        public FolderOpts(
            VMware.Vim.VimClient client,
            ResourcePoolOpts resourcePoolOpts,
            DatastoreOpts datastoreOpts,
            VmOpts vmOpts)
        {
            this.client = client;
            this.resourcePoolOpts = resourcePoolOpts;
            this.datastoreOpts = datastoreOpts;
            this.vmOpts = vmOpts;
        }

        /// <summary>
        /// Create configuration of Machine which will not have any VMDK or Network. It is just
        /// template configuration of VM.
        /// </summary>
        /// <param name="sourceNode">The source being protected.</param>
        /// <param name="targetResourcePoolGroupName">The target resource pool group name.</param>
        /// <param name="tgtHostView">The target host view.</param>
        /// <param name="tgtDatacenterView">The target data center.</param>
        /// <param name="createdVmInfo">Information about the VM to be created.</param>
        public void CreateVmConfig(
            VmCreateInfo sourceNode,
            string targetResourcePoolGroupName,
            HostSystem tgtHostView,
            Datacenter tgtDatacenterView,
            out CreatedVmInfo createdVmInfo)
        {
            createdVmInfo = new CreatedVmInfo();
            string targetDataCenter = tgtDatacenterView.Name;
            string displayName = sourceNode.display_name;
            if (!string.IsNullOrEmpty(sourceNode.new_displayname))
            {
                displayName = sourceNode.new_displayname;
            }

            try
            {
                VirtualMachine existingVm = this.vmOpts.GetVmViewByUuid(sourceNode.target_uuid);
                T.Tracer().TraceInformation(
                    "Found existing virtual machine: '{0}' uuid: '{1}'." +
                    "Not Deleting this vm.",
                    displayName,
                    sourceNode.target_uuid);
                this.UpdateCreateVmInfo(existingVm, sourceNode, createdVmInfo);
                return;
            }
            catch (VMwareClientException vce)
            {
                if (vce.ErrorCode != VMwareClientErrorCode.VirtualMachineNotFound)
                {
                    throw;
                }
            }

            T.Tracer().TraceInformation(
                "Creating virtual machine {0} uuid {1}.",
                displayName, 
                createdVmInfo.Uuid);

            var vmConfigSpec = new VirtualMachineConfigSpec();
            vmConfigSpec.Name = displayName;
            vmConfigSpec.CpuHotAddEnabled = true;
            vmConfigSpec.CpuHotRemoveEnabled = true;
            vmConfigSpec.MemoryMB = long.Parse(sourceNode.memsize);
            vmConfigSpec.NumCPUs = int.Parse(sourceNode.cpucount);
            vmConfigSpec.Uuid = sourceNode.target_uuid;

            var fileInfo = new VirtualMachineFileInfo();
            DiskInfo[] disksInfo = sourceNode.disk.ToArray();
            string datastoreSelected = "[" + disksInfo[0].datastore_selected + "]";
            string vmxPathName = sourceNode.vmx_path;
            string[] vmxPathSplit = vmxPathName.Split(
                new string[] { "[", "]" },
                StringSplitOptions.RemoveEmptyEntries);
            string vmPathName = datastoreSelected + vmxPathSplit.Last();
            string configFilesPath = datastoreSelected + " " + sourceNode.folder_path;
            if (!string.IsNullOrEmpty(sourceNode.vmDirectoryPath))
            {
                vmxPathSplit[vmxPathSplit.Length - 1] = vmxPathSplit.Last().TrimStart();
                vmPathName =
                    datastoreSelected +
                    " " +
                    sourceNode.vmDirectoryPath +
                    "/" +
                    vmxPathSplit.Last();
                configFilesPath =
                    datastoreSelected +
                    " " +
                    sourceNode.vmDirectoryPath +
                    "/" +
                    sourceNode.folder_path;
            }

            fileInfo.VmPathName = vmPathName;
            fileInfo.LogDirectory = configFilesPath;
            fileInfo.SnapshotDirectory = configFilesPath;
            fileInfo.SuspendDirectory = configFilesPath;
            vmConfigSpec.Files = fileInfo;
            T.Tracer().TraceInformation("VMX path on target: {0}.", vmPathName);

            vmConfigSpec.GuestId = sourceNode.alt_guest_name;
            vmConfigSpec.Version = sourceNode.vmx_version;
            if (string.IsNullOrEmpty(sourceNode.vmx_version))
            {
                if (string.Compare(sourceNode.hostversion, "4.0.0") < 0)
                {
                    vmConfigSpec.Version = "vmx-04";
                }
                else
                {
                    vmConfigSpec.Version = "vmx-07";
                }
            }

            if (sourceNode.efi)
            {
                vmConfigSpec.Firmware = "efi";
                vmConfigSpec.Version = "vmx-08";
                sourceNode.vmx_version = "vmx-08";
                T.Tracer().TraceInformation(
                    "Firmware type of virtual machine {0} is EFI and vmx version is vmx-08",
                    displayName);
            }

            string resourcePoolGroupId = sourceNode.resourcepoolgrpname;
            if (string.IsNullOrEmpty(resourcePoolGroupId))
            {
                resourcePoolGroupId = targetResourcePoolGroupName;
            }

            T.Tracer().TraceInformation("Resource pool group Id = {0}.", resourcePoolGroupId);
            ResourcePool tgtResourcePoolView =
                this.resourcePoolOpts.FindResourcePool(resourcePoolGroupId);
            Folder vmFolderView = (Folder)this.client.GetView(tgtDatacenterView.VmFolder, null);

            ManagedObjectReference task = null;

            try
            {
                task = vmFolderView.CreateVM_Task(
                    vmConfigSpec,
                    tgtResourcePoolView.MoRef,
                    tgtHostView.MoRef);
            }
            catch (Exception e)
            {
                T.Tracer().TraceException(e, "Failed to create virtual machine {0}.", displayName);
                throw;
            }

            Task taskView = (Task)this.client.GetView(task, null);
            bool keepWaiting = true;
            VirtualMachine createdVmView = null;

            while (keepWaiting)
            {
                TaskInfo taskInfo = taskView.Info;
                if (taskInfo.State == TaskInfoState.success)
                {
                    createdVmView = (VirtualMachine)this.client.GetView(
                        (ManagedObjectReference)taskInfo.Result,
                        null);
                    T.Tracer().TraceInformation("Successfully created VM {0}.", createdVmView.Name);
                    this.UpdateCreateVmInfo(createdVmView, sourceNode, createdVmInfo);
                    if (sourceNode.vsan)
                    {
                        return;
                    }

                    break;
                }

                if (taskInfo.State == TaskInfoState.error)
                {
                    string errorMessage = taskInfo.Error.LocalizedMessage;
                    T.Tracer().TraceError(
                        "Creation of virtual machine {0} failed with error message: {1}.",
                        displayName,
                        errorMessage);
                    throw VMwareClientException.ShadowVmCreationFailed(
                        displayName,
                        tgtHostView.Name,
                        errorMessage);
                }

                System.Threading.Thread.Sleep(TimeSpan.FromSeconds(1));
                T.Tracer().TraceInformation("Updating view of virtual machine {0}.", displayName);
                taskView.UpdateViewData();
            }

            this.CheckVmPath(
                createdVmView,
                vmPathName,
                datastoreSelected,
                targetDataCenter,
                tgtDatacenterView,
                tgtHostView,
                tgtResourcePoolView,
                vmFolderView);
        }

        /// <summary>
        /// Updates the createdVmInfo of created/found virtual machine
        /// </summary>
        /// <param name="vm">The created or found vm.</param>
        /// <param name="sourceNode">The source being protected.</param>
        /// <param name="createdVmInfo">Information about the VM to be created.</param>
        private void UpdateCreateVmInfo(
            VirtualMachine vm,
            VmCreateInfo sourceNode,
            CreatedVmInfo createdVmInfo)
        {
            createdVmInfo.Uuid = vm.Summary.Config.Uuid;
            createdVmInfo.HostName = sourceNode.hostname;
            sourceNode.target_uuid = vm.Summary.Config.Uuid;

            // Note: Setting vm_console_url is not ported.
            if (sourceNode.vsan)
            {
                string[] datastore = vm.Summary.Config.VmPathName.Split(
                    new string[] { "[", "]" },
                    StringSplitOptions.RemoveEmptyEntries);
                int finalSlash = datastore[1].LastIndexOf('/');
                string vSanFolder = datastore[1].Substring(1, finalSlash);
                sourceNode.vsan_folder = vSanFolder;
                T.Tracer().TraceInformation(
                    "Found VM vSan folder path as {0}.",
                    vSanFolder);
            }
        }

        /// <summary>
        /// Check what is the vmPath configured at Dr-Site. If it differs from what we need it
        /// makes necessary changes.
        /// </summary>
        /// <param name="vmView">The VM that got created.</param>
        /// <param name="vmPathName">The VM path name.</param>
        /// <param name="datastoreSelected">Selected data store.</param>
        /// <param name="targetDataCenter">Target data center.</param>
        /// <param name="tgtDatacenterView">Target data center view.</param>
        /// <param name="tgtHostView">Target host view.</param>
        /// <param name="tgtResourcePoolView">Target resource pool view.</param>
        /// <param name="vmFolderView">VM folder view.</param>
        private void CheckVmPath(
            VirtualMachine vmView,
            string vmPathName,
            string datastoreSelected,
            string targetDataCenter,
            Datacenter tgtDatacenterView,
            HostSystem tgtHostView,
            ResourcePool tgtResourcePoolView,
            Folder vmFolderView)
        {
            string displayName = vmView.Name;
            string createdVmPath = vmView.Summary.Config.VmPathName;
            string reqVmPath = vmPathName;

            int rIndex = createdVmPath.LastIndexOf('/');
            string createdDsPath = createdVmPath.Substring(0, rIndex);

            rIndex = reqVmPath.LastIndexOf('/');
            string reqDsPath = reqVmPath.Substring(0, rIndex);

            T.Tracer().TraceInformation(
                "ReqDsPath: {0}, path created: {1}.",
                reqDsPath,
                createdDsPath);
            if (createdDsPath == reqDsPath)
            {
                return;
            }

            T.Tracer().TraceInformation("Changing vmPathName to {0}.", reqDsPath);
            this.datastoreOpts.CreatePath(reqDsPath, targetDataCenter);
            this.vmOpts.UnregisterVm(vmView);
            this.datastoreOpts.MoveOrCopyFile(
                DatastoreOpts.MoveOrCopyOp.Move,
                targetDataCenter,
                createdVmPath,
                reqVmPath,
                force: true);
            VirtualMachine registeredVmView = this.vmOpts.RegisterVm(
                displayName,
                reqVmPath,
                vmFolderView,
                tgtResourcePoolView,
                tgtHostView);
        }

        /// <summary>
        /// Information gathered about the VM that was created/updated.
        /// </summary>
        public class CreatedVmInfo
        {
            /// <summary>
            /// Gets or sets the plan name.
            /// </summary>
            public string PlanName { get; set; }

            /// <summary>
            /// Gets or sets the uuid of the VM that was created.
            /// </summary>
            public string Uuid { get; set; }

            /// <summary>
            /// Gets or sets the host name.
            /// </summary>
            public string HostName { get; set; }

            /// <summary>
            /// Gets or sets the file name
            /// </summary>
            public string FileName { get; set; }
        }
    }
}
