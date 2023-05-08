//---------------------------------------------------------------
//  <copyright file="VmConfigureDiskInfo.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  VM disk configuration contract.
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
    /// VM disk configuration contract.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Reviewed.")]
    public class VmConfigureDiskInfo
    {
        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string inmage_hostid { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string clusternodes_inmageguids { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string vmDirectoryPath { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string vsan_folder { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public string machinetype { get; set; }

        /// <summary>
        /// Gets or sets the property value.
        /// </summary>
        public List<DiskInfo> disk { get; set; }
    }
}
