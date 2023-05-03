using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    class Clusters
    {
        public static List<string> ClusterNames= new List<string>(){"Asrcluscus","Asrcluswe","Asrclussea"};

        public static IDictionary<string, string> MABClusterMap = new Dictionary<string, string>()
        {
            {"Mabprodweu", "MABKustoProd"},
            {"Mabprod1", "MABKustoProd1"},
            {"Mabprodwus", "MABKustoProd"}
        };

        public static IDictionary<string, string> ASRClusterMap = new Dictionary<string, string>()
        {
            {"Asrcluswe", "ASRKustoDB_Europe"},
            {"Asrclussea", "ASRKustoDB_Asia"},
            {"Asrcluscus", "ASRKustoDB_US"},
            {"Asrclus1", "ASRKustoDB"}
        };
    }
}
