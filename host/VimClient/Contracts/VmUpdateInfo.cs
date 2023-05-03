//---------------------------------------------------------------
//  <copyright file="VmUpdateInfo.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  VM update contract.
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
    /// VM update contract.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Reviewed.")]
    public class VmUpdateInfo
    {
        /// <summary>
        /// Gets or sets the host display name.
        /// </summary>
        public string display_name { get; set; }

        /// <summary>
        /// Gets or sets the memory size.
        /// </summary>
        public string memsize { get; set; }

        /// <summary>
        /// Gets or sets the CPU count.
        /// </summary>
        public string cpucount { get; set; }

        /// <summary>
        /// Gets or sets the new display name.
        /// </summary>
        public string new_displayname { get; set; }

        /// <summary>
        /// Gets or sets the ide count.
        /// </summary>
        public string ide_count { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether Uuid is enabled for disk.
        /// </summary>
        public string diskenableuuid { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public int floppy_device_count { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public List<NicInfo> nic_info { get; set; }
    }
}
