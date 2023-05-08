using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace cspsconfigtool
{
    internal class CmdArgsParser
    {
        internal const string OptionEnableInstallerMode = "-EnableInstallerMode";
        
        internal static Dictionary<string, object> Parse(string[] args)
        {
            Dictionary<string, object> nameValues = new Dictionary<string, object>();
            if (args != null)
            {
                for (int index = 0; index < args.Length; index++)
                {
                    if (args[index].StartsWith("-"))
                    {
                        nameValues[args[index]] = ((index + 1) < args.Length && !args[index + 1].StartsWith("-")) ? args[index + 1] : true as object;
                        index++;
                    }
                }
            }
            return nameValues;
        }
    }
}
