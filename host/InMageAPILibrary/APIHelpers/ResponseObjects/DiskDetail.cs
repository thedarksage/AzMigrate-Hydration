using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class DiskDetail
    {
        public string Id { get; private set; }
        public string DiskName { get; private set; }
        public string DiskId { get; private set; }
        public long Capacity { get; private set; }
        public bool SystemVolume { get; private set; }
        public bool IsDynamicDisk { get; private set; }

        public DiskDetail(ParameterGroup diskDetail)
        {
            this.Id = diskDetail.Id;
            this.DiskName = diskDetail.GetParameterValue("DiskName");
            this.DiskId = diskDetail.GetParameterValue("DiskId");
            this.Capacity = long.Parse(diskDetail.GetParameterValue("Capacity"));
            this.SystemVolume = (diskDetail.GetParameterValue("SystemVolume") == "Yes") ? true : false;
            this.IsDynamicDisk = (diskDetail.GetParameterValue("DynamicDisk") == "Yes") ? true : false;
        }
    }
}
