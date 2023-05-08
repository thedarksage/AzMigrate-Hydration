//---------------------------------------------------------------
//  <copyright file="HostOpts.cs" company="Microsoft">
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
using System.Linq;
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware host operations.
    /// </summary>
    public class HostOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// Initializes a new instance of the <see cref="HostOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        public HostOpts(VMware.Vim.VimClient client)
        {
            this.client = client;
        }

        /// <summary>
        /// Gets Distributed Virtual PortGroupInfo on vSphere/ESX host.
        /// </summary>
        /// <param name="hostViews">Host View(s) of vSphere/ESX.</param>
        /// <returns>Returns list of DV portGroups mapping to Host.</returns>
        public List<DistributedVirtualPortgroupInfoEx> GetDvPortGroups(
            IList<HostSystem> hostViews)
        {
            var distVSwitchMng = (DistributedVirtualSwitchManager)this.client.GetView(
                this.client.ServiceContent.DvSwitchManager,
                null);

            var result = new List<DistributedVirtualPortgroupInfoEx>();
            foreach (var hostView in hostViews)
            {
                string hostName = hostView.Name;
                DVSManagerDvsConfigTarget dvsConfigTarget =
                    distVSwitchMng.QueryDvsConfigTarget(hostView.MoRef, dvs: null);
                DistributedVirtualPortgroupInfo[] dvPortGroup =
                    dvsConfigTarget.DistributedVirtualPortgroup;
                if (dvPortGroup == null || dvPortGroup.Count() == 0)
                {
                    T.Tracer().TraceInformation(
                        "Distributed virtual port groups " +
                        "are not exist on host {0}.",
                        hostName);
                    continue;
                }

                foreach (DistributedVirtualPortgroupInfo portGroupInfo in dvPortGroup)
                {
                    if (!portGroupInfo.UplinkPortgroup)
                    {
                        try
                        {
                            result.Add(
                                new DistributedVirtualPortgroupInfoEx(
                                    hostName,
                                    portGroupInfo));
                        }
                        catch (Exception)
                        {
                            // Skipping this distributed virtual port group.
                            // Any of the machines using this port group are to be skipped.
                        }
                    }
                }

                T.Tracer().TraceInformation(
                    "Successfully discovered distributed " +
                    "virtual switch configuration on host {0}.",
                    hostName);
            }

            return result;
        }

        /// <summary>
        /// Finds configuration information of each of the host in the HostViews.
        /// </summary>
        /// <param name="hostViews">The host views.</param>
        /// <returns>THe host configuration information.</returns>
        public List<HostConfigInfoEx> GetHostConfigInfo(List<HostSystem> hostViews)
        {
            T.Tracer().TraceInformation("Discovering host configuration information.");

            List<HostConfigInfoEx> hcInfoList = new List<HostConfigInfoEx>();
            foreach (HostSystem hostView in hostViews)
            {
                HostConfigInfoEx hcInfo = new HostConfigInfoEx();
                hcInfo.VSphereHostName = hostView.Name;
                hcInfo.Cpu = hostView.Summary.Hardware.NumCpuCores;

                double memSizeMb = (double)hostView.Summary.Hardware.MemorySize / (1024 * 1024);
                hcInfo.Memory = string.Format("{0:0.00}", memSizeMb);
                hcInfoList.Add(hcInfo);
            }

            return hcInfoList;
        }

        /// <summary>
        /// List all the portGroups on ESX host.
        /// </summary>
        /// <param name="hostViews">HostView specific to vSphere.</param>
        /// <returns>List of PortGroups mapping to Host.</returns>
        public List<PortGroupInfo> GetHostPortGroups(List<HostSystem> hostViews)
        {
            T.Tracer().TraceInformation("Discovering port group information.");

            List<PortGroupInfo> pgInfoList = new List<PortGroupInfo>();

            foreach (HostSystem hostView in hostViews)
            {
                string hostName = hostView.Name;
                hostView.UpdateViewData(new string[] { "config.network.portgroup" });
                HostPortGroup[] portGroups = hostView.Config.Network.Portgroup;
                foreach (HostPortGroup portGroup in portGroups)
                {
                    if (portGroup.Port != null)
                    {
                        bool skip = false;
                        foreach (HostPortGroupPort port in portGroup.Port)
                        {
                            if (port.Type == "host" || port.Type == "systemManagement")
                            {
                                skip = true;
                                break;
                            }
                        }

                        if (skip)
                        {
                            continue;
                        }
                    }

                    PortGroupInfo pgInfo = new PortGroupInfo();
                    pgInfo.VSphereHostName = hostName;
                    pgInfo.PortGroupName = portGroup.Spec.Name;
                    pgInfoList.Add(pgInfo);
                }
            }

            return pgInfoList;
        }

        /// <summary>
        /// Gets host views on vCenter/vSphere.
        /// </summary>
        /// <returns>The host views.</returns>
        public IList<HostSystem> GetHostViewsByProps()
        {
            var filter = new NameValueCollection();
            filter.Add("runtime.powerState", "poweredOn");

            IList<EntityViewBase> hostViews = this.client.FindEntityViews(
                viewType: typeof(HostSystem),
                beginEntity: null,
                filter: filter,
                properties: this.GetHostProps());

            if (hostViews == null)
            {
                T.Tracer().TraceError("No HostSystems found.");
                hostViews = new List<EntityViewBase>();
            }

            var newHostViews = new List<HostSystem>();
            foreach (var item in hostViews)
            {
                HostSystem hs = (HostSystem)item;
                if (hs.Summary.Runtime.InMaintenanceMode)
                {
                    T.Tracer().TraceInformation("{0} is in Maintenance Mode.", hs.Name);
                    continue;
                }

                newHostViews.Add(hs);
            }

            return newHostViews;
        }

        /// <summary>
        /// Gets host view of ESX. This is called when we are having multiple host view in case of
        /// vCenter.
        /// </summary>
        /// <param name="vSphereHostname">Host name for which the view has to be Found.</param>
        /// <returns>The host view.</returns>
        public HostSystem GetSubHostView(string vSphereHostname)
        {
            EntityViewBase view = this.client.FindEntityView(
                typeof(HostSystem),
                beginEntity: null,
                filter: new NameValueCollection { { "name", vSphereHostname } },
                properties: this.GetHostProps());
            if (view == null)
            {
                throw VMwareClientException.HostNotFound(vSphereHostname);
            }

            return (HostSystem)view;
        }

        /// <summary>
        /// Gets host view of ESX. This called the host is not managed by vCenter so there is
        /// only one.
        /// </summary>
        /// <returns>The host view.</returns>
        public HostSystem GetHostView()
        {
            T.Tracer().DebugAssert(
                this.client.ServiceContent.About.ApiType.Contains("HostAgent"),
                "GetHostView is only valid for use in non-vCenter scenario.");

            EntityViewBase view = this.client.FindEntityView(
                typeof(HostSystem),
                beginEntity: null,
                filter: null,
                properties: this.GetHostProps());
            T.Tracer().DebugAssert(
                view != null,
                "Not expecting host to be null in non-vCenter scenario.");

            return (HostSystem)view;
        }

        /// <summary>
        /// Merge the dvport group info and host portgroup info into Network Information on
        /// vCenter/vSphere.
        /// </summary>
        /// <param name="pgInfo">The host port group info.</param>
        /// <param name="dvPortGroupInfo">The dvport group info.</param>
        /// <returns>List of NetworkInfo objects.</returns>
        public NetworkInfo MergeNetworkInfo(
            List<PortGroupInfo> pgInfo,
            List<DistributedVirtualPortgroupInfoEx> dvPortGroupInfo)
        {
            return new NetworkInfo
            {
                PortGroupInfo = pgInfo,
                DvPortGroupInfo = dvPortGroupInfo
            };
        }

        /// <summary>
        /// Gets the host properties to be collected.
        /// </summary>
        /// <returns>The host properties.</returns>
        private string[] GetHostProps()
        {
            List<string> hostProps = new List<string>
            {
                "name",
                "datastoreBrowser",
                "parent",
                "configManager.datastoreSystem",
                "configManager.storageSystem",
                "summary.config.product.version",
                "summary.runtime.inMaintenanceMode",
                "summary.hardware.numCpuCores",
                "summary.hardware.memorySize",
                "summary.hardware.uuid"
            };

            return hostProps.ToArray();
        }

        /// <summary>
        /// Wraps a DistributedVirtualPortgroupInfo with some more data.
        /// </summary>
        public class DistributedVirtualPortgroupInfoEx
        {
            /// <summary>
            /// Initializes a new instance of the <see cref="DistributedVirtualPortgroupInfoEx" /> class.
            /// </summary>
            /// <param name="hostName">The host name.</param>
            /// <param name="portGroupInfo">A DistributedVirtualPortgroupInfo object.</param>
            public DistributedVirtualPortgroupInfoEx(
                string hostName,
                DistributedVirtualPortgroupInfo portGroupInfo)
            {
                this.VSphereHostName = hostName;
                this.DvPortGroupInfo = portGroupInfo;
                this.PortGroupNameDVPG = portGroupInfo.PortgroupName + "[DVPG]";
            }

            /// <summary>
            /// Gets the DistributedVirtualPortgroupInfo object.
            /// </summary>
            public DistributedVirtualPortgroupInfo DvPortGroupInfo { get; private set; }

            /// <summary>
            /// Gets the port group name DVPG.
            /// </summary>
            public string PortGroupNameDVPG { get; private set; }

            /// <summary>
            /// Gets the host name.
            /// </summary>
            public string VSphereHostName { get; private set; }
        }

        /// <summary>
        /// The host configuration information.
        /// </summary>
        public class HostConfigInfoEx
        {
            /// <summary>
            /// Gets or sets the CPUs.
            /// </summary>
            public short Cpu { get; set; }

            /// <summary>
            /// Gets or sets the memory.
            /// </summary>
            public string Memory { get; set; }

            /// <summary>
            /// Gets or sets the vSphere host name.
            /// </summary>
            public string VSphereHostName { get; set; }
        }

        /// <summary>
        /// The network information.
        /// </summary>
        public class NetworkInfo
        {
            /// <summary>
            /// Gets or sets the DV port group info.
            /// </summary>
            public List<DistributedVirtualPortgroupInfoEx> DvPortGroupInfo { get; set; }

            /// <summary>
            /// Gets or sets the port group info.
            /// </summary>
            public List<PortGroupInfo> PortGroupInfo { get; set; }
        }

        /// <summary>
        /// The port group information.
        /// </summary>
        public class PortGroupInfo
        {
            /// <summary>
            /// Gets or sets the port group name.
            /// </summary>
            public string PortGroupName { get; set; }

            /// <summary>
            /// Gets or sets the vSphere host name.
            /// </summary>
            public string VSphereHostName { get; set; }
        }
    }
}
