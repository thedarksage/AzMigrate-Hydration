//---------------------------------------------------------------
//  <copyright file="DiskInfo.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Disk info contract.
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
    /// Disk info contract.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Reviewed.")]
    public class DiskInfo
    {
        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string cluster_disk { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string controller_mode { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string controller_type { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string convert_rdm_to_vmdk { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string datastore_selected { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string disk_mode { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string disk_name { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string disk_signature { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string independent_persistent { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string @protected { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string scsi_mapping_vmx { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string size { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string source_disk_uuid { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string target_disk_uuid { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string thin_disk { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string selected { get; set; }
    }
}
