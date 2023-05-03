//---------------------------------------------------------------
//  <copyright file="VmOpts.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     28-April-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware VM operations.
    /// </summary>
    public class VmOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// The data store operations helper.
        /// </summary>
        private DatastoreOpts datastoreOpts;

        /// <summary>
        /// The data center operations helper.
        /// </summary>
        private DataCenterOpts dataCenterOpts;

        /// <summary>
        /// Initializes a new instance of the <see cref="VmOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        /// <param name="datastoreOpts">The data store operations helper.</param>
        /// <param name="dataCenterOpts">The data center operations helper.</param>
        public VmOpts(
            VMware.Vim.VimClient client,
            DatastoreOpts datastoreOpts,
            DataCenterOpts dataCenterOpts)
        {
            this.client = client;
            this.datastoreOpts = datastoreOpts;
            this.dataCenterOpts = dataCenterOpts;
        }

        /// <summary>
        /// Delete dummy disk from a machine. This will be called when there is failure while
        /// creating dummy disk. Will delete a dummy disk which has "Created = yes" in dummy disk
        /// information.
        /// </summary>
        /// <param name="mtView">The VM to remove from.</param>
        /// <param name="staleDummyDisks">The disks to remove</param>
        public void DeleteDummyDisk(VirtualMachine mtView, List<VirtualDisk> staleDummyDisks)
        {
            var configSpecs = new List<VirtualDeviceConfigSpec>();
            foreach (VirtualDisk disk in staleDummyDisks)
            {
                VirtualDeviceConfigSpec configSpec = new VirtualDeviceConfigSpec();
                configSpec.Operation = VirtualDeviceConfigSpecOperation.remove;
                configSpec.Device = disk;
                configSpec.FileOperation = VirtualDeviceConfigSpecFileOperation.destroy;
                configSpecs.Add(configSpec);
            }

            T.Tracer().TraceInformation(
                "Deleting all the dummy disks from {0}.",
                mtView.Name);
            this.ReconfigureVm(mtView, configSpecs);
        }

        /// <summary>
        /// Finds all the dummy disks on the master target.
        /// </summary>
        /// <param name="mtView">The master target vm view.</param>
        /// <returns>List of dummy virtual disks attached to the MT.</returns>
        public List<VirtualDisk> GetDummyDiskInfo(VirtualMachine mtView)
        {
            List<VirtualDisk> dummyDisks = new List<VirtualDisk>();
            VirtualDevice[] devices = mtView.Config.Hardware.Device;
            foreach (VirtualDevice device in devices)
            {
                if (device.DeviceInfo.Label.Contains(
                    "Hard Disk",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualDeviceFileBackingInfo backingInfo =
                        (VirtualDeviceFileBackingInfo)device.Backing;
                    int keyFile = device.Key;
                    string diskName = backingInfo.FileName;
                    string diskNameComp = "DummyDisk_";
                    if (keyFile >= 2000 && keyFile <= 2100)
                    {
                        int controllerNumber = device.ControllerKey.Value - 1000;
                        int unitNumber = (keyFile - 2000) % 16;
                        diskNameComp +=
                            controllerNumber.ToString() + unitNumber.ToString() + ".vmdk";
                    }
                    else if (keyFile >= 16000 && keyFile <= 16120)
                    {
                        int controllerNumber = device.ControllerKey.Value - 15000 + 4;
                        int unitNumber = (keyFile - 16000) % 30;
                        diskNameComp +=
                            controllerNumber.ToString() + unitNumber.ToString() + ".vmdk";
                    }

                    if (diskName.Contains(diskNameComp, StringComparison.OrdinalIgnoreCase))
                    {
                        dummyDisks.Add((VirtualDisk)device);
                    }
                }
            }

            return dummyDisks;
        }

        /// <summary>
        /// Collects all the information regarding the Virtual machines on Host parameter passed.
        /// </summary>
        /// <param name="vmViews">The virtual machines.</param>
        /// <param name="hostGroup">The host information.</param>
        /// <param name="vSphereHostName">The host name.</param>
        /// <param name="hostVersion">The host version.</param>
        /// <param name="osType">The OS type.</param>
        /// <param name="dvPortGroupInfo">The port info.</param>
        /// <returns>The list of virtual machines.</returns>
        public List<VirtualMachineInfo> GetVmData(
            List<VirtualMachine> vmViews,
            string hostGroup,
            string vSphereHostName,
            string hostVersion,
            OsType osType,
            List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo)
        {
            List<VirtualMachineInfo> vmList = new List<VirtualMachineInfo>();
            List<string> diskList = this.GetDiskInfo(vmViews, hostGroup);

            foreach (var vmView in vmViews)
            {
                try
                {
                    string hostGroupVm = vmView.Summary.Runtime.Host.Value;
                    string machineName = vmView.Name;
                    if (hostGroup != hostGroupVm)
                    {
                        continue;
                    }

                    if (string.Compare(vmView.Name, "unknown", ignoreCase: true) == 0 ||
                        vmView.Summary.Runtime.ConnectionState !=
                            VirtualMachineConnectionState.connected)
                    {
                        T.Tracer().TraceWarning(
                            "Discovery of machine '{0}' skipped as it is in connectionState: {1}.",
                            machineName,
                            vmView.Summary.Runtime.ConnectionState.ToString());
                        continue;
                    }

                    if (vmView.Summary.Config.Template)
                    {
                        T.Tracer().TraceInformation(
                            "Machine '{0}' is a template.",
                            machineName);
                    }

                    if (osType != OsType.All &&
                        string.IsNullOrEmpty(vmView.Summary.Config.GuestFullName))
                    {
                        T.Tracer().TraceError(
                            "Guest OS type is not defined for machine '{0}'. Ignoring.",
                            machineName);
                        T.Tracer().TraceError(
                            "1. Check whether summary of virtual machine displays " +
                            "GuestOS information.");
                        T.Tracer().TraceError(
                            "2. If it is not displayed click on EditSettings and say OK.");
                        T.Tracer().TraceError(
                            "3. If displayed, report this issue to InMage Support Team.");
                        continue;
                    }

                    if ((osType != OsType.All) &&
                        !vmView.Summary.Config.GuestFullName.Contains(
                            osType.ToString(),
                            StringComparison.OrdinalIgnoreCase))
                    {
                        string operatingSystem = vmView.Summary.Config.GuestFullName;
                        if (osType != OsType.Linux)
                        {
                            T.Tracer().TraceInformation(
                                "Skipped machine '{0}' (OS = {1}).",
                                machineName,
                                operatingSystem);
                            continue;
                        }
                        else if ((osType == OsType.Linux)
                            && !operatingSystem.Contains(
                            "CentOS", StringComparison.OrdinalIgnoreCase))
                        {
                            T.Tracer().TraceInformation(
                                "Skipped machine '{0}' (OS = {1}).",
                                machineName,
                                operatingSystem);
                            continue;
                        }
                    }

                    T.Tracer().TraceInformation(
                        "###Collecting details of virtual machine {0} ###.",
                        vmView.Name);

                    List<string> ipAddresses;
                    List<NetworkData> networkData;
                    this.FindNetworkDetails(
                        vmView,
                        dvPortGroupInfo,
                        out ipAddresses,
                        out networkData);

                    int ideCount;
                    int floppyCount;
                    bool rdm;
                    string cluster;
                    List<DiskData> diskDataList;
                    this.FindDiskDetails(
                        vmView,
                        diskList,
                        out ideCount,
                        out floppyCount,
                        out rdm,
                        out cluster,
                        out diskDataList);

                    vmList.Add(
                        new VirtualMachineInfo(
                            vmView,
                            vSphereHostName,
                            hostVersion,
                            ipAddresses,
                            networkData,
                            ideCount,
                            floppyCount,
                            rdm,
                            cluster,
                            diskDataList));
                }
                catch (VimException ve)
                {
                    T.Tracer().TraceException(
                        ve,
                        "Failed to discover virtual machine {0}.",
                        vmView.Name);

                    if (Debugger.IsAttached)
                    {
                        Debugger.Break();
                    }
                }
                catch (Exception e)
                {
                    T.Tracer().TraceException(
                        e,
                        "Unexpected exception: Failed to discover virtual machine {0}.",
                        vmView.Name);
                }
            }

            return vmList;
        }

        /// <summary>
        /// Gets the view of all the machines in a datacemter.
        /// </summary>
        /// <param name="datacenterView">Datacenter in question.</param>
        /// <returns>List of VMs.</returns>
        public List<VirtualMachine> GetVmViewInDatacenter(Datacenter datacenterView)
        {
            string datacenterName = datacenterView.Name;
            IList<EntityViewBase> vmViews = this.client.FindEntityViews(
                typeof(VirtualMachine),
                beginEntity: datacenterView.MoRef,
                filter: null,
                properties: this.GetVmProps());
            if (vmViews == null || vmViews.Count == 0)
            {
                throw VMwareClientException.NoVirtualMachineFoundUnderDataCenter(datacenterName);
            }

            List<VirtualMachine> vms = new List<VirtualMachine>();
            foreach (var vm in vmViews)
            {
                vms.Add((VirtualMachine)vm);
            }

            return vms;
        }

        /// <summary>
        /// Gets the virtual machine view on the basis of UUID.
        /// </summary>
        /// <param name="uuid">UUID of the machine.</param>
        /// <returns>The vm view.</returns>
        public VirtualMachine GetVmViewByUuid(string uuid)
        {
            IList<EntityViewBase> vmViews = this.client.FindEntityViews(
                typeof(VirtualMachine),
                beginEntity: null,
                filter: new NameValueCollection { { "summary.config.uuid", uuid } },
                properties: this.GetVmProps());
            if (vmViews == null || vmViews.Count == 0)
            {
                throw VMwareClientException.VirtualMachineNotFound(uuid);
            }

            if (vmViews.Count > 1)
            {
                throw VMwareClientException.MultipleVirtualMachinesNotFound(uuid);
            }

            return (VirtualMachine)vmViews[0];
        }

        /// <summary>
        /// Gets VM views on vCenter/vSphere.
        /// </summary>
        /// <returns>The VM views.</returns>
        public IList<VirtualMachine> GetVmViewsByProps()
        {
            IList<EntityViewBase> vmViews =
                this.client.FindEntityViews(
                viewType: typeof(VirtualMachine),
                beginEntity: null,
                filter: null,
                properties: this.GetVmProps());

            if (vmViews == null)
            {
                T.Tracer().TraceError("No VirtualMachines found");
                vmViews = new List<EntityViewBase>();
            }

            List<VirtualMachine> newVmViews = new List<VirtualMachine>();
            foreach (var item in vmViews)
            {
                newVmViews.Add((VirtualMachine)item);
            }

            return newVmViews;
        }

        /// <summary>
        /// Creates a New SATA controller spec.
        /// </summary>
        /// <param name="busNumber">Bus number.</param>
        /// <param name="controllerType">Controller type.</param>
        /// <param name="controllerKey">Controller key.</param>
        /// <returns>The new controller spec.</returns>
        public VirtualDeviceConfigSpec NewSATAControllerSpec(
            int busNumber,
            string controllerType,
            int controllerKey)
        {
            T.Tracer().TraceInformation(
                "Creating a new sata controller for bus number {0}, controllerType = {1}.",
                busNumber,
                controllerType);
            var connectionInfo = new VirtualDeviceConnectInfo
            {
                AllowGuestControl = false,
                Connected = true,
                StartConnected = true
            };

            VirtualSATAController controller;
            switch (controllerType)
            {
                case "VirtualAHCIController":
                    controller = new VirtualAHCIController();
                    break;

                case "VirtualSATAController":
                    controller = new VirtualSATAController();
                    break;

                default:
                    T.Tracer().Assert(
                        false,
                        "Unexpected controller type {0}. Fix this.",
                        controllerType);
                    throw VMwareClientException.UnexpectedSataControllerType(controllerType);
            }

            controller.Key = controllerKey;
            controller.BusNumber = busNumber;
            controller.Connectable = connectionInfo;
            controller.ControllerKey = 100;

            VirtualDeviceConfigSpec controllerSpec = new VirtualDeviceConfigSpec();
            controllerSpec.Device = controller;
            controllerSpec.Operation = VirtualDeviceConfigSpecOperation.add;
            return controllerSpec;
        }

        /// <summary>
        /// Creates a New SCSI controller spec.
        /// </summary>
        /// <param name="busNumber">Bus number.</param>
        /// <param name="sharing">Type of sharing.</param>
        /// <param name="controllerType">Controller type.</param>
        /// <param name="controllerKey">Controller key.</param>
        /// <returns>The new controller spec.</returns>
        public VirtualDeviceConfigSpec NewSCSIControllerSpec(
            int busNumber,
            VirtualSCSISharing sharing,
            string controllerType,
            int controllerKey)
        {
            T.Tracer().TraceInformation(
                "Creating a new scsi controller for bus number {0}, controllerType = {1} in " +
                "{2} basis.",
                busNumber,
                controllerType,
                sharing.ToString());
            var connectionInfo = new VirtualDeviceConnectInfo
            {
                AllowGuestControl = false,
                Connected = true,
                StartConnected = true
            };

            VirtualSCSIController controller;
            switch (controllerType)
            {
                case "VirtualLsiLogicSASController":
                    controller = new VirtualLsiLogicSASController();
                    break;

                case "VirtualLsiLogicController":
                    controller = new VirtualLsiLogicController();
                    break;

                case "VirtualBusLogicController":
                    controller = new VirtualBusLogicController();
                    break;

                case "ParaVirtualSCSIController":
                    controller = new ParaVirtualSCSIController();
                    break;

                case "VirtualSCSIController":
                    controller = new VirtualSCSIController();
                    break;

                default:
                    T.Tracer().Assert(
                        false,
                        "Unexpected controller type {0}. Fix this.",
                        controllerType);
                    throw VMwareClientException.UnexpectedScsiControllerType(controllerType);
            }

            controller.Key = controllerKey;
            controller.BusNumber = busNumber;
            controller.Connectable = connectionInfo;
            controller.ControllerKey = 100;
            controller.SharedBus = sharing;

            VirtualDeviceConfigSpec controllerSpec = new VirtualDeviceConfigSpec();
            controllerSpec.Device = controller;
            controllerSpec.Operation = VirtualDeviceConfigSpecOperation.add;
            return controllerSpec;
        }

        /// <summary>
        /// Reconfigures the virtual machine.
        /// </summary>
        /// <param name="vm">The VM in question.</param>
        /// <param name="configSpecs">The device configuration changes.</param>
        public void ReconfigureVm(VirtualMachine vm, List<VirtualDeviceConfigSpec> configSpecs)
        {
            VirtualMachineConfigSpec vmConfigSpec = new VirtualMachineConfigSpec();
            vmConfigSpec.DeviceChange = configSpecs.ToArray();
            this.ReconfigureVm(vm, vmConfigSpec);
        }

        /// <summary>
        /// Reconfigures the virtual machine.
        /// </summary>
        /// <param name="vm">The VM in question.</param>
        /// <param name="vmConfigSpec">The VM configuration spec.</param>
        public void ReconfigureVm(VirtualMachine vm, VirtualMachineConfigSpec vmConfigSpec)
        {
            bool keepTrying = true;

            // TODO (gsinha): Shall we implement time for 5 minutes?
            while (keepTrying)
            {
                try
                {
                    vm.ReconfigVM(vmConfigSpec);
                    keepTrying = false;
                }
                catch (VimException ve)
                {
                    TaskInProgress tip = ve.MethodFault as TaskInProgress;
                    if (tip != null)
                    {
                        TimeSpan waitTime = TimeSpan.FromSeconds(2);
                        T.Tracer().TraceError(
                            "Waiting for {0} secs as another task '{1}' is in progress on " +
                            "vm {2}.",
                            waitTime.TotalSeconds,
                            tip.Task.Value,
                            vm.Name);
                        System.Threading.Thread.Sleep(waitTime);
                        keepTrying = true;
                    }

                    T.Tracer().TraceException(
                        ve,
                        "Hit an exception while reconfiguring vm: {0} ({1}).",
                        vm.Name,
                        vm.MoRef.Value);
                    throw;
                }
            }
        }

        /// <summary>
        /// Deletes dummy disk from Master Target.
        /// </summary>
        /// <param name="mtView">The master target.</param>
        public void RemoveDummyDisk(VirtualMachine mtView)
        {
            string vmName = mtView.Name;
            List<VirtualDisk> dummyDisks = this.GetDummyDiskInfo(mtView);
            if (dummyDisks.Any())
            {
                this.DeleteDummyDisk(mtView, dummyDisks);
            }

            T.Tracer().TraceInformation(
                "Successfully removed dummy disk from {0}.",
                mtView.Name);
        }

        /// <summary>
        /// Updates virtual machine view.
        /// </summary>
        /// <param name="vmView">The VM view to update.</param>
        public void UpdateVmView(VirtualMachine vmView)
        {
            vmView.UpdateViewData(this.GetVmProps());
        }

        /// <summary>
        /// Powers on Virtual Machine and if a question is raised it will answer as well.
        /// </summary>
        /// <param name="vmView">The VM to power on.</param>
        public void PowerOn(VirtualMachine vmView)
        {
            ManagedObjectReference morHost = vmView.Summary.Runtime.Host;
            string hostName =
                ((HostSystem)this.client.GetView(morHost, new string[] { "name" })).Name;
            ManagedObjectReference taskRef = vmView.PowerOnVM_Task(morHost);
            Task taskView = (Task)this.client.GetView(taskRef, null);
            bool keepWaiting = true;

            try
            {
                while (keepWaiting)
                {
                    TaskInfo info = taskView.Info;
                    if (info.State == TaskInfoState.running)
                    {
                        VirtualMachine entityView =
                            (VirtualMachine)this.client.GetView(info.Entity, null);
                        if (entityView.Runtime.Question != null)
                        {
                            this.AnswerQuestion(entityView);
                            continue;
                        }
                    }
                    else if (info.State == TaskInfoState.success)
                    {
                        T.Tracer().TraceInformation(
                            "Virtual machine {0} under host {1} powered on.",
                            vmView.Name,
                            hostName);
                        break;
                    }
                    else if (info.State == TaskInfoState.error)
                    {
                        string errorMessage = info.Error.LocalizedMessage;
                        throw VMwareClientException.PowerOnFailedOnVm(vmView.Name, errorMessage);
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                    taskView.UpdateViewData();
                }
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is NotSupported)
                {
                    throw VMwareClientException.CannotPowerOnVmMarkedAsTemplate(vmView.Name);
                }

                if (ve.MethodFault is InvalidPowerState)
                {
                    throw VMwareClientException.CannotPowerOnVmInvalidPowerState(vmView.Name);
                }

                if (ve.MethodFault is InvalidState)
                {
                    throw VMwareClientException.CannotPowerOnVmInCurrentState(vmView.Name);
                }

                throw VMwareClientException.CannotPowerOnVm(vmView.Name, ve.ToString());
            }
            catch (Exception e)
            {
                throw VMwareClientException.CannotPowerOnVm(vmView.Name, e.ToString());
            }
        }

        /// <summary>
        /// Powers off the virtual machine.
        /// </summary>
        /// <param name="vmView">The virtual machine to power off.</param>
        /// <param name="expiryTime">Time out of PowerOff call.</param>
        /// <param name="force">Flag for force shutdown.</param>
        public void PowerOff(VirtualMachine vmView, TimeSpan expiryTime, bool force = false)
        {
            ManagedObjectReference morHost = vmView.Summary.Runtime.Host;
            string hostName =
                ((HostSystem)this.client.GetView(morHost, new string[] { "name" })).Name;
            if (vmView.Summary.Guest.ToolsStatus == VirtualMachineToolsStatus.toolsOk)
            {
                this.ShutdownGuest(vmView, hostName, expiryTime);
            }
            else
            {
                this.PowerOffVm(vmView, hostName, expiryTime);
            }
        }

        /// <summary>
        /// Upgrades VMware version.
        /// </summary>
        /// <param name="vmView">The VM to upgrade.</param>
        public void UpgradeVm(VirtualMachine vmView)
        {
            T.Tracer().TraceInformation("Upgrading VM hardware of machine {0}.", vmView.Name);

            ManagedObjectReference taskRef = vmView.UpgradeVM_Task(version: null);
            Task taskView = (Task)this.client.GetView(taskRef, null);
            bool keepWaiting = true;

            while (keepWaiting)
            {
                TaskInfo info = taskView.Info;
                if (info.State == TaskInfoState.running)
                {
                    // Keep waiting.
                }
                else if (info.State == TaskInfoState.success)
                {
                    T.Tracer().TraceInformation(
                        "Upgrading VM hardware for virtual machine {0} succeeded.",
                        vmView.Name);
                    break;
                }
                else if (info.State == TaskInfoState.error)
                {
                    string errorMessage = info.Error.LocalizedMessage;
                    throw VMwareClientException.FailedToUpgradeVm(vmView.Name, errorMessage);
                }

                System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                taskView.UpdateViewData();
            }
        }

        /// <summary>
        /// Creates CD-ROM device configuration.
        /// </summary>
        /// <param name="ideCount">Number of IDE controllers to be added.</param>
        /// <returns>The device config.</returns>
        public VirtualDeviceConfigSpec[] CreateCdRomDeviceSpec(int ideCount)
        {
            List<VirtualDeviceConfigSpec> cdRomDevSpecs = new List<VirtualDeviceConfigSpec>();
            List<int[]> keyInfo = new List<int[]>
            {
                new int[] { 201, 0 },
                new int[] { 200, 0 },
                new int[] { 200, 1 },
                new int[] { 201, 1 }
            };

            for (int i = 0; i < ideCount; i++)
            {
                int controllerKey = keyInfo[i][0];
                int unitNumber = keyInfo[i][1];

                var cdRomBacking = new VirtualCdromRemotePassthroughBackingInfo
                {
                    DeviceName = string.Empty,
                    Exclusive = false
                };

                var connectionInfo = new VirtualDeviceConnectInfo
                {
                    AllowGuestControl = true,
                    Connected = false,
                    StartConnected = false
                };

                var virtualCdRom = new VirtualCdrom
                {
                    Backing = cdRomBacking,
                    Connectable = connectionInfo,
                    ControllerKey = controllerKey,
                    Key = -44,
                    UnitNumber = unitNumber
                };

                var cdRomDevice = new VirtualDeviceConfigSpec
                {
                    Device = virtualCdRom,
                    Operation = VirtualDeviceConfigSpecOperation.add
                };

                cdRomDevSpecs.Add(cdRomDevice);
            }

            return cdRomDevSpecs.ToArray();
        }

        /// <summary>
        /// Creates floppy device specification.
        /// </summary>
        /// <param name="floppyDevCount">The number of floppy devices.</param>
        /// <returns>The device config.</returns>
        public VirtualDeviceConfigSpec[] CreateFloppyDeviceSpec(int floppyDevCount)
        {
            var floppyDeviceSpec = new List<VirtualDeviceConfigSpec>();

            for (int i = 0; i < floppyDevCount; i++)
            {
                int controllerKey = 400;
                int unitNumber = i;

                var floppyDriveBacking = new VirtualFloppyRemoteDeviceBackingInfo
                {
                    DeviceName = string.Empty,
                    UseAutoDetect = false
                };

                var connectionInfo = new VirtualDeviceConnectInfo
                {
                    AllowGuestControl = true,
                    Connected = false,
                    StartConnected = false
                };

                var floppyDrive = new VirtualFloppy
                {
                    Backing = floppyDriveBacking,
                    Connectable = connectionInfo,
                    ControllerKey = controllerKey,
                    Key = -66,
                    UnitNumber = unitNumber
                };

                var floppyDevice = new VirtualDeviceConfigSpec
                {
                    Device = floppyDrive,
                    Operation = VirtualDeviceConfigSpecOperation.add
                };

                floppyDeviceSpec.Add(floppyDevice);
            }

            return floppyDeviceSpec.ToArray();
        }

        /// <summary>
        /// Creates Network adapter configuration.
        /// </summary>
        /// <param name="networkCards">The network cards input.</param>
        /// <param name="isItvCenter">Whether dealing with vCenter.</param>
        /// <param name="displayName">The display name.</param>
        /// <param name="portgroupInfo">The port group info.</param>
        /// <returns>The device config.</returns>
        public VirtualDeviceConfigSpec[] CreateNetworkAdapterSpec(
            List<NicInfo> networkCards,
            bool isItvCenter,
            string displayName,
            List<HostOpts.DistributedVirtualPortgroupInfoEx> portgroupInfo)
        {
            var networkCardsSpec = new List<VirtualDeviceConfigSpec>();
            ////List<rootESXHostNic> networkCards =
            ////    sourceHost.nic.OrderBy(o => o.network_label).ToList();
            int nwAdapterCounter = 1;

            foreach (NicInfo networkAdapter in networkCards)
            {
                string networkName = networkAdapter.network_name;
                int controllerKey = 100;
                string addressType = networkAdapter.address_type;
                string macAddress = networkAdapter.nic_mac;
                string adapterType = networkAdapter.adapter_type;

                if (!string.IsNullOrEmpty(networkAdapter.new_network_name))
                {
                    networkName = networkAdapter.new_network_name;
                }

                string switchUuid = string.Empty;
                string portGroupKey = string.Empty;
                if (networkName.EndsWith("[DVPG]", StringComparison.OrdinalIgnoreCase))
                {
                    foreach (HostOpts.DistributedVirtualPortgroupInfoEx portGroup in portgroupInfo)
                    {
                        if (!isItvCenter && portGroup.DvPortGroupInfo.PortgroupType != "ephemeral")
                        {
                            continue;
                        }

                        if (portGroup.PortGroupNameDVPG != networkName)
                        {
                            continue;
                        }

                        string pgName = portGroup.DvPortGroupInfo.PortgroupName;
                        switchUuid = portGroup.DvPortGroupInfo.SwitchUuid;
                        portGroupKey = portGroup.DvPortGroupInfo.PortgroupKey;
                        T.Tracer().TraceInformation(
                            "{0}, {1}: switch UUID = {2}, port key = {3},",
                            displayName,
                            pgName,
                            switchUuid,
                            portGroupKey);
                        break;
                    }
                }

                VirtualDeviceBackingInfo virtualeCardBackingInfo = null;
                if (switchUuid == string.Empty || portGroupKey == string.Empty)
                {
                    virtualeCardBackingInfo = new VirtualEthernetCardNetworkBackingInfo
                    {
                        DeviceName = networkName,
                        UseAutoDetect = true
                    };
                }
                else
                {
                    var dvpg = new DistributedVirtualSwitchPortConnection
                    {
                        PortgroupKey = portGroupKey,
                        SwitchUuid = switchUuid
                    };

                    virtualeCardBackingInfo = new VirtualEthernetCardDistributedVirtualPortBackingInfo
                    {
                        Port = dvpg
                    };
                }

                var connectionInfo = new VirtualDeviceConnectInfo
                {
                    AllowGuestControl = true,
                    Connected = false,
                    StartConnected = true
                };

                VirtualEthernetCard nicCard = null;
                switch (adapterType)
                {
                    case "VirtualE1000":
                        nicCard = new VirtualE1000();
                        break;

                    case "VirtualE1000e":
                        nicCard = new VirtualE1000e();
                        break;

                    case "VirtualPCNet32":
                        nicCard = new VirtualPCNet32();
                        break;

                    case "VirtualSriovEthernetCard":
                        nicCard = new VirtualSriovEthernetCard();
                        break;

                    case "VirtualVmxnet":
                        nicCard = new VirtualVmxnet();
                        break;

                    default:
                        throw new NotSupportedException(string.Format(
                            "Adapter type {0}. Not supported. Fix this.",
                            adapterType));
                }

                nicCard.Backing = virtualeCardBackingInfo;
                nicCard.Connectable = connectionInfo;
                nicCard.ControllerKey = controllerKey;
                nicCard.Key = -55;
                nicCard.UnitNumber = 1;
                nicCard.AddressType = "generated";
                nicCard.MacAddress = string.Empty;
                nicCard.WakeOnLanEnabled = true;

                var ethernetDevice = new VirtualDeviceConfigSpec
                {
                    Device = nicCard,
                    Operation = VirtualDeviceConfigSpecOperation.add
                };

                networkCardsSpec.Add(ethernetDevice);
                networkAdapter.network_label =
                    string.Format("Network adapter {0}", nwAdapterCounter);
                nwAdapterCounter++;
            }

            return networkCardsSpec.ToArray();
        }

        /// <summary>
        /// Checks whether host name of machine is populated in a span of 3 Mins for the
        /// virtual machine.
        /// </summary>
        /// <param name="uuid">The virtual machine Id.</param>
        public void WaitForMachineBooted(string uuid)
        {
            this.CheckHostName(uuid);
        }

        /// <summary>
        /// Set the configuration parameters into vmx file.
        /// </summary>
        /// <param name="vmView">VM view to which option value has to be set.</param>
        /// <param name="paramName">The parameter name.</param>
        /// <param name="paramValue">The parameter value.</param>
        public void SetVmExtraConfigParam(
            VirtualMachine vmView,
            string paramName,
            string paramValue)
        {
            foreach (OptionValue config in vmView.Config.ExtraConfig)
            {
                if (string.Compare(config.Key, paramName, ignoreCase: true) == 0)
                {
                    if (string.Compare((string)config.Value, paramValue, ignoreCase: true) == 0)
                    {
                        return;
                    }
                }
            }

            T.Tracer().TraceInformation(
                "Setting {0} value as {1} in machine {2}.",
                paramName,
                paramValue,
                vmView.Name);
            var optConf = new OptionValue
            {
                Key = paramName,
                Value = paramValue
            };

            List<OptionValue> optionValues = new List<OptionValue> { optConf };
            this.ReconfigureVm(vmView, optionValues);
        }

        /// <summary>
        /// Adds all controller's to a virtual machine.
        /// </summary>
        /// <param name="vmView">Virtual machine view for which controllers are to be added.
        /// </param>
        public void AddSCSIControllersToVM(VirtualMachine vmView)
        {
            string vmxVersion = vmView.Config.Version;
            int numEthernetCard = vmView.Summary.Config.NumEthernetCards.GetValueOrDefault();

            // Max number of SCSI controllers allowed till now are 4 per vm. But this value
            // differs if vmxVersion < 7.
            int controllerCount = 4;
            if (string.Compare(vmxVersion, "vmx-07", ignoreCase: true) < 0)
            {
                controllerCount = 5 - numEthernetCard;
            }

            if (string.Compare(vmxVersion, "vmx-10", ignoreCase: true) == 0 &&
                vmView.Summary.Config.GuestFullName.Contains(
                    "Windows",
                    StringComparison.OrdinalIgnoreCase))
            {
                controllerCount = 8; // Including SATA controllers.
            }

            var controllerType = new Dictionary<string, string>();
            var controllerInfo = new Dictionary<int, string>();
            controllerType["scsi"] = "VirtualLsiLogicController";
            controllerType["sata"] = "VirtualSATAController";
            foreach (VirtualDevice device in vmView.Config.Hardware.Device)
            {
                if (device.DeviceInfo.Label.Contains(
                    "SCSI controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    controllerType["scsi"] = device.GetType().Name;
                    controllerCount--;
                    controllerInfo[device.Key] = device.GetType().Name;
                }
                else if (device.DeviceInfo.Label.Contains(
                    "SATA controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    controllerType["sata"] = device.GetType().Name;
                    controllerCount--;
                    controllerInfo[device.Key] = device.GetType().Name;
                }
            }

            if (controllerCount == 0)
            {
                T.Tracer().TraceInformation(
                    "SCSI controllers are already added to master target.");
                return;
            }

            var deviceChange = new List<VirtualDeviceConfigSpec>();
            for (int i = 0; i < 4 && controllerCount != 0; i++)
            {
                int key = 1000 + i;
                if (controllerInfo.ContainsKey(key))
                {
                    continue;
                }

                T.Tracer().TraceInformation(
                    "Constructing new SCSI Controller specification : busNumber => {0}, " +
                    "controllerType => {1} , controllerKey => {2}.",
                    i,
                    controllerType["scsi"],
                    key);
                VirtualDeviceConfigSpec scsiControllerSpec = this.NewSCSIControllerSpec(
                    i,
                    VirtualSCSISharing.noSharing,
                    controllerType["scsi"],
                    key);
                deviceChange.Add(scsiControllerSpec);
            }

            if (string.Compare(vmxVersion, "vmx-10", ignoreCase: true) == 0 &&
            vmView.Summary.Config.GuestFullName.Contains(
                "Windows",
                StringComparison.OrdinalIgnoreCase))
            {
                for (int i = 0; i < 4 && controllerCount != 0; i++)
                {
                    int key = 15000 + i;
                    if (controllerInfo.ContainsKey(key))
                    {
                        continue;
                    }

                    T.Tracer().TraceInformation(
                        "Constructing new SATA Controller specification : busNumber => {0}, " +
                        "controllerType => {1} , controllerKey => {2}.",
                        i,
                        controllerType["sata"],
                        key);
                    VirtualDeviceConfigSpec sataControllerSpec = this.NewSATAControllerSpec(
                        i,
                        controllerType["sata"],
                        key);
                    deviceChange.Add(sataControllerSpec);
                }
            }

            this.ReconfigureVm(vmView, deviceChange);
        }

        /// <summary>
        /// Registers a virtual machine to the inventory.
        /// </summary>
        /// <param name="vmName">The VM name.</param>
        /// <param name="vmPathName">The VM path name.</param>
        /// <param name="folderView">The VM folder view.</param>
        /// <param name="resourcePoolView">The resource pool view.</param>
        /// <param name="hostView">The host view.</param>
        /// <returns>Registered VM view.</returns>
        public VirtualMachine RegisterVm(
            string vmName,
            string vmPathName,
            Folder folderView,
            ResourcePool resourcePoolView,
            HostSystem hostView)
        {
            VirtualMachine registeredView = null;
            try
            {
                ManagedObjectReference task = folderView.RegisterVM_Task(
                    vmPathName,
                    vmName,
                    false, // asTemplate
                    resourcePoolView.MoRef,
                    hostView.MoRef);
                Task taskView = (Task)this.client.GetView(task, null);
                bool keepWaiting = true;

                while (keepWaiting)
                {
                    TaskInfo info = taskView.Info;
                    if (info.State == TaskInfoState.running)
                    {
                        T.Tracer().TraceInformation(
                            "Registering of machine {0} in progress.",
                            vmName);
                    }
                    else if (info.State == TaskInfoState.success)
                    {
                        T.Tracer().TraceInformation(
                            "Virtual machine {0} is registered to vSphere.",
                            vmName);
                        registeredView = (VirtualMachine)this.client.GetView(
                            (ManagedObjectReference)info.Result,
                            null);
                        break;
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                    taskView.UpdateViewData();
                }
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is AlreadyExists)
                {
                    throw VMwareClientException.AlreadyExists();
                }

                if (ve.MethodFault is OutOfBounds)
                {
                    throw VMwareClientException.MaximumNumberOfVmsExceeded();
                }

                if (ve.MethodFault is InvalidArgument)
                {
                    throw VMwareClientException.InvalidArgument();
                }

                if (ve.MethodFault is DatacenterMismatch)
                {
                    throw VMwareClientException.DataCenterMismatch();
                }

                if (ve.MethodFault is InvalidDatastore)
                {
                    throw VMwareClientException.InvalidDatastore(vmPathName);
                }

                if (ve.MethodFault is NotSupported)
                {
                    throw VMwareClientException.NotSupported();
                }

                if (ve.MethodFault is InvalidState)
                {
                    throw VMwareClientException.OperationNotAllowedInCurrentState();
                }

                throw;
            }

            T.Tracer().TraceInformation("Successfully registered {0} to the inventory.", vmName);
            return registeredView;
        }

        /// <summary>
        /// Unregisters a machine from ESX/vSphere.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        public void UnregisterVm(VirtualMachine vmView)
        {
            try
            {
                vmView.UnregisterVM();
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is InvalidPowerState)
                {
                    throw VMwareClientException.VmMustBePoweredOff(vmView.Name);
                }

                if (ve.MethodFault is HostNotConnected)
                {
                    throw VMwareClientException.UnableToCommunicateWithHost(vmView.Name);
                }

                throw;
            }
        }

        /// <summary>
        /// Find the disk information. Disk name, size, disk type and it's mode etc.
        /// </summary>
        /// <param name="vmView">The virtual machine.</param>
        /// <param name="diskList">The VMDK uuids.</param>
        /// <param name="ideCount">The IDE count.</param>
        /// <param name="floppyCount">The floppy count.</param>
        /// <param name="rdm">A true/false value.</param>
        /// <param name="cluster">"yes" or "no".</param>
        /// <param name="diskDataList">The disk data.</param>
        public void FindDiskDetails(
            VirtualMachine vmView,
            List<string> diskList,
            out int ideCount,
            out int floppyCount,
            out bool rdm,
            out string cluster,
            out List<DiskData> diskDataList)
        {
            rdm = false;
            cluster = "no";
            var mapOfKeyAndLabel = new Dictionary<int, string>();
            var mapControllerKeyType = new Dictionary<int, string>();
            var mapOfKeyAndMode = new Dictionary<int, string>();
            ideCount = 0;
            floppyCount = 0;
            diskDataList = new List<DiskData>();

            VirtualDevice[] devices = vmView.Config.Hardware.Device;
            string vmPathName = vmView.Summary.Config.VmPathName;
            foreach (VirtualDevice device in devices)
            {
                if (device.DeviceInfo.Label.Contains(
                    "SCSI controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualSCSIController controller = device as VirtualSCSIController;
                    if (controller.ScsiCtlrUnitNumber.HasValue)
                    {
                        mapOfKeyAndLabel.Add(controller.Key, controller.DeviceInfo.Label);
                        mapControllerKeyType.Add(controller.Key, device.GetType().Name);
                        mapOfKeyAndMode.Add(controller.Key, controller.SharedBus.ToString());
                    }
                }
                else if (device.DeviceInfo.Label.Contains(
                    "SATA controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualSATAController controller = device as VirtualSATAController;
                    mapOfKeyAndLabel.Add(controller.Key, controller.DeviceInfo.Label);
                    mapControllerKeyType.Add(controller.Key, device.GetType().Name);
                }
                else if (Regex.IsMatch(
                    device.DeviceInfo.Label,
                    @"^IDE [\d]$",  // "Eg: IDE 3"
                    RegexOptions.IgnoreCase))
                {
                    VirtualIDEController controller = device as VirtualIDEController;
                    if (controller.Device != null)
                    {
                        // TODO: (gsinha) Is the correct translation of the Perl code?
                        ideCount += controller.Device.Count();
                        mapControllerKeyType.Add(controller.Key, device.GetType().Name);
                    }
                }
                else if (device.DeviceInfo.Label.Contains(
                    "SIO controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    floppyCount++;
                }
            }

            foreach (VirtualDevice device in devices)
            {
                if (device.DeviceInfo.Label.Contains(
                    "Hard Disk",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualDeviceFileBackingInfo backingInfo =
                        (VirtualDeviceFileBackingInfo)device.Backing;
                    int keyFile = device.Key;
                    string diskName = backingInfo.FileName;
                    if (vmView.Snapshot != null)
                    {
                        VirtualMachineFileLayoutDiskLayout[] diskLayoutInfo = vmView.Layout.Disk;
                        foreach (var diskLayout in diskLayoutInfo)
                        {
                            if (diskLayout.Key != keyFile)
                            {
                                continue;
                            }

                            foreach (string diskFile in diskLayout.DiskFile)
                            {
                                // Eg:"something-123456.vmdk"
                                if (Regex.IsMatch(diskFile, @"-[0-9]{6}\.vmdk$"))
                                {
                                    continue;
                                }

                                diskName = diskFile;
                            }
                        }
                    }

                    int finalSlashVmPathName = vmPathName.LastIndexOf("/");
                    int finalSlashDiskName = diskName.LastIndexOf("/");
                    string vmDatastorePath = vmPathName.Substring(0, finalSlashVmPathName);
                    string diskDatastorePath = diskName.Substring(0, finalSlashDiskName);

                    DiskData diskData = new DiskData();
                    VirtualDisk vDisk = (VirtualDisk)device;
                    long fileSize = vDisk.CapacityInKB;
                    if (backingInfo is VirtualDiskRawDiskMappingVer1BackingInfo)
                    {
                        var rdmBackingInfo =
                            backingInfo as VirtualDiskRawDiskMappingVer1BackingInfo;
                        rdm = true;
                        diskData.DiskType = rdmBackingInfo.CompatibilityMode;
                        diskData.DiskMode = "Mapped RAW LUN";
                        diskData.IndependentPersistent = VirtualDiskMode.persistent.ToString();
                        diskData.DiskUuid = rdmBackingInfo.LunUuid;

                        if (string.Compare(
                            rdmBackingInfo.CompatibilityMode,
                            "virtualmode",
                            ignoreCase: true) == 0)
                        {
                            diskData.DiskUuid = rdmBackingInfo.Uuid;
                        }

                        if (rdmBackingInfo.DiskMode.Contains(
                            "independent_persistent",
                            StringComparison.OrdinalIgnoreCase))
                        {
                            diskData.IndependentPersistent =
                                VirtualDiskMode.independent_persistent.ToString();
                        }
                    }
                    else
                    {
                        string diskMode =
                            (string)device.Backing.GetType()
                            .GetProperty("DiskMode").GetValue(device.Backing);
                        string uuid =
                            (string)device.Backing.GetType()
                            .GetProperty("Uuid").GetValue(device.Backing);
                        if (diskMode.Contains(
                                VirtualDiskMode.independent_nonpersistent.ToString(),
                                StringComparison.OrdinalIgnoreCase) ||
                            diskMode.Contains(
                                VirtualDiskMode.independent_persistent.ToString(),
                                StringComparison.OrdinalIgnoreCase))
                        {
                            diskData.DiskType = diskMode;
                            diskData.DiskMode = "Virtual Disk";
                            diskData.IndependentPersistent =
                                VirtualDiskMode.independent_persistent.ToString();
                            diskData.DiskUuid = uuid;
                        }
                        else
                        {
                            diskData.DiskType = VirtualDiskMode.persistent.ToString();
                            diskData.DiskMode = "Virtual Disk";
                            diskData.IndependentPersistent = VirtualDiskMode.persistent.ToString();
                            diskData.DiskUuid = uuid;
                        }
                    }

                    string scsiControllerNumber = string.Empty;
                    int controllerKey = device.ControllerKey.Value;

                    if (keyFile >= 2000 && keyFile <= 2100)
                    {
                        double dblKeyFile = (keyFile - 2000) / 16;
                        int controllerNumber = (int)Math.Floor(dblKeyFile);
                        int unitNumber = (keyFile - 2000) % 16;
                        scsiControllerNumber = controllerNumber + ":" + unitNumber;
                        diskData.ScsiVmx = scsiControllerNumber;
                        diskData.IdeOrScsi = "scsi";
                        diskData.ControllerType = mapControllerKeyType[controllerKey];
                    }
                    else if (keyFile >= 16000 && keyFile <= 16120)
                    {
                        double dblKeyFile = (keyFile - 16000) / 30;
                        int controllerNumber = (int)Math.Floor(dblKeyFile) + 4;
                        int unitNumber = (keyFile - 16000) % 30;
                        scsiControllerNumber = controllerNumber + ":" + unitNumber;
                        diskData.ScsiVmx = scsiControllerNumber;
                        diskData.IdeOrScsi = "sata";
                        diskData.ControllerType = mapControllerKeyType[controllerKey];
                    }
                    else if (keyFile >= 3000 && keyFile <= 3100)
                    {
                        double dblKeyFile = (keyFile - 3000) / 16;
                        int controllerNumber = (int)Math.Floor(dblKeyFile);
                        int unitNumber = (keyFile - 3000) % 16;
                        scsiControllerNumber = controllerNumber + ":" + unitNumber;
                        diskData.ScsiVmx = scsiControllerNumber;
                        diskData.IdeOrScsi = "ide";
                        diskData.ControllerType = mapControllerKeyType[controllerKey];
                    }

                    if (mapOfKeyAndLabel.ContainsKey(controllerKey))
                    {
                        string scsiControllerLabel = mapOfKeyAndLabel[controllerKey];
                        int scsiVal = int.Parse(scsiControllerLabel.Last().ToString());
                        if (scsiControllerLabel.Contains(
                            "SATA",
                            StringComparison.OrdinalIgnoreCase))
                        {
                            scsiVal += 4;
                        }

                        diskData.ScsiHost = scsiControllerNumber;
                        if (!scsiControllerNumber.StartsWith(scsiVal.ToString()))
                        {
                            string[] scsiControllerSplit =
                                scsiControllerNumber.Split(
                                    new string[] { ":" },
                                    StringSplitOptions.RemoveEmptyEntries);
                            scsiControllerNumber = scsiVal + ":" + scsiControllerNumber[1];
                            diskData.ScsiHost = scsiControllerNumber;
                        }
                    }

                    diskData.ClusterDisk = "no";
                    if (mapOfKeyAndMode.ContainsKey(controllerKey))
                    {
                        if (string.Compare(
                            mapOfKeyAndMode[controllerKey],
                            "virtualSharing",
                            ignoreCase: true) == 0 ||
                            string.Compare(
                            mapOfKeyAndMode[controllerKey],
                            "physicalSharing",
                            ignoreCase: true) == 0)
                        {
                            diskData.ClusterDisk = "yes";
                        }
                    }

                    if (cluster != "yes")
                    {
                        if (diskList.Find(item => item == diskData.DiskUuid) != null)
                        {
                            if (diskData.ClusterDisk == "yes")
                            {
                                cluster = "yes";
                            }
                        }
                    }

                    if (cluster == "yes" && diskData.ClusterDisk == "yes")
                    {
                        string vmdkName =
                            diskName.Substring(
                            finalSlashDiskName,
                            diskName.Length - finalSlashDiskName - 1);
                        int dotIndex = vmdkName.LastIndexOf(".");
                        string vmdkFileName = vmdkName.Substring(0, dotIndex);
                        diskName = vmDatastorePath + vmdkFileName + ".vmdk";
                    }
                    else if (vmDatastorePath != diskDatastorePath)
                    {
                        string vmdkName =
                            diskName.Substring(
                            finalSlashDiskName,
                            diskName.Length - finalSlashDiskName - 1);
                        int dotIndex = vmdkName.LastIndexOf(".");
                        string vmdkFileName = vmdkName.Substring(0, dotIndex);
                        diskName =
                            vmDatastorePath + vmdkFileName + "_InMage_NC_" + keyFile + ".vmdk";
                    }

                    diskData.DiskName = diskName;
                    diskData.DiskSize = fileSize;

                    if (mapOfKeyAndMode.ContainsKey(controllerKey))
                    {
                        diskData.ControllerMode = mapOfKeyAndMode[controllerKey];
                    }
                    else
                    {
                        T.Tracer().TraceWarning(
                            "No controllerMode for disk: {0}.",
                            diskName);
                    }

                    diskData.DiskObjectId = vDisk.DiskObjectId;
                    diskData.CapacityInBytes = vDisk.CapacityInBytes;

                    // All done. Add it.
                    diskDataList.Add(diskData);
                }
            }
        }

        /// <summary>
        /// Checks if the VM is having controller of different types. having different controllers
        /// is a problem at booting time.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <returns>True if VM is having different controllers, false otherwise.</returns>
        public bool IsVmHavingDifferentControllerTypes(VirtualMachine vmView)
        {
            VirtualDevice[] deviceInfo = vmView.Config.Hardware.Device;
            bool differentCount = false;
            string controller_name = string.Empty;
            string vmName = vmView.Name;
            foreach (VirtualDevice device in deviceInfo)
            {
                VirtualSCSIController disk = device as VirtualSCSIController;
                if (disk != null && disk.ScsiCtlrUnitNumber.HasValue)
                {
                    if (string.IsNullOrEmpty(controller_name))
                    {
                        controller_name = device.DeviceInfo.Summary;
                    }
                    else
                    {
                        if (controller_name != device.DeviceInfo.Summary)
                        {
                            differentCount = true;
                            T.Tracer().TraceInformation(
                                "Multiple controller types exist in machine '{0}'.", vmName);
                            break;
                        }
                    }
                }
            }

            return differentCount;
        }

        /// <summary>
        /// Gets the VM ScsiControllerName.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <returns>The SCSI controller name.</returns>
        public string GetVmScsiControllerName(VirtualMachine vmView)
        {
            string scsiControllerName = string.Empty;
            VirtualDevice[] devices = vmView.Config.Hardware.Device;
            foreach (VirtualDevice device_temp in devices)
            {
                if (device_temp.DeviceInfo.Label.Contains(
                    "SCSI controller",
                    StringComparison.OrdinalIgnoreCase))
                {
                    scsiControllerName = device_temp.GetType().Name;
                    break;
                }
            }

            return scsiControllerName;
        }

        /// <summary>
        /// Gets virtual machine view under a specified datacenter with specified name.
        /// </summary>
        /// <param name="machineName">The machine name.</param>
        /// <param name="datacenterView">The datacenter view.</param>
        /// <returns>The virtual machine.</returns>
        public VirtualMachine GetVmViewByNameInDatacenter(
            string machineName,
            Datacenter datacenterView)
        {
            string dcName = datacenterView.Name;
            NameValueCollection filter = new NameValueCollection();
            filter.Add("name", machineName);
            List<EntityViewBase> vmViews = this.client.FindEntityViews(
                typeof(VirtualMachine),
                datacenterView.MoRef,
                filter,
                this.GetVmProps());

            if (vmViews == null)
            {
                throw VMwareClientException.VirtualMachineNotFoundInDatacenter(
                    machineName,
                    datacenterView.Name);
            }

            if (vmViews.Count > 1)
            {
                throw VMwareClientException.MultipleVirtualMachinesFoundInDataCenter(
                    machineName,
                    datacenterView.Name);
            }

            return (VirtualMachine)vmViews[0];
        }

        /// <summary>
        /// Gets virtual machine view under a specified datacenter with specified vmPathName.
        /// </summary>
        /// <param name="vmPathName">The VM path name.</param>
        /// <param name="datacenterView">The datacenter view.</param>
        /// <returns>The virtual machine.</returns>
        public VirtualMachine GetVmViewByVmPathNameInDatacenter(string vmPathName, Datacenter datacenterView)
        {
            string dcName = datacenterView.Name;

            List<EntityViewBase> vmViews = this.client.FindEntityViews(
                typeof(VirtualMachine),
                datacenterView.MoRef,
                filter: new NameValueCollection { { "summary.config.vmPathName", vmPathName } },
                properties: this.GetVmProps());

            if (vmViews == null)
            {
                throw VMwareClientException.VirtualMachineWithPathNameNotFoundInDatacenter(
                    vmPathName,
                    datacenterView.Name);
            }

            if (vmViews.Count > 1)
            {
                throw VMwareClientException.MultipleVirtualMachinesWithPathNameFoundInDataCenter(
                    vmPathName,
                    datacenterView.Name);
            }

            return (VirtualMachine)vmViews[0];
        }

        /// <summary>
        /// Compares VMDK of the machine with the information we have and maps to the VM.
        /// This is used to find machine when the UUID is blank or if we have multiple machines
        /// with same UUID.
        /// </summary>
        /// <param name="disksInfo">The disk info.</param>
        /// <param name="vmView">The VM view.</param>
        /// <param name="drFolderPath">DR folder path.</param>
        /// <param name="vSanFolderPath">VSan folder path.</param>
        /// <returns>True if a comparison is found, false otherwise.</returns>
        public bool CompareVmdk(
            List<DiskInfo> disksInfo,
            VirtualMachine vmView,
            string drFolderPath,
            string vSanFolderPath)
        {
            string machineName = vmView.Name;
            string vmPathName = vmView.Summary.Config.VmPathName;
            bool uuidComparison = false;
            List<string> vmdkFound = new List<string>();

            foreach (DiskInfo vmdk in disksInfo)
            {
                string diskUuid = vmdk.target_disk_uuid;
                if (!string.IsNullOrEmpty(diskUuid))
                {
                    T.Tracer().TraceInformation(
                        "Proceeding with disk uuid comparison with uuid {0}.",
                        diskUuid);
                    uuidComparison = true;
                    break;
                }
            }

            foreach (DiskInfo vmdk in disksInfo)
            {
                bool found = false;
                string vmdkName = vmdk.disk_name;
                string[] vmdkNameXml = vmdkName.Split(
                    new string[] { "[", "]" },
                    StringSplitOptions.RemoveEmptyEntries);
                string vSanFileName = string.Empty;
                if (!string.IsNullOrEmpty(drFolderPath))
                {
                    T.Tracer().TraceInformation("DR folder path: {0}", drFolderPath);
                    vmdkNameXml[vmdkNameXml.Length - 1] = vmdkNameXml.Last().TrimStart();
                    vmdkNameXml[vmdkNameXml.Length - 1] =
                        " " + drFolderPath + "/" + vmdkNameXml[vmdkNameXml.Length - 1];
                }

                if (!string.IsNullOrEmpty(vSanFolderPath))
                {
                    T.Tracer().TraceInformation("vSan folder path: {0}", vSanFolderPath);
                    string[] diskPath = vmdkName.Split(
                       new string[] { "/" },
                       StringSplitOptions.RemoveEmptyEntries);
                    vSanFileName = " " + vSanFolderPath + " . " + diskPath.Last();
                }

                VirtualDevice[] devices = vmView.Config.Hardware.Device;
                string diskUuid = vmdk.target_disk_uuid;
                if (uuidComparison)
                {
                    if (!string.IsNullOrEmpty(diskUuid))
                    {
                        foreach (VirtualDevice device in devices)
                        {
                            if (device.DeviceInfo.Label.Contains(
                                "Hard Disk",
                                StringComparison.OrdinalIgnoreCase))
                            {
                                string vmdkUuid =
                                    (string)device.Backing.GetType()
                                    .GetProperty("Uuid").GetValue(device.Backing);
                                PropertyInfo compatModeProperty =
                                    device.Backing.GetType().GetProperty("CompatibilityMode");
                                if (compatModeProperty != null)
                                {
                                    string compatabilityMode =
                                        (string)compatModeProperty.GetValue(device.Backing);
                                    if (string.Compare(
                                        compatabilityMode,
                                        "physicalmode",
                                        ignoreCase: true) == 0)
                                    {
                                        vmdkUuid =
                                            (string)device.Backing.GetType().GetProperty("LunUuid")
                                            .GetValue(device.Backing);
                                    }
                                }

                                if (vmdkUuid == diskUuid)
                                {
                                    found = true;
                                    var fbo = (VirtualDeviceFileBackingInfo)device.Backing;
                                    string diskName = fbo.FileName;
                                    string[] vmdkNameTarget = diskName.Split(
                                        new string[] { "[", "]" },
                                        StringSplitOptions.RemoveEmptyEntries);
                                    T.Tracer().TraceInformation(
                                        "Found disk {0} based on uuid.",
                                        diskName);

                                    vmdk.datastore_selected = vmdkNameTarget[0];
                                    vmdkFound.Add(vmdkName);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        T.Tracer().TraceInformation("Uuid not found for disk {0}", vmdkName);
                    }
                }
                else
                {
                    foreach (VirtualDevice device in devices)
                    {
                        if (device.DeviceInfo.Label.Contains(
                            "Hard Disk",
                            StringComparison.OrdinalIgnoreCase))
                        {
                            VirtualDeviceFileBackingInfo backingInfo =
                                (VirtualDeviceFileBackingInfo)device.Backing;
                            int keyFile = device.Key;
                            string diskName = backingInfo.FileName;
                            if (vmView.Snapshot != null)
                            {
                                VirtualMachineFileLayoutDiskLayout[] diskLayoutInfo =
                                    vmView.Layout.Disk;
                                foreach (var dlf in diskLayoutInfo)
                                {
                                    if (dlf.Key != keyFile)
                                    {
                                        continue;
                                    }

                                    string[] baseDiskInfo = dlf.DiskFile;
                                    foreach (var disk in baseDiskInfo)
                                    {
                                        // Eg:"something-123456.vmdk"
                                        if (Regex.IsMatch(disk, @"-[0-9]{6}\.vmdk$"))
                                        {
                                            continue;
                                        }

                                        diskName = disk;
                                        T.Tracer().TraceInformation(
                                            "Base disk information collected for key {0} = {1}",
                                            keyFile,
                                            diskName);
                                    }
                                }
                            }

                            string[] vmdkNameTarget = diskName.Split(
                                new string[] { "[", "]" },
                                StringSplitOptions.RemoveEmptyEntries);

                            // TODO: (priyanp)
                            ////if ( !exists $fileNames{$diskName} )
                            ////{
                            ////    $fileNames{$diskName}= $machineName;
                            ////}

                            vmdkNameXml[vmdkNameXml.Length - 1] =
                                Regex.Replace(vmdkNameXml.Last(), "s/_InMage_NC_(\\d)+", string.Empty);
                            vmdkNameTarget[vmdkNameTarget.Length - 1] =
                                Regex.Replace(vmdkNameTarget.Last(), "s/_InMage_NC_(\\d)+", string.Empty);
                            vSanFileName = Regex.Replace(vSanFileName, "s/_InMage_NC_(\\d)+", string.Empty);
                            T.Tracer().TraceInformation(
                                "Comparing {0} or {1} and {2}",
                                vmdkNameXml.Last(),
                                vSanFileName,
                                vmdkNameTarget.Last());
                            if ((vmdkNameXml.Last() == vmdkNameTarget.Last()) ||
                                (vSanFileName == vmdkNameTarget.Last()))
                            {
                                found = true;
                                vmdk.datastore_selected = vmdkNameTarget.Last();
                                vmdkFound.Add(vmdkName);
                                break;
                            }
                        }
                    }
                }
            }

            if (vmdkFound.Count == 0)
            {
                return false;
            }

            return true;
        }

        /// <summary>
        /// Updates virtual machines which is already existing. Things which needs a check are:
        /// 1. Display Name of Virtual Machine.
        /// 2. RAM and CPU configuration.
        /// 3. Number of IDE devices, Floppy Devices.
        /// 4. Virtual Network Cards.
        /// </summary>
        /// <param name="vmView">The virtual machine view.</param>
        /// <param name="sourceHost">The input to use.</param>
        /// <param name="isItvCenter">Whether dealing with vCenter.</param>
        /// <param name="dvPortGroupInfo">The port group info.</param>
        public void UpdateVm(
            VirtualMachine vmView,
            VmUpdateInfo sourceHost,
            bool isItvCenter,
            List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo)
        {
            var vmConfigSpec = new VirtualMachineConfigSpec();
            string displayName = sourceHost.display_name;
            if (!string.IsNullOrEmpty(displayName) && displayName != vmView.Name)
            {
                vmConfigSpec.Name = displayName;
            }

            var fileInfo = new VirtualMachineFileInfo();
            fileInfo.VmPathName = vmView.Summary.Config.VmPathName;
            int rIndex = vmView.Summary.Config.VmPathName.LastIndexOf('/');
            string createdDsPath = vmView.Summary.Config.VmPathName.Substring(0, rIndex + 1);
            fileInfo.LogDirectory = createdDsPath;
            fileInfo.SnapshotDirectory = createdDsPath;
            fileInfo.SuspendDirectory = createdDsPath;
            vmConfigSpec.Files = fileInfo;
            T.Tracer().TraceInformation("Setting log directory to {0}.", createdDsPath);

            if (!string.IsNullOrEmpty(sourceHost.memsize) &&
                vmView.Summary.Config.MemorySizeMB != int.Parse(sourceHost.memsize))
            {
                vmConfigSpec.MemoryMB = int.Parse(sourceHost.memsize);
            }

            if (!string.IsNullOrEmpty(sourceHost.cpucount) &&
                vmView.Summary.Config.NumCpu != int.Parse(sourceHost.cpucount))
            {
                vmConfigSpec.NumCPUs = int.Parse(sourceHost.cpucount);
                if (new Version(this.client.ServiceContent.About.ApiVersion) >= new Version(5, 0))
                {
                    vmConfigSpec.NumCoresPerSocket = 1;
                }
            }

            var deviceChanges = new List<VirtualDeviceConfigSpec>();
            VirtualDeviceConfigSpec[] cdRomDeviceSpec =
                this.CreateCdRomDeviceSpec(int.Parse(sourceHost.ide_count));
            deviceChanges.AddRange(cdRomDeviceSpec);
            VirtualDeviceConfigSpec[] floppyDeviceSpec =
                this.CreateFloppyDeviceSpec(sourceHost.floppy_device_count);
            deviceChanges.AddRange(floppyDeviceSpec);
            if (sourceHost.nic_info != null)
            {
                VirtualDeviceConfigSpec[] networkDeviceSpec = this.CreateNetworkAdapterSpec(
                    sourceHost.nic_info,
                    isItvCenter,
                    vmView.Name,
                    dvPortGroupInfo);
                deviceChanges.AddRange(networkDeviceSpec);
            }

            vmConfigSpec.DeviceChange = deviceChanges.ToArray();
            if (!string.IsNullOrEmpty(sourceHost.diskenableuuid) &&
                sourceHost.diskenableuuid != "na")
            {
                var optConf = new OptionValue
                {
                    Key = "disk.enableuuid",
                    Value = sourceHost.diskenableuuid == "yes" ? "true" : "false"
                };

                var optionValues = new List<OptionValue> { optConf };
                vmConfigSpec.ExtraConfig = optionValues.ToArray();
            }

            for (int i = 0;; i++)
            {
                try
                {
                    vmView.ReconfigVM(vmConfigSpec);
                    break;
                }
                catch (VimException ve)
                {
                    if (ve.MethodFault is TaskInProgress)
                    {
                        T.Tracer().TraceInformation(
                            "Waiting 2 seconds as another task is in progress.");
                        System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                        continue;
                    }

                    throw;
                }
            }
        }

        /// <summary>
        /// Removes CDROM, floppy and Network adapters from a virtual machine.
        /// </summary>
        /// <param name="vmView">Virtual machine view.</param>
        /// <param name="usingExistingVm">Whether existing replica VM is being updated. If so then
        /// some properties won't be touched.</param>
        public void RemoveDevices(VirtualMachine vmView, bool usingExistingVm)
        {
            if (vmView.Config.Hardware.Device == null)
            {
                return;
            }

            var deviceChanges = new List<VirtualDeviceConfigSpec>();
            foreach (VirtualDevice device in vmView.Config.Hardware.Device)
            {
                // For seeded replica VM case we don't want to touch the VM's NW adapter
                // configuration.
                if (!usingExistingVm && device.DeviceInfo.Label.StartsWith(
                    "Network adapter",
                    StringComparison.OrdinalIgnoreCase))
                {
                    var virtualEthernetCard = new VirtualEthernetCardNetworkBackingInfo
                    {
                        DeviceName = string.Empty,
                        UseAutoDetect = true
                    };

                    VirtualEthernetCard nicCard =
                        (VirtualEthernetCard)Activator.CreateInstance(device.GetType());
                    nicCard.Backing = virtualEthernetCard;
                    nicCard.ControllerKey = device.ControllerKey;
                    nicCard.Key = device.Key;
                    nicCard.UnitNumber = device.UnitNumber;
                    var ethernetDevice = new VirtualDeviceConfigSpec
                    {
                        Device = nicCard,
                        Operation = VirtualDeviceConfigSpecOperation.remove
                    };
                    deviceChanges.Add(ethernetDevice);
                }
                else if (device is VirtualCdrom)
                {
                    var cdRomBacking = new VirtualCdromRemotePassthroughBackingInfo
                    {
                        DeviceName = string.Empty,
                        Exclusive = false
                    };

                    var virtualCdRom = new VirtualCdrom
                    {
                        Backing = cdRomBacking,
                        ControllerKey = device.ControllerKey,
                        Key = device.Key,
                        UnitNumber = device.UnitNumber
                    };

                    var cdRomDevice = new VirtualDeviceConfigSpec
                    {
                        Device = virtualCdRom,
                        Operation = VirtualDeviceConfigSpecOperation.remove
                    };
                    deviceChanges.Add(cdRomDevice);
                }
                else if (device is VirtualFloppy)
                {
                    var floppyDriveBacking = new VirtualFloppyRemoteDeviceBackingInfo
                    {
                        DeviceName = string.Empty,
                        UseAutoDetect = false
                    };

                    var floppyDrive = new VirtualFloppy
                    {
                        Backing = floppyDriveBacking,
                        ControllerKey = device.ControllerKey,
                        Key = device.Key,
                        UnitNumber = device.UnitNumber
                    };

                    var floppyDevice = new VirtualDeviceConfigSpec
                    {
                        Device = floppyDrive,
                        Operation = VirtualDeviceConfigSpecOperation.remove
                    };
                    deviceChanges.Add(floppyDevice);
                }
            }

            if (deviceChanges.Any())
            {
                this.ReconfigureVm(vmView, deviceChanges);
            }
        }

        /// <summary>
        /// Removes disks from vm which are un-protected.
        /// </summary>
        /// <param name="disksInfo">Source server disks.</param>
        /// <param name="vmView">The VM view.</param>
        public void RemoveUnprotectedDisks(List<DiskInfo> disksInfo, VirtualMachine vmView)
        {
            string vmPathName = vmView.Summary.Config.VmPathName;
            VirtualDevice[] devices = vmView.Config.Hardware.Device;
            List<string> diskToBeRemoved = new List<string>();

            foreach (VirtualDevice device in devices)
            {
                bool found = false;
                if (device.DeviceInfo.Label.Contains(
                    "Hard Disk",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualDeviceFileBackingInfo backingInfo =
                        (VirtualDeviceFileBackingInfo)device.Backing;
                    int keyFile = device.Key;
                    string diskName = backingInfo.FileName;
                    if (vmView.Snapshot != null)
                    {
                        VirtualMachineFileLayoutDiskLayout[] diskLayoutInfo = vmView.Layout.Disk;
                        foreach (var diskLayout in diskLayoutInfo)
                        {
                            if (diskLayout.Key != keyFile)
                            {
                                continue;
                            }

                            foreach (string diskFile in diskLayout.DiskFile)
                            {
                                // Eg:"something-123456.vmdk"
                                if (Regex.IsMatch(diskFile, @"-[0-9]{6}\.vmdk$"))
                                {
                                    continue;
                                }

                                diskName = diskFile;
                                T.Tracer().TraceInformation(
                                    "Base disk information collected for key({0}) = {1}.",
                                    keyFile,
                                    diskName);
                            }
                        }
                    }

                    string diskNameToRemove = diskName;
                    int finalSlashVmPathName = vmPathName.LastIndexOf("/");
                    int finalSlashDiskName = diskName.LastIndexOf("/");
                    string vmDatastorePath = vmPathName.Substring(0, finalSlashVmPathName);
                    string diskDatastorePath = diskName.Substring(0, finalSlashDiskName);
                    T.Tracer().TraceInformation(
                        "Comparing {0} and {1}.",
                        vmDatastorePath,
                        diskDatastorePath);
                    if (vmDatastorePath != diskDatastorePath)
                    {
                        string vmdkName =
                            diskName.Substring(
                            finalSlashDiskName,
                            diskName.Length - finalSlashDiskName - 1);
                        int dotIndex = vmdkName.LastIndexOf(".");
                        string vmdkFileName = vmdkName.Substring(0, dotIndex);
                        diskName =
                            vmDatastorePath + vmdkFileName + "_InMage_NC_" + keyFile + ".vmdk";
                        T.Tracer().TraceInformation(
                            "Disk name: {0}.",
                            diskName);
                    }

                    string vmdkUuid =
                        (string)device.Backing.GetType().GetProperty("Uuid")
                        .GetValue(device.Backing);
                    PropertyInfo compatModeProperty =
                        device.Backing.GetType().GetProperty("CompatibilityMode");
                    if (compatModeProperty != null)
                    {
                        string compatabilityMode =
                            (string)compatModeProperty.GetValue(device.Backing);
                        if (string.Compare(
                            compatabilityMode,
                            "physicalmode",
                            ignoreCase: true) == 0)
                        {
                            vmdkUuid =
                                (string)device.Backing.GetType().GetProperty("LunUuid")
                                .GetValue(device.Backing);
                        }
                    }

                    string[] vmdkNameTarget = diskName.Split(
                        new string[] { "[", "]", "/" },
                        StringSplitOptions.RemoveEmptyEntries);
                    foreach (DiskInfo vmdk in disksInfo)
                    {
                        string vmdkName = vmdk.disk_name;
                        string[] vmdkNameXml = vmdkName.Split(
                            new string[] { "[", "]", "/" },
                            StringSplitOptions.RemoveEmptyEntries);

                        string tgtDiskUuid = vmdk.target_disk_uuid;

                        if (!string.IsNullOrEmpty(tgtDiskUuid))
                        {
                            if (tgtDiskUuid == vmdkUuid)
                            {
                                found = true;
                                break;
                            }
                        }
                        else
                        {
                            T.Tracer().TraceInformation(
                                "Comparing {0} and {1}.",
                                vmdkNameXml.Last(),
                                vmdkNameTarget.Last());
                            if (vmdkNameXml.Last() == vmdkNameTarget.Last())
                            {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found)
                    {
                        T.Tracer().TraceInformation("{0} will be removed from source machine.");
                        diskToBeRemoved.Add(diskNameToRemove);
                    }
                }
            }

            this.RemoveDisk(diskToBeRemoved, vmView);
        }

        /// <summary>
        /// It either creates disks/uses an existing disk.
        /// 1. In all cases it check whether the SCSI controller exists or not. If does not exist,
        ///    creates it.
        /// 2. If disk is to be re-used, it checks for its size as well.
        /// 3. If size has to be extended, it extends.
        /// 4. If size has to be decremented, it deleted existing disk and re-creates it.
        /// 5. It updates the virtual machine view from time-time.
        /// </summary>
        /// <param name="sourceHost">Configuration of the machine to be created.</param>
        /// <param name="vmView">Virtual machine view to which disks are configured.</param>
        /// <param name="mtView">Virtual machine view of MT.</param>
        /// <param name="isCluster">Whether its clustered.</param>
        /// <param name="createdDisks">Info. about the disks that were created/updated.</param>
        public void ConfigureDisks(
            VmConfigureDiskInfo sourceHost,
            VirtualMachine vmView,
            VirtualMachine mtView,
            bool isCluster,
            out Dictionary<string, CreatedDiskData> createdDisks)
        {
            createdDisks = new Dictionary<string, CreatedDiskData>();
            string machineName = vmView.Name;
            string clusterNodes = string.Empty;
            List<VirtualDeviceConfigSpec> deviceChange = new List<VirtualDeviceConfigSpec>();
            T.Tracer().TraceInformation("Configuring disks of machine {0}.", machineName);

            string inmageHostId = sourceHost.inmage_hostid;
            if (isCluster)
            {
                clusterNodes = sourceHost.clusternodes_inmageguids;
            }

            int ideCount;
            int floppyCount;
            bool rdm;
            string cluster;
            List<DiskData> existingDisksInfoList;
            this.FindDiskDetails(
                vmView,
                new List<string>(),
                out ideCount,
                out floppyCount,
                out rdm,
                out cluster,
                out existingDisksInfoList);
            DiskInfo[] disksInfo = sourceHost.disk.ToArray();
            Dictionary<string, string> disksInfoInVm;
            this.FindOccupiedScsiIds(vmView, out disksInfoInVm);

            foreach (DiskInfo disk in disksInfo)
            {
                T.Tracer().TraceInformation(
                    "Configuring disk {0} for machine {1}. vmDirectoryPath: {2}.",
                    disk.disk_name,
                    machineName,
                    sourceHost.vmDirectoryPath);

                DiskData existingDiskInfo = null;
                string diskName = disk.disk_name;
                string[] diskPath = diskName.Split(
                    new string[] { "[", "]" },
                    StringSplitOptions.RemoveEmptyEntries);
                diskPath[diskPath.Length - 1] = diskPath.Last().TrimStart();
                if (!string.IsNullOrEmpty(sourceHost.vmDirectoryPath))
                {
                    diskPath[diskPath.Length - 1] =
                        sourceHost.vmDirectoryPath + "/" + diskPath.Last();
                }
                else if (!string.IsNullOrEmpty(sourceHost.vsan_folder))
                {
                    diskPath = diskName.Split(
                    new string[] { "/" },
                    StringSplitOptions.RemoveEmptyEntries);
                    diskPath[diskPath.Length - 1] = sourceHost.vsan_folder + diskPath.Last();
                }

                diskName = "[" + disk.datastore_selected + "] " + diskPath.Last();
                T.Tracer().TraceInformation(
                    "Final disk name {0} for machine {1}.",
                    diskName,
                    machineName);

                if (existingDisksInfoList.Any())
                {
                    string tgtDiskUuid = disk.target_disk_uuid;

                    foreach (DiskData existingDisk in existingDisksInfoList)
                    {
                        if (!string.IsNullOrEmpty(tgtDiskUuid))
                        {
                            if (tgtDiskUuid != existingDisk.DiskUuid)
                            {
                                continue;
                            }
                        }
                        else if (diskName != existingDisk.DiskName)
                        {
                            continue;
                        }

                        existingDiskInfo = existingDisk;
                        break;
                    }
                }

                string diskUuid = disk.source_disk_uuid;
                if (string.Compare(sourceHost.machinetype, "physicalmachine", ignoreCase: true) == 0)
                {
                    if (disk.disk_signature != null)
                    {
                        diskUuid = disk.disk_signature;
                    }
                }

                if (existingDiskInfo != null)
                {
                    T.Tracer().TraceInformation("Already existing diskname: {0}.", diskName);

                    if (createdDisks.ContainsKey(diskUuid))
                    {
                        CreatedDiskData clusterDisk = createdDisks[diskUuid];
                        if (!string.IsNullOrEmpty(disk.@protected) && disk.@protected == "yes")
                        {
                        }
                        else if (clusterNodes.Contains(
                            clusterDisk.InMageHostId,
                            StringComparison.OrdinalIgnoreCase))
                        {
                            disk.selected = "no";
                            T.Tracer().TraceInformation(
                                "Setting as selected for cluster disk: {0}.",
                                diskName);
                        }
                    }

                    if (disk.cluster_disk == "yes")
                    {
                        createdDisks[diskUuid] = new CreatedDiskData
                        {
                            DiskName = diskName,
                            InMageHostId = inmageHostId
                        };
                    }

                    if (existingDiskInfo.DiskMode == "Mapped Raw LUN")
                    {
                        continue;
                    }

                    T.Tracer().Assert(
                        long.Parse(disk.size) <= existingDiskInfo.DiskSize,
                        "Seed disk logic only kicks in if existing disk is same or greater size " +
                        "but that is not the case. Existing disk size: {0}, Incoming disk " +
                        "size: {1}",
                        existingDiskInfo.DiskSize,
                        long.Parse(disk.size));

                    T.Tracer().TraceInformation(
                        "For VM '{0}' existing disk scsi vmx '{1}', " +
                        "source disk scsi vmx '{2}'. Existing disk and source disk" +
                        " size is same/smaller. Treating this as seed ir case.",
                        vmView.Name,
                        disk.scsi_mapping_vmx,
                        existingDiskInfo.ScsiVmx);
                    continue;
                }

                T.Tracer().Assert(
                    existingDiskInfo == null,
                    "Not expecting existing disk in disk creation case.");

                string operation = "create";
                int controllerKey;
                int unitNumber;
                int key;
                string scsiId;
                int busNumber;
                this.FindFreeSlotInVm(
                    vmView,
                    disksInfoInVm,
                    out controllerKey,
                    out unitNumber,
                    out scsiId,
                    out key,
                    out busNumber);
                disksInfoInVm[diskName] = key.ToString();
                disksInfoInVm[key.ToString()] = diskName;

                if (!this.FindScsiController(vmView, controllerKey))
                {
                    string sharing = disk.controller_mode;
                    string controllerType = disk.controller_type;
                    VirtualDeviceConfigSpec scsiControllerSpec =
                        this.NewSCSIControllerSpec(
                            busNumber,
                            (VirtualSCSISharing)Enum.Parse(
                                typeof(VirtualSCSISharing),
                                sharing),
                            controllerType,
                            controllerKey);
                    deviceChange.Add(scsiControllerSpec);
                    T.Tracer().TraceInformation(
                        "Successfully created SCSI device spec to be added at " +
                        "controllerKey: {0}, unitNumber: {1}, key: {2}, " +
                        "busNumber: {3}, scsiId: {4} for disk {5}.",
                        controllerKey,
                        unitNumber,
                        key,
                        busNumber,
                        scsiId,
                        diskName);
                }

                string rootFolderPath = string.Empty;
                if (!string.IsNullOrEmpty(sourceHost.vmDirectoryPath))
                {
                    rootFolderPath = sourceHost.vmDirectoryPath;
                    T.Tracer().TraceInformation("RootFolderPath: {0}.", rootFolderPath);
                }

                if (existingDiskInfo != null)
                {
                    T.Tracer().Assert(
                        existingDiskInfo == null,
                        "existingDiskInfo being not null in disk creation code path is " +
                        "unexpected!.");
                    ////operation = "edit";
                    ////string[] scsi = existingDiskInfo.ScsiVmx.Split(
                    ////    new string[] { ":" },
                    ////    StringSplitOptions.RemoveEmptyEntries);
                    ////int scsiLhs = int.Parse(scsi[0]);
                    ////int scsiRhs = int.Parse(scsi[1]);
                    ////diskKey = scsiLhs < 4 ?
                    ////    2000 + (scsiLhs * 16) + scsiRhs :
                    ////    16000 + ((splitLhs - 4) * 30) + scsiRhs;
                }
                else if (createdDisks.ContainsKey(diskUuid))
                {
                    T.Tracer().Assert(
                        createdDisks.Count == 0,
                        "createdDisks being set in disk creation code path is " +
                        "unexpected!.");
                    ////diskKey = splitLhs < 4 ?
                    ////    2000 + (splitLhs * 16) + splitRhs :
                    ////    16000 + ((splitLhs - 4) * 30) + splitRhs;

                    ////CreatedDiskData clusterDisk = createdDisks[diskUuid];
                    ////if (clusterNodes.Contains(
                    ////    clusterDisk.InMageHostId,
                    ////    StringComparison.OrdinalIgnoreCase))
                    ////{
                    ////    operation = "add";
                    ////    diskName = clusterDisk.DiskName;
                    ////    disk.selected = "no";
                    ////    T.Tracer().TraceInformation(
                    ////        "Adding cluster disk, setting as selected for cluster disk: {0}.",
                    ////        diskName);
                    ////}
                }

                VirtualDeviceConfigSpec virtualDeviceSpec =
                    this.VirtualDiskSpec(
                        diskConfig: disk,
                        controllerKey: controllerKey,
                        unitNumber: unitNumber,
                        diskKey: key,
                        rootFolderName: rootFolderPath,
                        operation: operation,
                        diskName: diskName);
                if (disk.cluster_disk == "yes")
                {
                    createdDisks[diskUuid] = new CreatedDiskData
                    {
                        DiskName = diskName,
                        InMageHostId = inmageHostId
                    };
                }

                deviceChange.Add(virtualDeviceSpec);
            }

            if (!this.client.ServiceContent.About.ApiType.Contains("VirtualCenter") &&
                deviceChange.Count > 0)
            {
                T.Tracer().TraceInformation(" Target is not Vcenter. Creating parent " +
                    "directories before creating disks.");
                this.CreateParentDirectoryForDeviceChanges(mtView, deviceChange);
            }

            this.ReconfigureVm(vmView, deviceChange);
        }

        /// <summary>
        /// It re-uses the existing Disk information and add them to Master Target.
        /// </summary>
        /// <param name="createdVmInfo">The replica VM that was created.</param>
        /// <param name="vmView">The virtual machine view whose disk are to be added to MT.</param>
        /// <param name="mtView">The master target view.</param>
        public void AttachReplicaDisksToMasterTarget(
            FolderOpts.CreatedVmInfo createdVmInfo,
            VirtualMachine vmView,
            VirtualMachine mtView)
        {
            List<VirtualDeviceConfigSpec> virtualDiskSpecs = new List<VirtualDeviceConfigSpec>();
            Dictionary<string, string> disksInfoInMT;
            this.FindOccupiedScsiIds(mtView, out disksInfoInMT);
            VirtualDevice[] devices = vmView.Config.Hardware.Device;

            foreach (VirtualDevice device in devices)
            {
                if (!device.DeviceInfo.Label.Contains(
                    "Hard Disk",
                    StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                var backingInfo = (VirtualDeviceFileBackingInfo)device.Backing;
                string fileName = backingInfo.FileName;
                if (disksInfoInMT.ContainsKey(fileName))
                {
                    T.Tracer().TraceInformation("{0} already added to master target.", fileName);
                    continue;
                }

                VirtualDisk vDisk = (VirtualDisk)device;
                long fileSize = vDisk.CapacityInKB;
                string diskMode =
                    (string)device.Backing.GetType()
                    .GetProperty("DiskMode").GetValue(device.Backing);
                bool? diskType = null;
                string compatabilityMode = string.Empty;
                string lunUuid = string.Empty;
                PropertyInfo compatModeProperty =
                    device.Backing.GetType().GetProperty("CompatibilityMode");
                if (compatModeProperty != null)
                {
                    compatabilityMode =
                        (string)compatModeProperty.GetValue(device.Backing);
                    if (string.Compare(
                        compatabilityMode,
                        "physicalmode",
                        ignoreCase: true) == 0)
                    {
                        lunUuid =
                            (string)device.Backing.GetType().GetProperty("LunUuid")
                            .GetValue(device.Backing);
                    }
                }
                else
                {
                    diskType =
                        (bool?)device.Backing.GetType().GetProperty("ThinProvisioned")
                        .GetValue(device.Backing);
                }

                int controllerKey;
                int unitNumber;
                string scsiId;
                int key;
                int busNumber;
                this.FindFreeSlotInVm(
                    mtView,
                    disksInfoInMT,
                    out controllerKey,
                    out unitNumber,
                    out scsiId,
                    out key,
                    out busNumber);

                T.Tracer().TraceInformation(
                    "Attaching disk {0} of size {1} at SCSI unit number {2} of type {3} " +
                    "in mode {4}.",
                    fileName,
                    fileSize,
                    unitNumber,
                    diskType,
                    diskMode);
                VirtualDeviceFileBackingInfo diskBackingInfo = new VirtualDiskFlatVer2BackingInfo
                {
                    DiskMode = diskMode,
                    FileName = fileName,
                    ThinProvisioned = diskType
                };

                if (!string.IsNullOrEmpty(compatabilityMode))
                {
                    diskBackingInfo = new VirtualDiskRawDiskMappingVer1BackingInfo
                    {
                        CompatibilityMode = compatabilityMode,
                        DiskMode = diskMode,
                        DeviceName = lunUuid,
                        FileName = fileName
                    };
                    T.Tracer().TraceInformation(
                        "DeviceName: {0}, compatabilityMode: {1}.",
                        lunUuid,
                        compatabilityMode);
                }

                var connectionInfo = new VirtualDeviceConnectInfo
                {
                    AllowGuestControl = false,
                    Connected = true,
                    StartConnected = true
                };

                var virtualDisk = new VirtualDisk
                {
                    ControllerKey = controllerKey,
                    UnitNumber = unitNumber,
                    Key = -1,
                    Backing = backingInfo,
                    CapacityInKB = fileSize
                };

                var virtualDeviceSpec = new VirtualDeviceConfigSpec
                {
                    Operation = VirtualDeviceConfigSpecOperation.add,
                    Device = virtualDisk
                };

                virtualDiskSpecs.Add(virtualDeviceSpec);

                disksInfoInMT[fileName] = key.ToString();
                disksInfoInMT[key.ToString()] = fileName;
                createdVmInfo.FileName = scsiId;
                T.Tracer().TraceInformation(
                    "Successfully created re-use existing disk spec for {0}.",
                    fileName);
            }

            if (virtualDiskSpecs.Any())
            {
                this.ReconfigureVm(mtView, virtualDiskSpecs);
            }
        }

        /// <summary>
        /// Returns an available controller key, unit number and key where a disk can be added
        /// to the VM.
        /// </summary>
        /// <param name="vmView">The VM to check.</param>
        /// <param name="disksInfoInVm">Existing key usage information.</param>
        /// <param name="freeControllerKey">The available controller key.</param>
        /// <param name="freeUnitNumber">The available unit number.</param>
        /// <param name="scsiId">The SCSI Id for the disk.</param>
        /// <param name="freeKey">The available key.</param>
        /// <param name="busNumber">The bus number.</param>
        public void FindFreeSlotInVm(
            VirtualMachine vmView,
            Dictionary<string, string> disksInfoInVm,
            out int freeControllerKey,
            out int freeUnitNumber,
            out string scsiId,
            out int freeKey,
            out int busNumber)
        {
            int? controllerKey = null;
            int? unitNumber = null;
            int? key = null;
            busNumber = 0;
            string vmxVersion = vmView.Config.Version;
            scsiId = null;
            for (int i = 0; i <= 3; i++)
            {
                for (int j = 0; j <= 15; j++)
                {
                    if (j == 7)
                    {
                        continue;
                    }

                    int keyCheck = 2000 + (i * 16) + j;
                    if (disksInfoInVm.ContainsKey(keyCheck.ToString()))
                    {
                        continue;
                    }

                    unitNumber = (keyCheck - 2000) % 16;
                    controllerKey = 1000 + i;
                    scsiId = i + ":" + j;
                    key = keyCheck;
                    busNumber = i;
                    break;
                }

                if (unitNumber.HasValue && controllerKey.HasValue)
                {
                    break;
                }
            }

            if ((!unitNumber.HasValue || !controllerKey.HasValue) &&
                string.Compare(vmxVersion, "vmx-10", ignoreCase: true) >= 0 &&
                vmView.Summary.Config.GuestFullName.Contains(
                    "Windows",
                    StringComparison.OrdinalIgnoreCase))
            {
                for (int i = 0; i <= 3; i++)
                {
                    for (int j = 0; j <= 29; j++)
                    {
                        int unitNumCheck = 16000 + (i * 30) + j;
                        if (disksInfoInVm.ContainsKey(unitNumCheck.ToString()))
                        {
                            continue;
                        }

                        unitNumber = (unitNumCheck - 16000) % 30;
                        controllerKey = 15000 + i;
                        scsiId = i + ":" + j;
                        key = unitNumCheck;
                        busNumber = i;
                        break;
                    }

                    if (unitNumber.HasValue && controllerKey.HasValue)
                    {
                        break;
                    }
                }
            }

            if (!unitNumber.HasValue || !controllerKey.HasValue)
            {
                throw VMwareClientException.NoFreeSlotInMasterTarget(vmView.Name);
            }

            freeControllerKey = controllerKey.Value;
            freeUnitNumber = unitNumber.Value;
            freeKey = key.Value;
        }

        /// <summary>
        /// Checks whether the guest os parameter is set or not.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <param name="guestId">The guest os name</param>
        public void CheckGuestOsFullName(VirtualMachine vmView, string guestId)
        {
            for (int i = 0; i < 5; i++)
            {
                if (string.IsNullOrEmpty(vmView.Summary.Config.GuestFullName))
                {
                    T.Tracer().TraceInformation(
                        "Guest OS name is not populated for {0}. So reconfiguring it.",
                        vmView.Name);
                    VirtualMachineConfigSpec spec = new VirtualMachineConfigSpec
                    {
                        GuestId = guestId
                    };

                    this.ReconfigureVm(vmView, spec);
                    this.UpdateVmView(vmView);
                }
                else
                {
                    T.Tracer().TraceInformation("Gues OS name is populated for {0}.", vmView.Name);
                    break;
                }
            }
        }

        /// <summary>
        /// Maps each and every disk selected for protection with its corresponding SCSI number
        /// fetched through CX-API.
        /// </summary>
        /// <param name="cxIpAndPortNum">CX IP and port number.</param>
        /// <param name="machineType">The source host machine type.</param>
        public void MapSourceScsiNumbersOfHost(string cxIpAndPortNum, string machineType)
        {
            if (string.Compare(machineType, "physicalmachine", ignoreCase: true) == 0)
            {
                return;
            }

            throw new NotImplementedException(
                "MapSourceScsiNumbersOfHost not implemented as it was not required for P2V.");
        }

        /// <summary>
        /// It will power on and off the virtual machine. This is used only in the case of
        /// Linux machines.
        /// </summary>
        /// <param name="vmView">Virtual machine view.</param>
        public void OnOffMachine(VirtualMachine vmView)
        {
            for (int i = 0; i <= 3; i++)
            {
                try
                {
                    this.PowerOn(vmView);
                    break;
                }
                catch (Exception)
                {
                    if (i == 3)
                    {
                        throw;
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(10));
                }
            }

            for (int i = 0; i <= 3; i++)
            {
                try
                {
                    this.PowerOff(vmView, TimeSpan.FromSeconds(600));
                    break;
                }
                catch (Exception)
                {
                    if (i == 3)
                    {
                        throw;
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(10));
                }
            }
        }

        /// <summary>
        /// Creates the parent directories for virtual disk file paths specified in deviceChanges.
        /// </summary>
        /// <param name="mtView">The mt virtual machine view.</param>
        /// <param name="deviceChanges">The list of virtual disk device changes.</param>
        private void CreateParentDirectoryForDeviceChanges(
            VirtualMachine mtView,
            List<VirtualDeviceConfigSpec> deviceChanges)
        {
            try
            {
                Datacenter mtDataCenterView = null;
                if (mtView.Parent != null)
                {
                    ManagedObjectReference parentView = mtView.Parent;
                    while (!parentView.Type.Contains(
                        "Datacenter",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        ViewBase view = mtView.Client.GetView(
                            parentView,
                            new string[] { "parent" });
                        parentView =
                            (ManagedObjectReference)view.GetType()
                            .GetProperty("Parent").GetValue(view);
                    }

                    mtDataCenterView =
                        (Datacenter)mtView.Client.GetView(parentView, new string[] { "name" });

                    mtDataCenterView =
                        this.dataCenterOpts.GetDataCenterView(mtDataCenterView.Name);
                }

                T.Tracer().TraceInformation(
                    "MT {0} is present in datacenter: {1}.",
                    mtView.Name,
                    mtDataCenterView.Name);

                HashSet<string> folderPaths = new HashSet<string>();
                foreach (VirtualDeviceConfigSpec deviceChange in deviceChanges)
                {
                    VirtualDisk vdisk = deviceChange.Device as VirtualDisk;
                    VirtualDiskFlatVer2BackingInfo backing =
                        vdisk.Backing as VirtualDiskFlatVer2BackingInfo;

                    string fileName = backing.FileName;
                    T.Tracer().TraceInformation(
                       "For MT {0} device change contains virtual disk path {1}. Creating parent " +
                       "folder path for disk paths.",
                       mtView.Name,
                       fileName);
                    string[] splitFileName = fileName.Split('/');
                    StringBuilder folderPath = new StringBuilder();
                    int index = 0;
                    for (; index < splitFileName.Count() - 1; index++)
                    {
                        folderPath.Append(splitFileName[index]);
                        folderPath.Append('/');
                    }

                    T.Tracer().TraceInformation(
                      "For virtual disk path {0} parent folder path {1}.",
                      fileName,
                      folderPath.ToString());
                    folderPaths.Add(folderPath.ToString());
                }

                foreach (string folderPath in folderPaths)
                {
                    T.Tracer().TraceInformation(
                      "On MT: {0} creating folder on path {1}.",
                      mtView.Name,
                      folderPath);

                    this.datastoreOpts.CreatePath(folderPath, mtDataCenterView.Name);
                }
            }
            catch (Exception ex)
            {
                T.Tracer().TraceException(
                    ex,
                    "Hit exception in creating directory on mt data store.");

                VMwareClientException vmwareException = ex as VMwareClientException;
                if (vmwareException != null &&
                    vmwareException.ErrorCode != VMwareClientErrorCode.FileAlreadyExists)
                {
                    throw;
                }
                else
                {
                    throw VMwareClientException.DirectoryCreationFailed(mtView.Name, ex.Message);
                }
            }
        }

        /// <summary>
        /// Removes the specified disks from the virtual machine.
        /// </summary>
        /// <param name="diskInfo">The disks to remove.</param>
        /// <param name="vmView">The virtual machine view.</param>
        private void RemoveDisk(List<string> diskInfo, VirtualMachine vmView)
        {
            foreach (string diskToBeRemoved in diskInfo)
            {
                foreach (VirtualDevice device in vmView.Config.Hardware.Device)
                {
                    if (!device.DeviceInfo.Label.Contains(
                        "Hard Disk",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        continue;
                    }

                    var backingInfo = (VirtualDeviceFileBackingInfo)device.Backing;
                    if (diskToBeRemoved != backingInfo.FileName)
                    {
                        continue;
                    }

                    VirtualDisk virtualDisk = (VirtualDisk)device;
                    string diskMode =
                        (string)device.Backing.GetType()
                        .GetProperty("DiskMode").GetValue(device.Backing);

                    this.RemoveVmdk(
                        backingInfo.FileName,
                        virtualDisk.CapacityInKB,
                        diskMode,
                        device.ControllerKey,
                        device.Key,
                        destroy: false,
                        vmView: vmView);
                }
            }
        }

        /// <summary>
        /// Removes vmdk form virtual machine.
        /// </summary>
        /// <param name="fileName">The VMDK file name.</param>
        /// <param name="fileSize">The file size.</param>
        /// <param name="diskMode">The disk mode.</param>
        /// <param name="controllerKey">The controller key.</param>
        /// <param name="key">The key value.</param>
        /// <param name="destroy">Whether to delete the file too.</param>
        /// <param name="vmView">The VM view.</param>
        private void RemoveVmdk(
            string fileName,
            long fileSize,
            string diskMode,
            int? controllerKey,
            int key,
            bool destroy,
            VirtualMachine vmView)
        {
            T.Tracer().TraceInformation(
                "Removing disk {0} from machine {1} of size {2} at SCSI controller number {3} " +
                "and unit number {4} in mode {5}.",
                fileName,
                vmView.Name,
                fileSize.ToString(),
                controllerKey,
                key,
                diskMode);

            var diskBackingInfo = new VirtualDiskFlatVer2BackingInfo
            {
                DiskMode = diskMode,
                FileName = fileName
            };

            var virtualDisk = new VirtualDisk
            {
                ControllerKey = controllerKey,
                Key = key,
                Backing = diskBackingInfo,
                CapacityInKB = fileSize
            };

            var devSpec = new List<VirtualDeviceConfigSpec>();
            if (destroy)
            {
                devSpec.Add(new VirtualDeviceConfigSpec
                {
                    Operation = VirtualDeviceConfigSpecOperation.remove,
                    Device = virtualDisk,
                    FileOperation = VirtualDeviceConfigSpecFileOperation.destroy
                });
            }
            else
            {
                devSpec.Add(new VirtualDeviceConfigSpec
                {
                    Operation = VirtualDeviceConfigSpecOperation.remove,
                    Device = virtualDisk
                });
            }

            this.ReconfigureVm(vmView, devSpec);
            T.Tracer().TraceInformation(
                "Successfully removed {0} from master target {1}.",
                fileName,
                vmView.Name);
        }

        /// <summary>
        /// Checks whether the Host name is populated for the specified machine.
        /// </summary>
        /// <param name="uuid">The virtual machine Id.</param>
        /// <returns>Whether the host name is populated.</returns>
        private bool CheckHostName(string uuid)
        {
            VirtualMachine vmView = this.GetVmViewByUuid(uuid);
            for (int i = 0; i < 300;)
            {
                if (string.IsNullOrEmpty(vmView.Guest.HostName))
                {
                    T.Tracer().TraceInformation(
                        "Host name for machine {0} not yet populated. Waiting.",
                        vmView.Name);
                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(5));
                    i += 5;
                    vmView.UpdateViewData(this.GetVmProps());
                }
                else
                {
                    string hostName = vmView.Guest.HostName;
                    T.Tracer().TraceInformation(
                        "Machine {0} booted successfully and host name is {1}.",
                        vmView.Name,
                        hostName);
                    return true;
                }
            }

            T.Tracer().TraceError("Host name not populated for {0}.", vmView.Name);

            // 18757 -- Marking as success even when host name of machine is not populated.
            return true;
        }

        /// <summary>
        /// Uses ShutdowGuest() to shut down a machine. This is called when VMware tools are
        /// running.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <param name="hostName">The host name.</param>
        /// <param name="timeout">Time out of ShutdownVM call.</param>
        private void ShutdownGuest(
            VirtualMachine vmView,
            string hostName,
            TimeSpan timeout)
        {
            T.Tracer().TraceInformation(
                "Shutting down machine {0} on host {1}.",
                vmView.Name,
                hostName);

            try
            {
                // Will be skipping the power off call in case VM is already in powered off state.
                if (vmView.Summary.Runtime.PowerState == VirtualMachinePowerState.poweredOff)
                {
                    T.Tracer().TraceInformation(
                        "Virtual machine {0} is already in powered off state.",
                        vmView.Name);
                    return;
                }

                vmView.ShutdownGuest();

                vmView.UpdateViewData(this.GetVmProps());
                for (int i = 0;; i += 5)
                {
                    if (vmView.Summary.Runtime.PowerState == VirtualMachinePowerState.poweredOff)
                    {
                        break;
                    }

                    if (i < timeout.TotalSeconds)
                    {
                        T.Tracer().TraceInformation(
                            "Sleeping a bit as machine {0} state is not powered off: {1}.",
                            vmView.Name,
                            vmView.Summary.Runtime.PowerState.ToString());
                        System.Threading.Thread.Sleep(TimeSpan.FromSeconds(5));
                        vmView.UpdateViewData(this.GetVmProps());
                        continue;
                    }

                    throw VMwareClientException.FailedToShutdownVmTimeout(vmView.Name);
                }
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is NotSupported)
                {
                    throw VMwareClientException.CannotPowerOffVmMarkedAsTemplate(vmView.Name);
                }

                if (ve.MethodFault is InvalidPowerState)
                {
                    throw VMwareClientException.CannotPowerOffVmInvalidPowerState(vmView.Name);
                }

                if (ve.MethodFault is InvalidState)
                {
                    throw VMwareClientException.CannotPowerOffVmInCurrentState(vmView.Name);
                }

                throw VMwareClientException.CannotPowerOffVm(vmView.Name, ve.ToString());
            }
            catch (Exception e)
            {
                throw VMwareClientException.CannotPowerOffVm(vmView.Name, e.ToString());
            }

            T.Tracer().TraceInformation("Successfully shut down machine {0}.", vmView.Name);
        }

        /// <summary>
        /// Uses command PowerOffVm() to power off the machine. This is used when VmWare tools
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <param name="hostName">The host name.</param>
        /// <param name="timeout">Time Out of PowerOff VM call.</param>
        private void PowerOffVm(
            VirtualMachine vmView,
            string hostName,
            TimeSpan timeout)
        {
            T.Tracer().TraceInformation(
                "Powering off machine {0} on host {1}.",
                vmView.Name,
                hostName);

            try
            {
                // Will be skipping the power off call in case VM is already in powered off state.
                if (vmView.Summary.Runtime.PowerState == VirtualMachinePowerState.poweredOff)
                {
                    T.Tracer().TraceInformation(
                        "Virtual machine {0} is already in powered off state.",
                        vmView.Name);
                    return;
                }

                ManagedObjectReference taskRef = vmView.PowerOffVM_Task();
                var taskView = (Task)this.client.GetView(taskRef, null);

                while (true)
                {
                    TaskInfo info = taskView.Info;
                    if (info.State == TaskInfoState.running)
                    {
                        T.Tracer().TraceInformation(
                            "Powering off machine {0}.",
                            vmView.Name);
                    }
                    else if (info.State == TaskInfoState.success)
                    {
                        T.Tracer().TraceInformation(
                            "Virtual machine {0} under host {1} powered off.",
                            vmView.Name,
                            hostName);
                        break;
                    }
                    else if (info.State == TaskInfoState.error)
                    {
                        string errorMessage = info.Error.LocalizedMessage;
                        T.Tracer().TraceError(
                            "Power-off failed on machine {0} due to error: {1}.",
                            vmView.Name,
                            errorMessage);
                        throw VMwareClientException.PowerOffFailedOnVm(vmView.Name, errorMessage);
                    }

                    vmView.UpdateViewData(this.GetVmProps());
                    for (int i = 0;; i += 5)
                    {
                        if (vmView.Summary.Runtime.PowerState == VirtualMachinePowerState.poweredOff)
                        {
                            break;
                        }

                        if (i < timeout.TotalSeconds)
                        {
                            T.Tracer().TraceInformation(
                                "Sleeping a bit as machine {0} state is not powered off: {1}.",
                                vmView.Name,
                                vmView.Summary.Runtime.PowerState.ToString());
                            System.Threading.Thread.Sleep(TimeSpan.FromSeconds(5));
                            vmView.UpdateViewData(this.GetVmProps());
                            continue;
                        }
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                    taskView.UpdateViewData();
                }
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is NotSupported)
                {
                    throw VMwareClientException.CannotPowerOffVmMarkedAsTemplate(vmView.Name);
                }

                if (ve.MethodFault is InvalidPowerState)
                {
                    throw VMwareClientException.CannotPowerOffVmInvalidPowerState(vmView.Name);
                }

                if (ve.MethodFault is InvalidState)
                {
                    throw VMwareClientException.CannotPowerOffVmInCurrentState(vmView.Name);
                }

                throw VMwareClientException.CannotPowerOffVm(vmView.Name, ve.ToString());
            }
            catch (Exception e)
            {
                throw VMwareClientException.CannotPowerOffVm(vmView.Name, e.ToString());
            }
        }

        /// <summary>
        /// This is for the answering of the question after we make the machine up. As of now
        /// we are going with default options.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        private void AnswerQuestion(VirtualMachine vmView)
        {
            ElementDescription[] choice = vmView.Runtime.Question.Choice.ChoiceInfo;
            string powerOnAnswer = "1";
            T.Tracer().TraceInformation("Selecting the option: {0}.", powerOnAnswer);

            foreach (ElementDescription item in choice)
            {
                if (item.Key == powerOnAnswer)
                {
                    T.Tracer().TraceInformation("Description of option was: {0}.", item.Label);
                }
            }

            try
            {
                vmView.AnswerVM(vmView.Runtime.Question.Id, powerOnAnswer);
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is ConcurrentAccess)
                {
                    throw VMwareClientException.AnswerVmConcurrentAccess(vmView.Name);
                }

                if (ve.MethodFault is InvalidArgument)
                {
                    throw VMwareClientException.QuestionIdDoesNotApply(
                        vmView.Runtime.Question.Id,
                        vmView.Name);
                }

                throw VMwareClientException.FailedToAnswerQuestion(vmView.Name, ve.ToString());
            }
            catch (Exception e)
            {
                throw VMwareClientException.FailedToAnswerQuestion(vmView.Name, e.ToString());
            }
        }

        /// <summary>
        /// Populates arrays Network Name, Mac address and IP values to that NIC.
        /// </summary>
        /// <param name="vmView">The virtual machines.</param>
        /// <param name="dvPortGroupInfo">The port group info.</param>
        /// <param name="ipAddresses">The IP addresses.</param>
        /// <param name="networkData">The network data.</param>
        private void FindNetworkDetails(
            VirtualMachine vmView,
            List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo,
            out List<string> ipAddresses,
            out List<NetworkData> networkData)
        {
            ipAddresses = new List<string>();
            string displayName = vmView.Name;
            VirtualDevice[] devices = vmView.Config.Hardware.Device;
            var mapKeyNic = new List<KeyValuePair<int, NetworkAdapterInfo>>();
            var macAddressList = new Dictionary<string, string>();
            var networkDataList = new List<NetworkData>();
            var macInformationFetched = new Dictionary<string, string>();

            foreach (var device in devices)
            {
                if (device.DeviceInfo.Label.StartsWith(
                    "Network adapter",
                    StringComparison.OrdinalIgnoreCase))
                {
                    if (device is VirtualEthernetCard)
                    {
                        VirtualEthernetCard ethernetCard = device as VirtualEthernetCard;
                        if (ethernetCard.MacAddress != null)
                        {
                            macAddressList.Add(
                                ethernetCard.MacAddress,
                                device.DeviceInfo.Label);
                        }

                        NetworkAdapterInfo info = new NetworkAdapterInfo
                        {
                            AddressType = ethernetCard.AddressType,
                            Label = device.DeviceInfo.Label,
                            BackingType = string.Empty,
                            AdapterType = device.GetType().Name
                        };

                        if (device.Backing is
                            VirtualEthernetCardDistributedVirtualPortBackingInfo)
                        {
                            var bInfo = device.Backing
                                as VirtualEthernetCardDistributedVirtualPortBackingInfo;
                            info.PortGroupKey = bInfo.Port.PortgroupKey;

                            // TODO: (gsinha) Is this the correct string setting below?
                            info.BackingType =
                                "VirtualEthernetCardDistributedVirtualPortBackingInfo";
                        }

                        mapKeyNic.Add(
                            new KeyValuePair<int, NetworkAdapterInfo>(device.Key, info));
                    }
                }
            }

            if (vmView.Guest.Net != null && vmView.Guest.Net.Count() > 0)
            {
                GuestNicInfo[] guestNicInfos = vmView.Guest.Net;
                foreach (var guestNicInfo in guestNicInfos)
                {
                    if (guestNicInfo.DeviceConfigId < 0)
                    {
                        T.Tracer().TraceWarning("Unable to find network adapter for {0}.",
                            guestNicInfo.MacAddress);
                        continue;
                    }

                    if (guestNicInfo.IpAddress == null)
                    {
                        continue;
                    }

                    string[] ipAddress = guestNicInfo.IpAddress;
                    NetworkData nwData = new NetworkData
                    {
                        MacAddress = guestNicInfo.MacAddress
                    };

                    NetworkAdapterInfo adapterInfo =
                        mapKeyNic.Find(o => o.Key == guestNicInfo.DeviceConfigId).Value;
                    nwData.NetworkLabel = adapterInfo.Label;
                    nwData.AddressType = adapterInfo.AddressType;
                    nwData.AdapterType = adapterInfo.AdapterType;
                    macInformationFetched[guestNicInfo.MacAddress] = nwData.NetworkLabel;

                    if (!string.IsNullOrEmpty(guestNicInfo.Network))
                    {
                        nwData.Network = guestNicInfo.Network;
                        if (adapterInfo.BackingType ==
                            "VirtualEthernetCardDistributedVirtualPortBackingInfo")
                        {
                            foreach (var pgInfo in dvPortGroupInfo)
                            {
                                if (adapterInfo.PortGroupKey ==
                                    pgInfo.DvPortGroupInfo.PortgroupKey)
                                {
                                    nwData.Network = pgInfo.PortGroupNameDVPG;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (string.IsNullOrEmpty(adapterInfo.PortGroupKey))
                        {
                            T.Tracer().TraceError(
                                "Unable to determine distributed port " +
                                "group name to which network adapter {0} is attached to.",
                                nwData.NetworkLabel);
                            nwData.Network = string.Empty;
                        }
                        else if (dvPortGroupInfo.Count == 0)
                        {
                            T.Tracer().TraceError(
                                "Found empty details in array dvPortGroupInfo.");
                            nwData.Network = string.Empty;
                        }
                        else
                        {
                            foreach (var pgInfo in dvPortGroupInfo)
                            {
                                if (adapterInfo.PortGroupKey ==
                                    pgInfo.DvPortGroupInfo.PortgroupKey)
                                {
                                    nwData.Network = pgInfo.PortGroupNameDVPG;
                                    break;
                                }
                            }
                        }
                    }

                    var ipv4Addresses = ipAddress.Where(ip => !ip.Contains("::"));
                    ipAddresses.AddRange(ipv4Addresses);
                    nwData.MacAssociatedIp = string.Join(",", ipv4Addresses);

                    if (guestNicInfo.DnsConfig != null &&
                        guestNicInfo.DnsConfig.IpAddress != null)
                    {
                        nwData.IsDhcp = guestNicInfo.DnsConfig.Dhcp;
                        nwData.DnsIp = string.Empty;
                        if (guestNicInfo.DnsConfig.IpAddress != null)
                        {
                            nwData.DnsIp =
                                string.Join(",", guestNicInfo.DnsConfig.IpAddress);
                        }
                    }
                    else
                    {
                        // This is an ESX of version less than ESX 4.1.
                        nwData.IsDhcp = false;
                        nwData.DnsIp = string.Empty;
                    }

                    // All done. Add it.
                    networkDataList.Add(nwData);
                }
            }
            else
            {
                // guest.net property is not set on the VM.
                List<string> dnsIp = new List<string>();
                bool dhcp = false;
                if (!string.IsNullOrEmpty(vmView.Guest.IpAddress))
                {
                    string ipAddress = vmView.Guest.IpAddress;
                    if (vmView.Guest.IpStack != null)
                    {
                        foreach (GuestStackInfo gsInfo in vmView.Guest.IpStack)
                        {
                            dhcp = gsInfo.DnsConfig.Dhcp;
                            dnsIp.AddRange(gsInfo.DnsConfig.IpAddress);
                        }
                    }
                }

                foreach (VirtualDevice device in devices)
                {
                    if (device.DeviceInfo.Label.StartsWith(
                        "Network adapter",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        NetworkData nwData = new NetworkData();
                        if (device is VirtualEthernetCard)
                        {
                            VirtualEthernetCard ethernetCard =
                                device as VirtualEthernetCard;
                            nwData.IsDhcp = dhcp;
                            nwData.DnsIp = string.Join(",", dnsIp);
                            nwData.MacAddress = ethernetCard.MacAddress;
                            nwData.NetworkLabel = device.DeviceInfo.Label;
                            nwData.AddressType = string.Empty;
                            nwData.AdapterType = device.GetType().Name;
                            nwData.MacAssociatedIp = string.Empty;
                            if (nwData.MacAddress != null)
                            {
                                macInformationFetched[nwData.MacAddress] =
                                    nwData.NetworkLabel;
                            }

                            if (device.DeviceInfo.Summary.Contains(
                                "DistributedVirtualPortBacking"))
                            {
                                T.Tracer().TraceError(
                                    "Found empty list in array dvPortGroupInfo");
                                if (dvPortGroupInfo == null || dvPortGroupInfo.Count == 0)
                                {
                                    nwData.Network = string.Empty;
                                }
                                else
                                {
                                    var bInfo = device.Backing
                                        as VirtualEthernetCardDistributedVirtualPortBackingInfo;
                                    foreach (var pgInfo in dvPortGroupInfo)
                                    {
                                        if (bInfo.Port.PortgroupKey ==
                                            pgInfo.DvPortGroupInfo.PortgroupKey)
                                        {
                                            nwData.Network = pgInfo.PortGroupNameDVPG;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                nwData.Network = device.DeviceInfo.Summary;
                            }

                            // All done. Add it.
                            networkDataList.Add(nwData);
                        }
                    }
                }
            }

            foreach (string macAddress in macAddressList.Keys)
            {
                if (macInformationFetched.ContainsKey(macAddress))
                {
                    continue;
                }

                NetworkData nwData = new NetworkData
                {
                    NetworkLabel = macAddressList[macAddress],
                    Network = string.Empty,
                    MacAddress = macAddress,
                    MacAssociatedIp = string.Empty,
                    DnsIp = string.Empty,
                    IsDhcp = false
                };

                networkDataList.Add(nwData);
            }

            if (ipAddresses.Count == 0)
            {
                ipAddresses.Add("NOT SET");
            }

            networkData = networkDataList;
        }

        /// <summary>
        /// Collects disk information for verifying cluster disk.
        /// </summary>
        /// <param name="vmViews">The virtual machine list.</param>
        /// <param name="hostGroup">The host in question.</param>
        /// <returns>List of the disk Uuids.</returns>
        private List<string> GetDiskInfo(List<VirtualMachine> vmViews, string hostGroup)
        {
            List<string> diskList = new List<string>();
            foreach (var vmView in vmViews)
            {
                try
                {
                    if (vmView.Summary.Runtime.Host == null)
                    {
                        continue;
                    }

                    string hostGroupVm = vmView.Summary.Runtime.Host.Value;
                    if (hostGroup == hostGroupVm &&
                        string.Compare(vmView.Name, "unknown", ignoreCase: true) != 0 &&
                        !vmView.Summary.Config.Template &&
                        vmView.Summary.Runtime.ConnectionState !=
                            VirtualMachineConnectionState.orphaned &&
                        vmView.Config.Hardware.Device != null &&
                        string.Compare(vmView.Guest.GuestState, "unknown", ignoreCase: true) != 0)
                    {
                        VirtualDevice[] devices = vmView.Config.Hardware.Device;
                        foreach (var device in devices)
                        {
                            if (device.DeviceInfo.Label.Contains(
                                "Hard Disk",
                                StringComparison.OrdinalIgnoreCase))
                            {
                                // Making the assumption that the UUID property in any of the possible
                                // disk types is named 'Uuid'. This is to avoid having to
                                // enumerate/handle all possible types (similar to the perl script).
                                Type diskType = device.Backing.GetType();
                                string vmdkUuid =
                                    (string)diskType.GetProperty("Uuid").GetValue(device.Backing);
                                PropertyInfo compatModeProperty =
                                    diskType.GetProperty("CompatibilityMode");
                                if (compatModeProperty != null)
                                {
                                    string compatabilityMode =
                                        (string)compatModeProperty.GetValue(device.Backing);
                                    if (string.Compare(
                                        compatabilityMode,
                                        "physicalmode",
                                        ignoreCase: true) == 0)
                                    {
                                        vmdkUuid =
                                            (string)diskType.GetProperty("LunUuid")
                                            .GetValue(device.Backing);
                                    }
                                }

                                diskList.Add(vmdkUuid);
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    // Bug 4705741
                    T.Tracer().TraceException(
                        ex,
                        "Unexpected exception: Failed to get disks of virtual machine {0}.",
                        vmView.Name);
                }
            }

            return diskList;
        }

        /// <summary>
        /// Gets the virtual machine properties to be collected.
        /// </summary>
        /// <returns>Virtual machine properties.</returns>
        private string[] GetVmProps()
        {
            List<string> vmProps = new List<string>
            {
                "name",
                "config.extraConfig",
                "config.guestFullName",
                "config.hardware.device",
                "config.version",
                "guest.guestState",
                "guest.hostName",
                "guest.ipAddress",
                "guest.net",
                "layout.disk",
                "parent",
                "resourcePool",
                "snapshot",
                "summary.config.ftInfo",
                "summary.config.guestFullName",
                "summary.config.guestId",
                "summary.config.instanceUuid",
                "summary.config.memorySizeMB",
                "summary.config.numCpu",
                "summary.config.numVirtualDisks",
                "summary.config.numEthernetCards",
                "summary.config.template",
                "summary.config.uuid",
                "summary.config.vmPathName",
                "summary.guest.toolsStatus",
                "summary.runtime.connectionState",
                "summary.runtime.host",
                "summary.runtime.powerState",
                "datastore"
            };

            Version apiVersion = new Version(this.client.ServiceContent.About.ApiVersion);
            if (apiVersion >= new Version(4, 1))
            {
                vmProps.Add("guest.ipStack");
                vmProps.Add("parentVApp");
            }

            if (apiVersion >= new Version(5, 0))
            {
                vmProps.Add("config.firmware");
            }

            return vmProps.ToArray();
        }

        /// <summary>
        /// Reconfigures the virtual machine with extra config information.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <param name="optionValues">New values.</param>
        private void ReconfigureVm(VirtualMachine vmView, List<OptionValue> optionValues)
        {
            var vmConfigSpec = new VirtualMachineConfigSpec();
            vmConfigSpec.ExtraConfig = optionValues.ToArray();

            bool keepTrying = true;

            // TODO (gsinha): Shall we implement time for 5 minutes?
            while (keepTrying)
            {
                try
                {
                    vmView.ReconfigVM(vmConfigSpec);
                    keepTrying = false;
                }
                catch (VimException ve)
                {
                    TaskInProgress tip = ve.MethodFault as TaskInProgress;
                    if (tip != null)
                    {
                        TimeSpan waitTime = TimeSpan.FromSeconds(2);
                        T.Tracer().TraceError(
                            "Waiting for {0} secs as another task '{1}' is in progress on " +
                            "vm {2}.",
                            waitTime.TotalSeconds,
                            tip.Task.Value,
                            vmView.Name);
                        System.Threading.Thread.Sleep(waitTime);
                        keepTrying = true;
                    }

                    T.Tracer().TraceException(
                        ve,
                        "Hit an exception while reconfiguring vm: {0} ({1}).",
                        vmView.Name,
                        vmView.MoRef.Value);
                    throw;
                }
            }
        }

        /// <summary>
        /// Finds SCSI controller Information in machine view.
        /// </summary>
        /// <param name="vmView">The virtual machine.</param>
        /// <param name="key">The SCSI controller key.</param>
        /// <returns>True if found. False if not.</returns>
        private bool FindScsiController(VirtualMachine vmView, int key)
        {
            foreach (VirtualDevice device in vmView.Config.Hardware.Device)
            {
                if (key == device.Key)
                {
                    T.Tracer().TraceInformation("Found SCSI controller with key: {0}.", key);
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Create/edit a new Virtual Disk spec. Disk can be either VMDK/RDM/RDMP.
        /// </summary>
        /// <param name="diskConfig">Disk information.</param>
        /// <param name="controllerKey">The controller key.</param>
        /// <param name="unitNumber">The unit number.</param>
        /// <param name="diskKey">The disk key.</param>
        /// <param name="rootFolderName">The root folder.</param>
        /// <param name="operation">Either create/add/edit.</param>
        /// <param name="diskName">The disk name.</param>
        /// <returns>The virtual disk spec.</returns>
        private VirtualDeviceConfigSpec VirtualDiskSpec(
            DiskInfo diskConfig,
            int controllerKey,
            int unitNumber,
            int diskKey,
            string rootFolderName,
            string operation,
            string diskName)
        {
            string[] diskPath = diskConfig.disk_name.Split(
                new string[] { "[", "]" },
                StringSplitOptions.RemoveEmptyEntries);
            diskPath[diskPath.Length - 1] = diskPath.Last().TrimStart();
            if (!string.IsNullOrEmpty(rootFolderName))
            {
                T.Tracer().TraceInformation("Root folder name: {0}.", rootFolderName);
                diskPath[diskPath.Length - 1] = rootFolderName + "/" + diskPath.Last();
            }

            string fileName = diskName;
            long fileSize = long.Parse(diskConfig.size);
            bool diskType = bool.Parse(diskConfig.thin_disk);
            string diskMode = diskConfig.independent_persistent;

            T.Tracer().TraceInformation(
                "Creating/reconfiguring ({0}) disk {1} of size {2} at SCSI unit number {3} of " +
                "type {4} in mode {5}. ControllerNumber: {6}, diskKey: {7}.",
                operation,
                fileName,
                fileSize,
                unitNumber,
                diskType ? "thin provisioned" : "not thin provisioned",
                diskMode,
                controllerKey,
                diskKey);
            var diskBackingInfo = new VirtualDiskFlatVer2BackingInfo
            {
                DiskMode = diskMode,
                FileName = fileName,
                ThinProvisioned = diskType,
                Uuid = diskConfig.target_disk_uuid
            };

            if (diskConfig.cluster_disk == "yes" ||
                !diskConfig.controller_mode.Contains(
                    "noSharing",
                    StringComparison.OrdinalIgnoreCase))
            {
                diskBackingInfo.EagerlyScrub = true;
                diskBackingInfo.ThinProvisioned = false;
            }

            if (diskConfig.disk_mode == "Mapped Raw LUN" &&
                bool.Parse(diskConfig.convert_rdm_to_vmdk) == false)
            {
                throw new NotImplementedException(
                    "TODO (gsinha): MakeScsiDiskNode()  not ported which brings in this data.");
            }

            var connectionInfo = new VirtualDeviceConnectInfo
            {
                AllowGuestControl = false,
                Connected = true,
                StartConnected = true
            };

            var virtualDisk = new VirtualDisk
            {
                ControllerKey = controllerKey,
                UnitNumber = unitNumber,
                Key = diskKey,
                Backing = diskBackingInfo,
                CapacityInKB = fileSize
            };

            var virtualDeviceSpec = new VirtualDeviceConfigSpec
            {
                Operation = VirtualDeviceConfigSpecOperation.add,
                Device = virtualDisk,
                FileOperation = VirtualDeviceConfigSpecFileOperation.create
            };

            if (operation == "edit" || operation == "add")
            {
                virtualDeviceSpec = new VirtualDeviceConfigSpec
                {
                    Operation = operation == "edit" ?
                        VirtualDeviceConfigSpecOperation.edit :
                        VirtualDeviceConfigSpecOperation.add,
                    Device = virtualDisk
                };
            }

            return virtualDeviceSpec;
        }

        /// <summary>
        /// Finds the Disk information. It maps SCSI ID and Disk Name.
        /// </summary>
        /// <param name="vmView">The VM view.</param>
        /// <param name="disksInfoInVm">The file name to key and its reverse map.</param>
        private void FindOccupiedScsiIds(
            VirtualMachine vmView,
            out Dictionary<string, string> disksInfoInVm)
        {
            disksInfoInVm = new Dictionary<string, string>();
            foreach (VirtualDevice device in vmView.Config.Hardware.Device)
            {
                disksInfoInVm[device.Key.ToString()] = device.DeviceInfo.Label;
                if (device.DeviceInfo.Label.Contains(
                    "Hard Disk",
                    StringComparison.OrdinalIgnoreCase))
                {
                    VirtualDeviceFileBackingInfo backingInfo =
                        (VirtualDeviceFileBackingInfo)device.Backing;
                    disksInfoInVm[device.Key.ToString()] = backingInfo.FileName;
                    disksInfoInVm[backingInfo.FileName] = device.Key.ToString();
                }
            }
        }

        /// <summary>
        /// Details gathered about a disk.
        /// </summary>
        public class DiskData
        {
            /// <summary>
            /// Gets or sets the capacity.
            /// </summary>
            public long? CapacityInBytes { get; set; }

            /// <summary>
            /// Gets or sets a value indicating "yes" or "no".
            /// </summary>
            public string ClusterDisk { get; set; }

            /// <summary>
            /// Gets or sets the controller mode.
            /// </summary>
            public string ControllerMode { get; set; }

            /// <summary>
            /// Gets or sets the controller type.
            /// </summary>
            public string ControllerType { get; set; }

            /// <summary>
            /// Gets or sets the disk mode.
            /// </summary>
            public string DiskMode { get; set; }

            /// <summary>
            /// Gets or sets the disk name.
            /// </summary>
            public string DiskName { get; set; }

            /// <summary>
            /// Gets or sets the disk object id.
            /// </summary>
            public string DiskObjectId { get; set; }

            /// <summary>
            /// Gets or sets the disk size.
            /// </summary>
            public long DiskSize { get; set; }

            /// <summary>
            /// Gets or sets the disk type.
            /// </summary>
            public string DiskType { get; set; }

            /// <summary>
            /// Gets or sets the disk uuid.
            /// </summary>
            public string DiskUuid { get; set; }

            /// <summary>
            /// Gets or sets a value indicating "ide" or "scsi".
            /// </summary>
            public string IdeOrScsi { get; set; }

            /// <summary>
            /// Gets or sets the independent persistent value.
            /// </summary>
            public string IndependentPersistent { get; set; }

            /// <summary>
            /// Gets or sets the scsi host.
            /// </summary>
            public string ScsiHost { get; set; }

            /// <summary>
            /// Gets or sets the SCSI VMX property.
            /// </summary>
            public string ScsiVmx { get; set; }
        }

        /// <summary>
        /// Information gathered about a network adapter.
        /// </summary>
        public class NetworkAdapterInfo
        {
            /// <summary>
            /// Gets or sets the adapter type.
            /// </summary>
            public string AdapterType { get; set; }

            /// <summary>
            /// Gets or sets the address type.
            /// </summary>
            public string AddressType { get; set; }

            /// <summary>
            /// Gets or sets the backing type.
            /// </summary>
            public string BackingType { get; set; }

            /// <summary>
            /// Gets or sets the label.
            /// </summary>
            public string Label { get; set; }

            /// <summary>
            /// Gets or sets the port group key.
            /// </summary>
            public string PortGroupKey { get; set; }
        }

        /// <summary>
        /// Details gathered about a network.
        /// </summary>
        public class NetworkData
        {
            /// <summary>
            /// Gets or sets the adapter type.
            /// </summary>
            public string AdapterType { get; set; }

            /// <summary>
            /// Gets or sets the address type.
            /// </summary>
            public string AddressType { get; set; }

            /// <summary>
            /// Gets or sets the DNS IP.
            /// </summary>
            public string DnsIp { get; set; }

            /// <summary>
            /// Gets or sets a value indicating whether DHCP is enabled.
            /// </summary>
            public bool IsDhcp { get; set; }

            /// <summary>
            /// Gets or sets the mac address.
            /// </summary>
            public string MacAddress { get; set; }

            /// <summary>
            /// Gets or sets the mac associated IP.
            /// </summary>
            public string MacAssociatedIp { get; set; }

            /// <summary>
            /// Gets or sets the network.
            /// </summary>
            public string Network { get; set; }

            /// <summary>
            /// Gets or sets the network label.
            /// </summary>
            public string NetworkLabel { get; set; }
        }

        /// <summary>
        /// Data gathered about a virtual machine on ESX/vCenter.
        /// </summary>
        public class VirtualMachineInfo
        {
            /// <summary>
            /// Whether VM is clustered ("yes" or "no").
            /// </summary>
            private string cluster;

            /// <summary>
            /// The data center name.
            /// </summary>
            private string dataCenterName;

            /// <summary>
            /// The data center MoRef Id.
            /// </summary>
            private string dataCenterRefId;

            /// <summary>
            /// The VM data store.
            /// </summary>
            private string dataStore;

            /// <summary>
            /// The disk data.
            /// </summary>
            private List<DiskData> diskData;

            /// <summary>
            /// The count of floppy devices.
            /// </summary>
            private int floppyCount;

            /// <summary>
            /// The VM folder path.
            /// </summary>
            private string folderPath;

            /// <summary>
            /// The host version.
            /// </summary>
            private string hostVersion;

            /// <summary>
            /// The count of IDE devices.
            /// </summary>
            private int ideCount;

            /// <summary>
            /// The IP addresses.
            /// </summary>
            private List<string> ipAddresses;

            /// <summary>
            /// The network data.
            /// </summary>
            private List<NetworkData> networkData;

            /// <summary>
            /// Whether raw disk mapping is on.
            /// </summary>
            private bool rdm;

            /// <summary>
            /// The resource pool name.
            /// </summary>
            private string resourcePoolName;

            /// <summary>
            /// The backing virtual machine object.
            /// </summary>
            private VirtualMachine vmView;

            /// <summary>
            /// The vSphere host for this virtual machine.
            /// </summary>
            private string vSphereHostName;

            /// <summary>
            /// Initializes a new instance of the <see cref="VirtualMachineInfo" /> class.
            /// </summary>
            /// <param name="vmView">The VM view.</param>
            /// <param name="vSphereHostName">The host name.</param>
            /// <param name="hostVersion">The host version.</param>
            /// <param name="ipAddresses">The IP addresses.</param>
            /// <param name="networkData">The networking data.</param>
            /// <param name="ideCount">The IDE device count.</param>
            /// <param name="floppyCount">The floppy device count.</param>
            /// <param name="rdm">Raw disk mapping.</param>
            /// <param name="cluster">"yes" or "no".</param>
            /// <param name="diskData">The disk data.</param>
            public VirtualMachineInfo(
                VirtualMachine vmView,
                string vSphereHostName,
                string hostVersion,
                List<string> ipAddresses,
                List<NetworkData> networkData,
                int ideCount,
                int floppyCount,
                bool rdm,
                string cluster,
                List<DiskData> diskData)
            {
                this.vmView = vmView;
                this.vSphereHostName = vSphereHostName;
                this.hostVersion = hostVersion;
                this.dataCenterName = string.Empty;
                this.ipAddresses = ipAddresses;
                this.networkData = networkData;
                this.ideCount = ideCount;
                this.floppyCount = floppyCount;
                this.rdm = rdm;
                this.cluster = cluster;
                this.diskData = diskData;
                this.PopuateProperties();
            }

            /// <summary>
            /// Gets the cluster property ("yes or "no").
            /// </summary>
            public string Cluster
            {
                get
                {
                    return this.cluster;
                }
            }

            /// <summary>
            /// Gets the data center name.
            /// </summary>
            public string DataCenterName
            {
                get
                {
                    return this.dataCenterName;
                }
            }

            /// <summary>
            /// Gets the data center MoRef Id.
            /// </summary>
            public string DataCenterRefId
            {
                get
                {
                    return this.dataCenterRefId;
                }
            }

            /// <summary>
            /// Gets the data store.
            /// </summary>
            public string DataStore
            {
                get
                {
                    return this.dataStore;
                }
            }

            /// <summary>
            /// Gets the disk data.
            /// </summary>
            public List<DiskData> DiskData
            {
                get
                {
                    return this.diskData;
                }
            }

            /// <summary>
            /// Gets a value indicating whether disk enable uuid is set.
            /// </summary>
            public bool DiskEnableUuid
            {
                get
                {
                    OptionValue[] extraConfig = this.vmView.Config.ExtraConfig;
                    foreach (OptionValue config in extraConfig)
                    {
                        if (string.Compare(config.Key, "disk.enableuuid", ignoreCase: true) == 0)
                        {
                            return bool.Parse((string)config.Value);
                        }
                    }

                    return false;
                }
            }

            /// <summary>
            /// Gets the display name.
            /// </summary>
            public string DisplayName
            {
                get
                {
                    return this.vmView.Name;
                }
            }

            /// <summary>
            /// Gets a value indicating whether Efi is enabled.
            /// </summary>
            public bool Efi
            {
                get
                {
                    return this.vmView.Config.Firmware == "efi";
                }
            }

            /// <summary>
            /// Gets the floppy count.
            /// </summary>
            public int FloppyCount
            {
                get
                {
                    return this.floppyCount;
                }
            }

            /// <summary>
            /// Gets the folder path.
            /// </summary>
            public string FolderPath
            {
                get
                {
                    return this.folderPath;
                }
            }

            /// <summary>
            /// Gets the guest Id.
            /// </summary>
            public string GuestId
            {
                get
                {
                    return this.vmView.Summary.Config.GuestId;
                }
            }

            /// <summary>
            /// Gets the host name.
            /// </summary>
            public string HostName
            {
                get
                {
                    return this.vmView.Guest.HostName;
                }
            }

            /// <summary>
            /// Gets the host version.
            /// </summary>
            public string HostVersion
            {
                get
                {
                    return this.hostVersion;
                }
            }

            /// <summary>
            /// Gets the IDE count.
            /// </summary>
            public int IdeCount
            {
                get
                {
                    return this.ideCount;
                }
            }

            /// <summary>
            /// Gets the IP Addresses. Contains "NOT SET" if nothing.
            /// </summary>
            public List<string> IpAddresses
            {
                get
                {
                    return this.ipAddresses;
                }
            }

            /// <summary>
            /// Gets the memory size.
            /// </summary>
            public int? MemorySizeMB
            {
                get
                {
                    return this.vmView.Summary.Config.MemorySizeMB;
                }
            }

            /// <summary>
            /// Gets the network data.
            /// </summary>
            public List<NetworkData> NetworkData
            {
                get
                {
                    return this.networkData;
                }
            }

            /// <summary>
            /// Gets the number of CPUs.
            /// </summary>
            public int? NumCpu
            {
                get
                {
                    return this.vmView.Summary.Config.NumCpu;
                }
            }

            /// <summary>
            /// Gets the number of virtual disks.
            /// </summary>
            public int? NumVirtualDisks
            {
                get
                {
                    return this.vmView.Summary.Config.NumVirtualDisks;
                }
            }

            /// <summary>
            /// Gets the operating system.
            /// </summary>
            public string OperatingSystem
            {
                get
                {
                    return this.vmView.Summary.Config.GuestFullName;
                }
            }

            /// <summary>
            /// Gets the power state property.
            /// </summary>
            public VirtualMachinePowerState PowerState
            {
                get
                {
                    return this.vmView.Summary.Runtime.PowerState;
                }
            }

            /// <summary>
            /// Gets a value indicating whether RDM is present or not.
            /// </summary>
            public bool Rdm
            {
                get
                {
                    return this.rdm;
                }
            }

            /// <summary>
            /// Gets or sets the resource pool property.
            /// </summary>
            public string ResourcePool
            {
                get
                {
                    if (!string.IsNullOrEmpty(this.resourcePoolName))
                    {
                        return this.resourcePoolName;
                    }

                    if (!this.Template)
                    {
                        return this.vmView.ResourcePool.Value;
                    }
                    else
                    {
                        return null;
                    }
                }

                set
                {
                    this.resourcePoolName = value;
                }
            }

            /// <summary>
            /// Gets the resource pool group property.
            /// </summary>
            public string ResourcePoolGroup
            {
                get
                {
                    return this.vmView.ResourcePool != null ? this.vmView.ResourcePool.Value : null;
                }
            }

            /// <summary>
            /// Gets a value indicating whether VM is a template or not.
            /// </summary>
            public bool Template
            {
                get
                {
                    return this.vmView.Summary.Config.Template;
                }
            }

            /// <summary>
            /// Gets the VM tools status.
            /// </summary>
            public VirtualMachineToolsStatus? ToolsStatus
            {
                get
                {
                    if (this.vmView.Summary.Guest != null)
                    {
                        return this.vmView.Summary.Guest.ToolsStatus;
                    }

                    return null;
                }
            }

            /// <summary>
            /// Gets the uuid.
            /// </summary>
            public string Uuid
            {
                get
                {
                    return this.vmView.Summary.Config.Uuid;
                }
            }

            /// <summary>
            /// Gets the instance uuid.
            /// </summary>
            public string InstanceUuid
            {
                get
                {
                    return this.vmView.Summary.Config.InstanceUuid;
                }
            }

            /// <summary>
            /// Gets the vm console url.
            /// </summary>
            public string VmConsoleUrl
            {
                get
                {
                    // TODO (gsinha): Implement if really needed.
                    return "NA";
                }
            }

            /// <summary>
            /// Gets the VM path name.
            /// </summary>
            public string VmPathName
            {
                get
                {
                    return this.vmView.Summary.Config.VmPathName;
                }
            }

            /// <summary>
            /// Gets the VMX version.
            /// </summary>
            public string VmxVersion
            {
                get
                {
                    return this.vmView.Config.Version;
                }
            }

            /// <summary>
            /// Gets the vSphere host name.
            /// </summary>
            public string VSphereHostName
            {
                get
                {
                    return this.vSphereHostName;
                }
            }

            /// <summary>
            /// Fill properties that are not directly backed by the vm view and we need to do some
            /// more leg work.
            /// </summary>
            private void PopuateProperties()
            {
                // Fill any data center name.
                if (this.vmView.Parent != null)
                {
                    ManagedObjectReference parent = this.vmView.Parent;
                    while (!parent.Type.Contains(
                        "Datacenter",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        ViewBase view = this.vmView.Client.GetView(
                            parent,
                            new string[] { "parent" });
                        parent =
                            (ManagedObjectReference)view.GetType()
                            .GetProperty("Parent").GetValue(view);
                    }

                    Datacenter dc =
                        (Datacenter)this.vmView.Client.GetView(parent, new string[] { "name" });
                    this.dataCenterName = dc.Name;
                    this.dataCenterRefId = dc.MoRef.Value;
                }
                else if (this.vmView.ParentVApp != null)
                {
                    ManagedObjectReference parent = this.vmView.ParentVApp;
                    while (!parent.Type.Contains(
                        "Datacenter",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        ViewBase view = this.vmView.Client.GetView(
                            parent,
                            new string[] { "parent" });
                        parent =
                            (ManagedObjectReference)view.GetType()
                            .GetProperty("Parent").GetValue(view);
                    }

                    Datacenter dc =
                        (Datacenter)this.vmView.Client.GetView(parent, new string[] { "name" });
                    this.dataCenterName = dc.Name;
                    this.dataCenterRefId = dc.MoRef.Value;
                }

                // Fill data store, folder path.
                // Eg: [datastore1] TestVm1/TestVm1.vmx
                string vmPathName = this.vmView.Summary.Config.VmPathName;
                string[] splitPath =
                    vmPathName.Split(
                    new string[] { "[", "]" },
                    StringSplitOptions.RemoveEmptyEntries);
                this.dataStore = splitPath[0];
                int finalSlash = splitPath[1].LastIndexOf('/');
                this.folderPath = splitPath[1].Substring(0, finalSlash + 1).Trim();
            }
        }

        /// <summary>
        /// Disk output from ConfigureDisks().
        /// </summary>
        public class CreatedDiskData
        {
            /// <summary>
            /// Gets or sets the disk name.
            /// </summary>
            public string DiskName { get; set; }

            /// <summary>
            /// Gets or sets the inmage host id.
            /// </summary>
            public string InMageHostId { get; set; }
        }

        /// <summary>
        /// Information about the SCSI controller being defined.
        /// </summary>
        private class ScsiControllerDefined
        {
            /// <summary>
            /// Gets or sets the controller key value if one was added.
            /// </summary>
            public int? Defined { get; set; }

            /// <summary>
            /// Gets or sets the value whether it already existed.
            /// </summary>
            public string Exists { get; set; }
        }
    }
}