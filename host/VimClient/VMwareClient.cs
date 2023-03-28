//---------------------------------------------------------------
//  <copyright file="VMwareClient.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     02-Oct-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security;
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// Operating system type enum.
    /// </summary>
    public enum OsType
    {
        /// <summary>
        /// All OS types will discovered.
        /// </summary>
        All = 0,

        /// <summary>
        /// Windows VMs discovery.
        /// </summary>
        Windows = 1,

        /// <summary>
        /// Linux VMs discovery.
        /// </summary>
        Linux = 2
    }

    /// <summary>
    /// Helper class for managing VMWare ESX/vCenter.
    /// </summary>
    public partial class VMwareClient
    {
        /// <summary>
        /// The host operations helper.
        /// </summary>
        private HostOpts hostOpts;

        /// <summary>
        /// The VM operations helper.
        /// </summary>
        private VmOpts vmOpts;

        /// <summary>
        /// The resource pool operations helper.
        /// </summary>
        private ResourcePoolOpts resourcePoolOpts;

        /// <summary>
        /// The data center operations helper.
        /// </summary>
        private DataCenterOpts dataCenterOpts;

        /// <summary>
        /// The data store operations helper.
        /// </summary>
        private DatastoreOpts datastoreOpts;

        /// <summary>
        /// The folder operations helper.
        /// </summary>
        private FolderOpts folderOpts;

        /// <summary>
        /// The ESX/vCenter IP address.
        /// </summary>
        private string serverIpAddress;

        /// <summary>
        /// The Vim client.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// Initializes a new instance of the <see cref="VMwareClient" /> class.
        /// </summary>
        /// <param name="ip">The ESX/vCenter IP address.</param>
        /// <param name="username">The user name.</param>
        /// <param name="password">The password.</param>
        public VMwareClient(string ip, string username, SecureString password)
            : this(ip, username, VimExtensions.SecureStringToString(password))
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="VMwareClient" /> class.
        /// </summary>
        /// <param name="ip">The ESX/vCenter IP address.</param>
        /// <param name="username">The user name.</param>
        /// <param name="password">The password.</param>
        public VMwareClient(string ip, string username, string password)
        {
            this.client = new VimClientImpl();
            this.serverIpAddress = ip;
            string url = "https://" + ip + "/sdk";
            T.Tracer().TraceInformation("Connecting to vCenter/vSphere Server: " + ip);
            ServiceContent sc;
            try
            {
                sc = this.client.Connect(url);
                UserSession userSession = this.client.Login(username, password);

                T.Tracer().TraceInformation(
                    "Successfully connected to vCenter/vSphere Server: " + ip);

                // Setting session locale to en_US, need to handle non-english messages
                SessionManager sessionManager =
                    (SessionManager)this.client.GetView(sc.SessionManager, null);
                sessionManager.SetLocale("en_US");

                this.IsvCenter = sc.About.ApiType.Contains("VirtualCenter");
                this.VCenterVersion = sc.About.Version;

                this.hostOpts = new HostOpts(this.client);
                this.resourcePoolOpts = new ResourcePoolOpts(this.client);
                this.dataCenterOpts = new DataCenterOpts(this.client);
                this.datastoreOpts = new DatastoreOpts(this.client, this.dataCenterOpts);
                this.vmOpts = new VmOpts(
                    this.client,
                    this.datastoreOpts,
                    this.dataCenterOpts);
                this.folderOpts = new FolderOpts(
                    this.client,
                    this.resourcePoolOpts,
                    this.datastoreOpts,
                    this.vmOpts);

                if (this.IsvCenter)
                {
                    this.InstanceUuid = sc.About.InstanceUuid;
                }
                else
                {
                    // Stand alone ESXi host don't have a notion of InstanceUuid. Use the
                    // machine's uuid.
                    HostSystem hostView = this.hostOpts.GetHostView();
                    this.InstanceUuid = hostView.Summary.Hardware.Uuid;
                }
            }
            catch (Exception exception)
            {
                T.Tracer().TraceException(
                    exception,
                    "Hit an exception while connecting to vCenter/vSphere.");
                throw;
            }
        }

        /// <summary>
        /// Gets a value indicating whether the server is vCenter or not.
        /// </summary>
        public bool IsvCenter { get; private set; }

        /// <summary>
        /// Gets the vCenter/ESX version.
        /// </summary>
        public string VCenterVersion { get; private set; }

        /// <summary>
        /// Gets the instance UUID for this vCenter (not applicable for ESXi host).
        /// </summary>
        public string InstanceUuid { get; private set; }

        /// <summary>
        /// Gets the ESX/vCenter IP address.
        /// </summary>
        public string ServerIpAddress
        {
            get
            {
                return this.serverIpAddress;
            }
        }

        /// <summary>
        /// Gets the host operations helper.
        /// </summary>
        public HostOpts HostOpts
        {
            get
            {
                return this.hostOpts;
            }
        }

        /// <summary>
        /// Gets the VM operations helper.
        /// </summary>
        public VmOpts VmOpts
        {
            get
            {
                return this.vmOpts;
            }
        }

        /// <summary>
        /// Gets the resource pool operations helper.
        /// </summary>
        public ResourcePoolOpts ResourcePoolOpts
        {
            get
            {
                return this.resourcePoolOpts;
            }
        }

        /// <summary>
        /// Gets the data center operations helper.
        /// </summary>
        public DataCenterOpts DataCenterOpts
        {
            get
            {
                return this.dataCenterOpts;
            }
        }

        /// <summary>
        /// Gets the data store operations helper.
        /// </summary>
        public DatastoreOpts DatastoreOpts
        {
            get
            {
                return this.datastoreOpts;
            }
        }

        /// <summary>
        /// Gets the folder operations helper.
        /// </summary>
        public FolderOpts FolderOpts
        {
            get
            {
                return this.folderOpts;
            }
        }

        /// <summary>
        /// Gets the Vim client.
        /// </summary>
        public VMware.Vim.VimClient SdkClient
        {
            get
            {
                return this.client;
            }
        }

        /// <summary>
        /// Returns the ESX/vCenter layout.
        /// </summary>
        /// <param name="osType">The OS type.</param>
        /// <returns>A INFO object.</returns>
        public InfrastructureView GetInfrastructureView(OsType osType)
        {
            List<VirtualMachine> vmViews;
            List<HostSystem> hostViews;
            List<Datacenter> datacenterViews;
            List<ResourcePool> resourcePoolViews;

            try
            {
                this.GetViews(
                    out vmViews,
                    out hostViews,
                    out datacenterViews,
                    out resourcePoolViews);

                List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo =
                    this.hostOpts.GetDvPortGroups(hostViews);

                List<VmOpts.VirtualMachineInfo> vmInfo = this.GetVmInfo(
                    osType,
                    vmViews,
                    hostViews,
                    resourcePoolViews,
                    dvPortGroupInfo);

                List<ResourcePoolOpts.ResourcePoolInfo> resourcePoolInfo =
                    this.resourcePoolOpts.FindResourcePoolInfo(
                        resourcePoolViews,
                        hostViews);

                List<DataCenterOpts.DataCenterInfo> dcInfo =
                    this.dataCenterOpts.GetDataCenterInfo(datacenterViews);

                List<DatastoreOpts.DataStoreInfo> dsInfo =
                    this.datastoreOpts.GetDataStoreInfo(hostViews, datacenterViews);

                List<HostOpts.PortGroupInfo> pgInfo =
                    this.hostOpts.GetHostPortGroups(hostViews);

                HostOpts.NetworkInfo networkInfo =
                    this.hostOpts.MergeNetworkInfo(pgInfo, dvPortGroupInfo);

                List<HostOpts.HostConfigInfoEx> configInfo =
                    this.hostOpts.GetHostConfigInfo(hostViews);

                // Gathered all the data. Lets build the INFO object.
                InfrastructureView esxInfo = this.MakeInfoObject(
                    osType,
                    vmInfo,
                    dsInfo,
                    dcInfo,
                    networkInfo,
                    configInfo,
                    resourcePoolInfo);

                return esxInfo;
            }
            catch (Exception exception)
            {
                T.Tracer().TraceException(
                    exception,
                    "Hit an exception while fetching information from vCenter/vSphere.");
                throw;
            }
        }

        /// <summary>
        /// Returns the vCenter/ESX layout of required properties.
        /// This method is used by Process Server to do its periodic discovery.
        /// Changes here will affect it.
        /// </summary>
        /// <param name="options">List of required properties.</param>
        /// <param name="osType">The OS type.</param>
        /// <returns>A Info Object.</returns>
        public InfrastructureView GetInfrastructureViewLite(
            string[] options,
            OsType osType)
        {
            List<VirtualMachine> vmViews = new List<VirtualMachine>();
            List<HostSystem> hostViews = new List<HostSystem>();
            List<Datacenter> datacenterViews = new List<Datacenter>();
            List<ResourcePool> resourcePoolViews = new List<ResourcePool>();

            T.Tracer().TraceInformation("Selected options: " + string.Join(",", options));

            try
            {
                hostViews = this.hostOpts.GetHostViewsByProps().ToList();

                List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo =
                    new List<HostOpts.DistributedVirtualPortgroupInfoEx>();
                List<VmOpts.VirtualMachineInfo> vmInfo =
                    new List<VmOpts.VirtualMachineInfo>();
                List<DatastoreOpts.DataStoreInfo> dsInfo =
                    new List<DatastoreOpts.DataStoreInfo>();
                HostOpts.NetworkInfo networkInfo = new HostOpts.NetworkInfo();
                List<ResourcePoolOpts.ResourcePoolInfo> resourcePoolInfo =
                    new List<ResourcePoolOpts.ResourcePoolInfo>();
                List<DataCenterOpts.DataCenterInfo> dcInfo =
                    new List<DataCenterOpts.DataCenterInfo>();
                List<HostOpts.HostConfigInfoEx> configInfo =
                    new List<HostOpts.HostConfigInfoEx>();

                if (options.Contains("hosts") || options.Contains("networks"))
                {
                    dvPortGroupInfo = this.hostOpts.GetDvPortGroups(hostViews);
                }

                if (options.Contains("hosts") || options.Contains("resourcepools"))
                {
                    resourcePoolViews =
                        this.resourcePoolOpts.GetResourcePoolViewsByProps().ToList();
                }

                if (options.Contains("hosts"))
                {
                    vmViews = this.vmOpts.GetVmViewsByProps().ToList();
                    vmInfo = this.GetVmInfo(
                        osType,
                        vmViews,
                        hostViews,
                        resourcePoolViews,
                        dvPortGroupInfo);
                }

                if (options.Contains("datacenters") || options.Contains("datastores"))
                {
                    datacenterViews =
                        this.dataCenterOpts.GetDataCenterViewsByProps().ToList();
                }

                if (options.Contains("datastores"))
                {
                    dsInfo =
                        this.datastoreOpts.GetDataStoreInfo(hostViews, datacenterViews);
                }

                if (options.Contains("networks"))
                {
                    List<HostOpts.PortGroupInfo> pgInfo =
                        this.hostOpts.GetHostPortGroups(hostViews);
                    networkInfo = this.hostOpts.MergeNetworkInfo(pgInfo, dvPortGroupInfo);
                }

                if (options.Contains("resourcepools"))
                {
                    resourcePoolInfo =
                        this.resourcePoolOpts.FindResourcePoolInfo(
                            resourcePoolViews,
                            hostViews);
                }

                if (options.Contains("datacenters"))
                {
                    dcInfo = this.dataCenterOpts.GetDataCenterInfo(datacenterViews);
                }

                if (options.Contains("configs"))
                {
                    configInfo = this.hostOpts.GetHostConfigInfo(hostViews);
                }

                // Gathered all the data. Lets build the INFO object.
                InfrastructureView esxInfo = this.MakeInfoObject(
                    osType,
                    vmInfo,
                    dsInfo,
                    dcInfo,
                    networkInfo,
                    configInfo,
                    resourcePoolInfo);

                return esxInfo;
            }
            catch (Exception exception)
            {
                T.Tracer().TraceException(
                    exception,
                    "Hit an exception while fetching the required information" +
                    "from vCenter/vSphere.");
                throw;
            }
        }

        /// <summary>
        /// OS type enum for the OS type string.
        /// </summary>
        /// <param name="osType">OS type string.</param>
        /// <returns>OS type enum.</returns>
        public OsType GetOsTypeEnum(string osType = null)
        {
            OsType osTypeEnum = OsType.All;
            if (string.IsNullOrEmpty(osType))
            {
                osTypeEnum = OsType.All;
            }
            else if (osType.Equals("linux", StringComparison.OrdinalIgnoreCase))
            {
                osTypeEnum = OsType.Linux;
            }
            else if (osType.Equals("windows", StringComparison.OrdinalIgnoreCase))
            {
                osTypeEnum = OsType.Windows;
            }

            return osTypeEnum;
        }

        /// <summary>
        /// Gets view of VirtualMachine, Host, DataCenter, ResourcePool.
        /// </summary>
        /// <param name="vmViews">The VM view.</param>
        /// <param name="hostViews">The host view.</param>
        /// <param name="dataCenterViews">The data center view.</param>
        /// <param name="resourcePoolViews">The resource pool view.</param>
        private void GetViews(
            out List<VirtualMachine> vmViews,
            out List<HostSystem> hostViews,
            out List<Datacenter> dataCenterViews,
            out List<ResourcePool> resourcePoolViews)
        {
            try
            {
                vmViews = this.vmOpts.GetVmViewsByProps().ToList();
                hostViews = this.hostOpts.GetHostViewsByProps().ToList();
                dataCenterViews = this.dataCenterOpts.GetDataCenterViewsByProps().ToList();
                resourcePoolViews =
                    this.resourcePoolOpts.GetResourcePoolViewsByProps().ToList();
            }
            catch (Exception exception)
            {
                T.Tracer().TraceException(
                    exception,
                    "Hit an exception while collecting the views from vCenter/vSphere.");
                throw;
            }
        }

        /// <summary>
        /// Collects VM information.
        /// </summary>
        /// <param name="osType">The OS type.</param>
        /// <param name="vmViews">The vm views.</param>
        /// <param name="hostViews">The host views.</param>
        /// <param name="resourcePoolViews">The resource pool views.</param>
        /// <param name="dvPortGroupInfo">The port group info.</param>
        /// <returns>The virtual machines information.</returns>
        private List<VmOpts.VirtualMachineInfo> GetVmInfo(
            OsType osType,
            List<VirtualMachine> vmViews,
            List<HostSystem> hostViews,
            List<ResourcePool> resourcePoolViews,
            List<HostOpts.DistributedVirtualPortgroupInfoEx> dvPortGroupInfo)
        {
            var vmInfoList = new List<VmOpts.VirtualMachineInfo>();
            foreach (var hostView in hostViews)
            {
                string hostGroup = hostView.MoRef.Value;
                string vSphereHostName = hostView.Name;
                string hostVersion = hostView.Summary.Config.Product.Version;
                T.Tracer().TraceInformation(
                    "Collecting details of virtual machines on host: {0}, "
                    + "hostVersion: {1}.",
                    vSphereHostName,
                    hostVersion);

                List<VmOpts.VirtualMachineInfo> vmInfo = this.vmOpts.GetVmData(
                    vmViews,
                    hostGroup,
                    vSphereHostName,
                    hostVersion,
                    osType,
                    dvPortGroupInfo);
                vmInfoList.AddRange(vmInfo);
            }

            Dictionary<string, string> mapResourceGroupName =
                this.resourcePoolOpts.MapResourcePool(resourcePoolViews);
            foreach (var vm in vmInfoList)
            {
                if (!string.IsNullOrEmpty(vm.ResourcePool))
                {
                    vm.ResourcePool = mapResourceGroupName[vm.ResourcePool];
                }
            }

            return vmInfoList;
        }

        /// <summary>
        /// Constructs the INFO object that can back Info.xml generation.
        /// </summary>
        /// <param name="osType">The OS type.</param>
        /// <param name="vmInfo">The virtual machine/host information.</param>
        /// <param name="dsInfo">The data store information.</param>
        /// <param name="dcInfo">The data center information.</param>
        /// <param name="networkInfo">The network information.</param>
        /// <param name="configInfo">The host configuration information.</param>
        /// <param name="resourcePoolInfo">The resource pool information.</param>
        /// <returns>The INFO object.</returns>
        private InfrastructureView MakeInfoObject(
            OsType osType,
            List<VmOpts.VirtualMachineInfo> vmInfo,
            List<DatastoreOpts.DataStoreInfo> dsInfo,
            List<DataCenterOpts.DataCenterInfo> dcInfo,
            HostOpts.NetworkInfo networkInfo,
            List<HostOpts.HostConfigInfoEx> configInfo,
            List<ResourcePoolOpts.ResourcePoolInfo> resourcePoolInfo)
        {
            T.Tracer().TraceInformation("Collecting discovered information.");

            InfrastructureView esxInfo = new InfrastructureView();
            esxInfo.vCenter = this.IsvCenter ? "yes" : "no";
            esxInfo.version = this.VCenterVersion;

            List<INFOHost> hostList = new List<INFOHost>();
            foreach (var vm in vmInfo)
            {
                INFOHost host = new INFOHost();
                host.display_name = vm.DisplayName;
                host.hostname = vm.HostName ?? "NOT SET";
                host.ip_address = string.Join(",", vm.IpAddresses);
                host.vSpherehostname = vm.VSphereHostName;
                host.datacenter = vm.DataCenterName;
                host.datacenter_refid = vm.DataCenterRefId;
                host.resourcepool = vm.ResourcePool;
                host.resourcepoolgrpname = vm.ResourcePoolGroup;
                host.hostversion = vm.HostVersion;
                host.powered_status =
                    vm.PowerState != VirtualMachinePowerState.poweredOn ? "OFF" : "ON";
                host.vmx_path = vm.VmPathName;
                host.source_uuid = vm.Uuid;
                host.source_vc_id = vm.InstanceUuid;
                host.memsize = vm.MemorySizeMB.GetValueOrDefault().ToString();
                host.cpucount = vm.NumCpu.GetValueOrDefault().ToString();
                host.vmx_version = vm.VmxVersion;
                host.vmwaretools = vm.ToolsStatus.GetValueOrDefault().ToXmlString();
                host.ide_count = vm.IdeCount.ToString();
                host.alt_guest_name = vm.GuestId;
                host.floppy_device_count = vm.FloppyCount.ToString();
                host.folder_path = vm.FolderPath;
                host.datastore = vm.DataStore;
                host.os_info = osType.ToString();
                host.operatingsystem = vm.OperatingSystem;
                host.rdm = vm.Rdm ? "TRUE" : "FALSE";
                host.number_of_disks = vm.NumVirtualDisks.GetValueOrDefault();
                host.cluster = vm.Cluster;
                host.efi = vm.Efi;
                host.diskenableuuid = vm.DiskEnableUuid ? "yes" : "no";
                host.vm_console_url = vm.VmConsoleUrl;
                host.template = vm.Template;

                this.MakeDiskInfo(host, vm.DiskData);
                this.MakeNicInfo(host, vm.NetworkData);
                hostList.Add(host);
            }

            esxInfo.host = hostList.ToArray();
            this.MakeDataCenterInfo(esxInfo, dcInfo);
            this.MakeDatastoreInfo(esxInfo, dsInfo);
            this.MakeNetworkInfo(esxInfo, networkInfo);
            this.MakeResourcePoolInfo(esxInfo, resourcePoolInfo);
            this.MakeConfigInfo(esxInfo, configInfo);

            return esxInfo;
        }

        /// <summary>
        /// Arranges all the host configuration information.
        /// </summary>
        /// <param name="esxInfo">The root info node.</param>
        /// <param name="configInfoList">The config information.</param>
        private void MakeConfigInfo(
            InfrastructureView esxInfo,
            List<HostOpts.HostConfigInfoEx> configInfoList)
        {
            List<INFOConfig> configs = new List<INFOConfig>();

            if (configInfoList.Count == 0)
            {
                T.Tracer().TraceInformation("Datastores list is empty.");
            }
            else
            {
                foreach (var configInfo in configInfoList)
                {
                    INFOConfig config = new INFOConfig();
                    config.vSpherehostname = configInfo.VSphereHostName;
                    config.cpucount = configInfo.Cpu;
                    config.memsize = configInfo.Memory;
                    configs.Add(config);
                }
            }

            esxInfo.config = configs.ToArray();
        }

        /// <summary>
        /// Arranges all resource pool information.
        /// </summary>
        /// <param name="esxInfo">The root info node.</param>
        /// <param name="resourcePoolInfoList">The resource pool information.</param>
        private void MakeResourcePoolInfo(
            InfrastructureView esxInfo,
            List<ResourcePoolOpts.ResourcePoolInfo> resourcePoolInfoList)
        {
            List<INFOResourcepool> resourcePools = new List<INFOResourcepool>();

            if (resourcePoolInfoList.Count == 0)
            {
                T.Tracer().TraceInformation("Resourcepool list is empty.");
            }
            else
            {
                foreach (var rPoolInfo in resourcePoolInfoList)
                {
                    INFOResourcepool pool = new INFOResourcepool();
                    pool.host = rPoolInfo.InHost;
                    pool.name = rPoolInfo.Name;
                    pool.groupId = rPoolInfo.GroupName;
                    pool.type = rPoolInfo.ResPoolType;
                    pool.owner = rPoolInfo.Owner;
                    pool.owner_type = rPoolInfo.OwnerType;
                    resourcePools.Add(pool);
                }
            }

            esxInfo.resourcepool = resourcePools.ToArray();
        }

        /// <summary>
        /// Arranges all network information.
        /// </summary>
        /// <param name="esxInfo">The root info node.</param>
        /// <param name="networkInfo">The network information.</param>
        private void MakeNetworkInfo(
            InfrastructureView esxInfo,
            HostOpts.NetworkInfo networkInfo)
        {
            List<INFONetwork> networks = new List<INFONetwork>();
            if (networkInfo.PortGroupInfo == null)
            {
                T.Tracer().TraceInformation("Network port group list is empty.");
            }
            else
            {
                foreach (HostOpts.PortGroupInfo pgInfo in networkInfo.PortGroupInfo)
                {
                    INFONetwork network = new INFONetwork();
                    network.vSpherehostname = pgInfo.VSphereHostName;
                    network.name = pgInfo.PortGroupName;
                    network.switchUuid = string.Empty;
                    network.portgroupKey = string.Empty;
                    networks.Add(network);
                }
            }

            if (networkInfo.DvPortGroupInfo == null)
            {
                T.Tracer().TraceInformation("Network dv port list is empty.");
            }
            else
            {
                foreach (HostOpts.DistributedVirtualPortgroupInfoEx dvPgInfo
                    in networkInfo.DvPortGroupInfo)
                {
                    INFONetwork network = new INFONetwork();
                    network.vSpherehostname = dvPgInfo.VSphereHostName;
                    network.name = dvPgInfo.PortGroupNameDVPG;
                    network.switchUuid = dvPgInfo.DvPortGroupInfo.SwitchUuid;
                    network.portgroupKey = dvPgInfo.DvPortGroupInfo.PortgroupKey;

                    if (!string.IsNullOrEmpty(dvPgInfo.DvPortGroupInfo.PortgroupType) &&
                        !this.IsvCenter &&
                        dvPgInfo.DvPortGroupInfo.PortgroupType != "ephemeral")
                    {
                        continue;
                    }

                    networks.Add(network);
                }
            }

            esxInfo.network = networks.ToArray();
        }

        /// <summary>
        /// Arranges all data center information.
        /// </summary>
        /// <param name="esxInfo">The root info node.</param>
        /// <param name="dcInfoList">The data center information.</param>
        private void MakeDataCenterInfo(
            InfrastructureView esxInfo,
            List<DataCenterOpts.DataCenterInfo> dcInfoList)
        {
            List<INFODataCenter> dataCenters = new List<INFODataCenter>();
            if (dcInfoList.Count == 0)
            {
                T.Tracer().TraceInformation("Datacenters list is empty.");
            }
            else
            {
                foreach (var dcInfo in dcInfoList)
                {
                    INFODataCenter datacenter = new INFODataCenter();
                    datacenter.datacenter_name = dcInfo.Name;
                    datacenter.refid = dcInfo.RefId;
                    dataCenters.Add(datacenter);
                }
            }

            esxInfo.datacenter = dataCenters.ToArray();
        }

        /// <summary>
        /// Arranges all datastore information.
        /// </summary>
        /// <param name="esxInfo">The root info node.</param>
        /// <param name="dsInfoList">The data store information.</param>
        private void MakeDatastoreInfo(
            InfrastructureView esxInfo,
            List<DatastoreOpts.DataStoreInfo> dsInfoList)
        {
            List<INFODatastore> datastores = new List<INFODatastore>();

            if (dsInfoList.Count == 0)
            {
                T.Tracer().TraceInformation("Datastores list is empty.");
            }
            else
            {
                foreach (var dsInfo in dsInfoList)
                {
                    INFODatastore datastore = new INFODatastore();
                    datastore.datastore_name = dsInfo.SymbolicName;
                    datastore.vSpherehostname = dsInfo.VSphereHostName;
                    datastore.total_size = dsInfo.Capacity;
                    datastore.free_space = dsInfo.FreeSpace;
                    datastore.datastore_blocksize_mb = dsInfo.BlockSize;
                    datastore.filesystem_version = dsInfo.FileSystem;
                    datastore.type = dsInfo.Type;
                    datastore.uuid = dsInfo.Uuid;
                    datastore.disk_name = dsInfo.DiskName;
                    datastore.display_name = dsInfo.DisplayName;
                    datastore.vendor = dsInfo.Vendor;
                    datastore.lun = dsInfo.LunNumber;
                    datastore.mount_path = dsInfo.Name;
                    datastore.refid = dsInfo.RefId;
                    datastores.Add(datastore);
                }
            }

            esxInfo.datastore = datastores.ToArray();
        }

        /// <summary>
        /// Arranges all data of nics in a host.
        /// </summary>
        /// <param name="host">The host in question.</param>
        /// <param name="networkDataList">NICs of the host.</param>
        private void MakeNicInfo(INFOHost host, List<VmOpts.NetworkData> networkDataList)
        {
            List<INFOHostNic> nics = new List<INFOHostNic>();
            foreach (var nwInfo in networkDataList)
            {
                INFOHostNic nic = new INFOHostNic();
                nic.network_label = nwInfo.NetworkLabel;
                nic.network_name = nwInfo.Network;
                nic.nic_mac = nwInfo.MacAddress;
                nic.ip = nwInfo.MacAssociatedIp;
                nic.address_type = nwInfo.AddressType;
                nic.adapter_type = nwInfo.AdapterType;
                nic.dhcp = nwInfo.IsDhcp ? "1" : "0";
                nic.dnsip = nwInfo.DnsIp;
                nics.Add(nic);
            }

            host.nic = nics.ToArray();
        }

        /// <summary>
        /// Arranges all data of disks in a host.
        /// </summary>
        /// <param name="host">The host in question.</param>
        /// <param name="diskDataList">Disks of the host.</param>
        private void MakeDiskInfo(INFOHost host, List<VmOpts.DiskData> diskDataList)
        {
            List<INFOHostDisk> disks = new List<INFOHostDisk>();
            foreach (var diskInfo in diskDataList)
            {
                INFOHostDisk disk = new INFOHostDisk();
                disk.disk_name = diskInfo.DiskName;
                disk.size = diskInfo.DiskSize;
                disk.disk_type = diskInfo.DiskType;
                disk.disk_mode = diskInfo.DiskMode;
                disk.source_disk_uuid = diskInfo.DiskUuid;
                disk.ide_or_scsi = diskInfo.IdeOrScsi;
                disk.scsi_mapping_vmx = diskInfo.ScsiVmx;
                disk.scsi_mapping_host = diskInfo.ScsiHost;
                disk.independent_persistent = diskInfo.IndependentPersistent;
                disk.controller_type = diskInfo.ControllerType;
                disk.controller_mode = diskInfo.ControllerMode;
                disk.cluster_disk = diskInfo.ClusterDisk;
                disk.disk_object_id = diskInfo.DiskObjectId;
                disk.capacity_in_bytes =
                    diskInfo.CapacityInBytes.GetValueOrDefault().ToString();
                disks.Add(disk);
            }

            host.disk = disks.ToArray();
        }
    }
}
