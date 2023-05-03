using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class HardwareConfiguration
    {
        public int CPUCount { get; private set; }
        public long Memory { get; private set; }

        public HardwareConfiguration(ParameterGroup hardwareConfiguration)
        {
            this.CPUCount = int.Parse(hardwareConfiguration.GetParameterValue("CPUCount"));
            this.Memory = long.Parse(hardwareConfiguration.GetParameterValue("Memory"));
        }
    }
}
