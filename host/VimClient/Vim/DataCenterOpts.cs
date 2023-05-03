//---------------------------------------------------------------
//  <copyright file="DataCenterOpts.cs" company="Microsoft">
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
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware data center operations.
    /// </summary>
    public class DataCenterOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// Initializes a new instance of the <see cref="DataCenterOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        public DataCenterOpts(VMware.Vim.VimClient client)
        {
            this.client = client;
        }

        /// <summary>
        /// Gets view of a specific data center on vCenter/vSphere.
        /// </summary>
        /// <param name="datacenterName">Data center name for which view is to be found.</param>
        /// <returns>The data center view.</returns>
        public Datacenter GetDataCenterView(string datacenterName)
        {
            string encName = datacenterName.EncodeNameSpecialChars();
            EntityViewBase view = this.client.FindEntityView(
                typeof(Datacenter),
                beginEntity: null,
                filter: new NameValueCollection { { "name", encName } },
                properties: this.GetDataCenterProps());
            if (view == null)
            {
                throw VMwareClientException.DataCenterNotFound(datacenterName);
            }

            return (Datacenter)view;
        }

        /// <summary>
        /// Gets host views on vCenter/vSphere.
        /// </summary>
        /// <returns>The host views.</returns>
        public IList<Datacenter> GetDataCenterViewsByProps()
        {
            IList<EntityViewBase> dataCenterViews =
                this.client.FindEntityViews(
                    viewType: typeof(Datacenter),
                    beginEntity: null,
                    filter: null,
                    properties: this.GetDataCenterProps());

            if (dataCenterViews == null)
            {
                T.Tracer().TraceError("No Datacenter views found.");
                dataCenterViews = new List<EntityViewBase>();
            }

            List<Datacenter> newDataCenterViews = new List<Datacenter>();
            foreach (var item in dataCenterViews)
            {
                newDataCenterViews.Add((Datacenter)item);
            }

            return newDataCenterViews;
        }

        /// <summary>
        /// Collects the data center Information.
        /// </summary>
        /// <param name="datacenterViews">The data center views.</param>
        /// <returns>List of DataCenterInfo.</returns>
        public List<DataCenterInfo> GetDataCenterInfo(List<Datacenter> datacenterViews)
        {
            T.Tracer().TraceInformation("Discovering data center information.");

            List<DataCenterInfo> dcInfoList = new List<DataCenterInfo>();
            List<ManagedObjectReference> dcArray = new List<ManagedObjectReference>();
            foreach (Datacenter dcView in datacenterViews)
            {
                var dataCenterInfo = new DataCenterInfo();
                dataCenterInfo.Name = dcView.Name;
                dataCenterInfo.RefId = dcView.MoRef.Value;
                dcInfoList.Add(dataCenterInfo);
            }

            return dcInfoList;
        }

        /// <summary>
        /// Gets the data center properties to be collected.
        /// </summary>
        /// <returns>The data center properties</returns>
        private string[] GetDataCenterProps()
        {
            List<string> dataCenterProps = new List<string>
            {
                 "name",
                 "datastore",
                 "vmFolder"
            };

            return dataCenterProps.ToArray();
        }

        /// <summary>
        /// The data store information that is gathered.
        /// </summary>
        public class DataCenterInfo
        {
            /// <summary>
            /// Gets or sets the data center name.
            /// </summary>
            public string Name { get; set; }

            /// <summary>
            /// Gets or sets the MoRef Id.
            /// </summary>
            public string RefId { get; set; }
        }
    }
}
