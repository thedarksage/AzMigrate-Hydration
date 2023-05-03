//---------------------------------------------------------------
//  <copyright file="ResourcePoolOpts.cs" company="Microsoft">
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
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware resource pool operations.
    /// </summary>
    public class ResourcePoolOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// Initializes a new instance of the <see cref="ResourcePoolOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        public ResourcePoolOpts(VMware.Vim.VimClient client)
        {
            this.client = client;
        }

        /// <summary>
        /// Finds resource pool information like pool name, group name, type of pool and its type
        /// and also maps to its host.
        /// </summary>
        /// <param name="resourcePoolViews">The resource pool views.</param>
        /// <param name="hostViews">The host views.</param>
        /// <returns>A list of ResourcePoolInfo.</returns>
        public List<ResourcePoolInfo> FindResourcePoolInfo(
            List<ResourcePool> resourcePoolViews,
            List<HostSystem> hostViews)
        {
            T.Tracer().TraceInformation("Discovering resource pool information.");

            var rPoolInfoList = new List<ResourcePoolInfo>();
            var mapHostAndParent = new Dictionary<string, string>();
            foreach (var hostView in hostViews)
            {
                mapHostAndParent[hostView.Name] = hostView.Parent.Value;
            }

            var vAppPoolMap = new Dictionary<string, string>();
            foreach (var rPoolView in resourcePoolViews)
            {
                if (rPoolView.MoRef == null ||
                    string.Compare(rPoolView.MoRef.Type, "virtualapp", ignoreCase: true) != 0)
                {
                    continue;
                }

                vAppPoolMap[rPoolView.MoRef.Value] = rPoolView.MoRef.Type;
            }

            foreach (var rPoolView in resourcePoolViews)
            {
                ResourcePoolInfo info = new ResourcePoolInfo();
                info.GroupName = rPoolView.MoRef.Value;
                info.Name = rPoolView.Name;
                info.Owner = rPoolView.Owner.Value;
                info.OwnerType = rPoolView.Owner.Type;
                info.ResPoolType = rPoolView.MoRef.Type;

                ResourcePool parentPool = rPoolView;
                while (string.Compare(
                    parentPool.Parent.Type,
                    "resourcepool",
                    ignoreCase: true) == 0)
                {
                    parentPool =
                        (ResourcePool)rPoolView.Client.GetView(parentPool.Parent, null);
                }

                if (vAppPoolMap.ContainsKey(parentPool.MoRef.Value) ||
                    vAppPoolMap.ContainsKey(parentPool.Parent.Value))
                {
                    continue;
                }

                foreach (var host in mapHostAndParent.Keys)
                {
                    if (mapHostAndParent[host] == info.Owner)
                    {
                        info.InHost = host;
                        if (string.Compare(
                            info.OwnerType,
                            "clustercomputeresource",
                            ignoreCase: true) == 0)
                        {
                            rPoolInfoList.Add(info);
                            continue;
                        }

                        rPoolInfoList.Add(info);
                        break;
                    }
                }
            }

            return rPoolInfoList;
        }

        /// <summary>
        /// Gets host views on vCenter/vSphere.
        /// </summary>
        /// <returns>The host views.</returns>
        public IList<ResourcePool> GetResourcePoolViewsByProps()
        {
            IList<EntityViewBase> resourcePoolViews =
                this.client.FindEntityViews(
                    viewType: typeof(ResourcePool),
                    beginEntity: null,
                    filter: null,
                    properties: this.GetResourcePoolProps());

            if (resourcePoolViews == null)
            {
                T.Tracer().TraceError("No Resourcepools found.");
                resourcePoolViews = new List<EntityViewBase>();
            }

            List<ResourcePool> newResourcePoolViews = new List<ResourcePool>();
            foreach (var item in resourcePoolViews)
            {
                newResourcePoolViews.Add((ResourcePool)item);
            }

            return newResourcePoolViews;
        }

        /// <summary>
        /// Maps resource pool group to its names. At VM level we have only the resource pool
        /// group name but not their names as displayed on vCenter/vSphere.
        /// </summary>
        /// <param name="resourcePoolViews">The resource pool views.</param>
        /// <returns>A map of resource pool group name and its name as displayed at vSphere level.
        /// </returns>
        public Dictionary<string, string> MapResourcePool(List<ResourcePool> resourcePoolViews)
        {
            var mapResourceGroupName = new Dictionary<string, string>();
            foreach (ResourcePool rPoolView in resourcePoolViews)
            {
                if (string.IsNullOrEmpty(rPoolView.MoRef.Value))
                {
                    continue;
                }

                string resourceGroup = rPoolView.MoRef.Value;
                string resourcePoolName = rPoolView.Name;
                mapResourceGroupName[resourceGroup] = resourcePoolName;
            }

            return mapResourceGroupName;
        }

        /// <summary>
        /// Finds resource pool reference using resource pool group ID.
        /// </summary>
        /// <param name="resourcePoolGroupId">Resource Pool Group ID.</param>
        /// <returns>ResourcePool if found. Throws if not.</returns>
        public ResourcePool FindResourcePool(string resourcePoolGroupId)
        {
            IList<ResourcePool> resPoolViews = this.GetResourcePoolViewsByProps();
            foreach (var pool in resPoolViews)
            {
                if (pool.MoRef.Value == resourcePoolGroupId)
                {
                    return pool;
                }
            }

            throw VMwareClientException.ResourcePoolNotFound(resourcePoolGroupId);
        }

        /// <summary>
        /// Moves the Vm To ResourcePool.
        /// </summary>
        /// <param name="resourcePoolView">The resource pool view.</param>
        /// <param name="vmView">The vm view.</param>
        public void MoveVmToResourcePool(ResourcePool resourcePoolView, VirtualMachine vmView)
        {
            try
            {
                resourcePoolView.MoveIntoResourcePool(
                    new ManagedObjectReference[] { vmView.MoRef });
            }
            catch (Exception e)
            {
                T.Tracer().TraceException(
                    e,
                    "Unable to move virtual machine {0} to resource pool {1}.",
                    vmView.Name,
                    resourcePoolView.Name);
                throw;
            }
        }

        /// <summary>
        /// Gets the resource pool properties to be collected.
        /// </summary>
        /// <returns>The resource pool properties</returns>
        private string[] GetResourcePoolProps()
        {
            List<string> dataCenterProps = new List<string>
            {
                 "name",
                 "parent",
                 "owner"
            };

            return dataCenterProps.ToArray();
        }

        /// <summary>
        /// The resource pool information.
        /// </summary>
        public class ResourcePoolInfo
        {
            /// <summary>
            /// Gets or sets the group name.
            /// </summary>
            public string GroupName { get; set; }

            /// <summary>
            /// Gets or sets the host.
            /// </summary>
            public string InHost { get; set; }

            /// <summary>
            /// Gets or sets the name.
            /// </summary>
            public string Name { get; set; }

            /// <summary>
            /// Gets or sets the owner.
            /// </summary>
            public string Owner { get; set; }

            /// <summary>
            /// Gets or sets the owner type.
            /// </summary>
            public string OwnerType { get; set; }

            /// <summary>
            /// Gets or sets the resource pool type.
            /// </summary>
            public string ResPoolType { get; set; }
        }
    }
}
