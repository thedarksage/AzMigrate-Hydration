//---------------------------------------------------------------
//  <copyright file="VmCreateInfo.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  VM creation contract.
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
    /// VM creation contract.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Reviewed.")]
    public class VmCreateInfo
    {
        /// <summary>
        /// Gets or sets the desired VM UUID.
        /// </summary>
        public string target_uuid { get; set; }

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
        public string vmx_path { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string folder_path { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string vmDirectoryPath { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string alt_guest_name { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public List<DiskInfo> disk { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string vmx_version { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string hostversion { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether EFI is enabled.
        /// </summary>
        public bool efi { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string resourcepoolgrpname { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string hostname { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether vsan is in play.
        /// </summary>
        public bool vsan { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string vsan_folder { get; set; }
    }
}
