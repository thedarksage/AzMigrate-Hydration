using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class DiskMappingForProtection
    {
        public string SourceDiskName { get; private set; }
        public string TargetLUNId { get; private set; }

        public DiskMappingForProtection(string sourceDiskName, int targetLUNId)
        {
            this.SourceDiskName = sourceDiskName;
            this.TargetLUNId = targetLUNId.ToString();
        }
    }
}
