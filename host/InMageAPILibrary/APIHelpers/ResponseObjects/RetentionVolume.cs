using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class RetentionVolume
    {
        public string Id { get; private set; }
        public string VolumeName { get; private set; }
        public long Capacity { get; private set; }
        public long FreeSpace { get; private set; }
        public int RetentionSpaceThreshold { get; private set; }

        public RetentionVolume(ParameterGroup retentionVolume)
        {
            this.Id = retentionVolume.Id;
            this.VolumeName = retentionVolume.GetParameterValue("VolumeName");
            this.Capacity = long.Parse(retentionVolume.GetParameterValue("Capacity"));
            this.FreeSpace = long.Parse(retentionVolume.GetParameterValue("FreeSpace"));
            int value;
            int.TryParse(retentionVolume.GetParameterValue("RetentionSpaceThreshold"), out value);
            this.RetentionSpaceThreshold = value;
        }
    }
}
