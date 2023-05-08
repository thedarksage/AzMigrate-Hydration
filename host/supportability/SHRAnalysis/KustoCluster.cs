using System;
using System.Collections.Generic;
using System.Data;
using System.Linq;

namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    class SHRConstants
    {
        public static IDictionary<string, string> MABClusterMap = new Dictionary<string, string>()
        {
            {"https://mabprodweu.kusto.windows.net",  "MABKustoProd"},
            {"https://mabprod1.kusto.windows.net",  "MABKustoProd1"},
            {"https://mabprodwus.kusto.windows.net",  "MABKustoProd"}
        };

        public static IDictionary<string, string> ASRClusterMap = new Dictionary<string, string>()
        {
            {"Asrcluswe",  "ASRKustoDB_Europe"},
            {"Asrclussea",  "ASRKustoDB_Asia"},
            {"Asrcluscus",  "ASRKustoDB_US"},
            {"Asrclus1",  "ASRKustoDB"}
        };

    };
}