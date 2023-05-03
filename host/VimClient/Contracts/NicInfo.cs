//---------------------------------------------------------------
//  <copyright file="NicInfo.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  NIC info contract.
//  </summary>
//
//  History:     28-July-2015   GSinha   Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VMware.Client
{
    /// <summary>
    /// NIC info contract.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Reviewed.")]
    public class NicInfo
    {
        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string adapter_type { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string address_type { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string dhcp { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string dnsip { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string gateway { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string ip { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string mask { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string network_label { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string network_name { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string new_dnsip { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string new_gateway { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string new_ip { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string new_network_name { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string new_macaddress { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string nic_mac { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string virtual_ips { get; set; }
    }
}
