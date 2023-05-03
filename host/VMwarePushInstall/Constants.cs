using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VMwareClient
{
    /// <summary>
    ///  Defines the constants for the project.
    /// </summary>
    public static class Constants
    {
        /// <summary>
        /// Constants for push install job.
        /// </summary>
        public static class PushInstallJobConstants
        {
            /// <summary>
            /// Represents the caller stack type.
            /// </summary>
            public static class StackType
            {
                /// <summary>
                /// CS stack type.
                /// </summary>
                public static string Cs = "CS";

                /// <summary>
                /// RCM stack type.
                /// </summary>
                public static string Rcm = "RCM";
            }
        }
    }
}
