using Kusto.Data.Net.Client;
using System;
using System.Collections.Generic;

namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    class BiosIdAnalyzer
    {
        public static void AnalyzeBiosIds()
        {
            IDictionary<string, string> CSHostIdBiosId = new Dictionary<string, string>();
            IDictionary<string, string> SRCHostIdBiosId = new Dictionary<string, string>();
            IDictionary<string, string> SRCHostIdOSType = new Dictionary<string, string>();

            var qCSBiosId = @"SRSErrorEvent 
                                        | where TIMESTAMP > ago(180d)
                                        | where ErrorType == ""ProviderError""
                                        | where DRAErrorDetails contains ""VX agent registered to CX with details ""
                                        | extend HostId = extract(""hostid::([^,]+)"", 1, DRAErrorDetails)
                                        | extend patchDetails = extract(""patchDetails([^ ]+)"", 1, DRAErrorDetails)
                                        | where patchDetails != ""(,""
                                        | extend args = extract_all(""([^,]+)"", patchDetails)
                                        | project HostId, args
                                        | summarize  argset = make_list(args) by HostId
                                        | where argset[14] != """"
                                        | extend BiosIdStr = tostring(argset[14])
                                        | extend CsRegisteredBiosID = extract(""\""([^\""]+)"", 1, BiosIdStr)
                                        | distinct HostId, CsRegisteredBiosID";

            var srcQuery = @"InMageTelemetrySourceV2
                            | where PreciseTimeStamp >= ago(180d)
                            //| where OsVer contains ""windows""
                            | where FabType == ""VMware""
                            | summarize arg_max(PreciseTimeStamp, *) by HostId
                            | extend BiosID=tolower(BiosId)
                            | distinct HostId, BiosID, OsVer";


            foreach (var Cluster in SHRConstants.ASRClusterMap.Keys)
            {
                var Database = SHRConstants.ASRClusterMap[Cluster];
                using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{Database};Fed=true"))
                {
                    using (var reader = queryProvider.ExecuteQuery(qCSBiosId))
                    {
                        while (reader.Read())
                        {
                            var HostId = reader.GetString(0).Trim().ToLower();
                            var BiosId = reader.GetString(1).Trim().ToLower();
                            if (!CSHostIdBiosId.ContainsKey(HostId))
                            {
                                CSHostIdBiosId.Add(HostId, BiosId);
                            }
                            else
                            {
                                if (!CSHostIdBiosId[HostId].Equals(BiosId, StringComparison.OrdinalIgnoreCase))
                                {
                                    Console.WriteLine("CSInfo: Source {0} BiosId: \"{1}\" registered with new BiosId: \"{2}\"",
                                                        HostId, CSHostIdBiosId[HostId], BiosId);
                                }
                                CSHostIdBiosId[HostId] = BiosId;
                            }
                        }
                    }
                }
            }
            uint totalWindowsMatches = 0;
            uint totalLinuxMatches = 0;
            uint totalLinuxSwappedMatches = 0;

            uint totalWindowsMisMatches = 0;
            uint totalLinuxMisMatches = 0;

            foreach (var Cluster in SHRConstants.MABClusterMap.Keys)
            {
                var Database = SHRConstants.MABClusterMap[Cluster];
                using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{Database};Fed=true"))
                {
                    using (var reader = queryProvider.ExecuteQuery(srcQuery))
                    {
                        while (reader.Read())
                        {
                            var HostId = reader.GetString(0).Trim().ToLower();
                            var BiosId = reader.GetString(1).Trim().ToLower();
                            var OsVer = reader.GetString(2).Trim().ToLower();

                            var platform = OsVer.Contains("windows") ? "Windows" : "Linux";

                            if (!SRCHostIdBiosId.ContainsKey(HostId))
                            {
                                SRCHostIdBiosId.Add(HostId, BiosId);
                                SRCHostIdOSType.Add(HostId, OsVer);
                            }
                            else
                            {
                                if (!SRCHostIdBiosId[HostId].Equals(BiosId, StringComparison.OrdinalIgnoreCase))
                                {
                                    Console.WriteLine("SourceInfo: VM {0} BiosId: \"{1}\" updated with BiosId: \"{2}\"",
                                                    HostId, SRCHostIdBiosId[HostId], BiosId);
                                    SRCHostIdBiosId[HostId] = BiosId;
                                    SRCHostIdOSType[HostId] = OsVer;
                                }
                            }
                        }
                    }
                }
            }

            foreach (var hostid in CSHostIdBiosId.Keys)
            {
                if (SRCHostIdBiosId.ContainsKey(hostid))
                {
                    string srcGuid = SRCHostIdBiosId[hostid];
                    string swappedGuid = "";

                    Guid uuid;
                    if (Guid.TryParse(srcGuid, out uuid))
                    {
                        var byteGuid = uuid.ToByteArray();
                        string tempGuidBytes = BitConverter.ToString(byteGuid, 0, byteGuid.Length).Replace("-", "");
                        swappedGuid = new Guid(tempGuidBytes).ToString();

                        if (SRCHostIdOSType[hostid].Contains("windows"))
                        {
                            srcGuid = swappedGuid;
                        }
                        else
                        {
                            srcGuid = uuid.ToString();
                        }
                    }

                    bool isMatched = true;
                    if (!srcGuid.Equals(CSHostIdBiosId[hostid], StringComparison.OrdinalIgnoreCase) &&
                        !swappedGuid.Equals(CSHostIdBiosId[hostid], StringComparison.OrdinalIgnoreCase))
                    {
                        Console.WriteLine("{0}\t{1,-36}\t{2,-36}\t{3}", hostid, CSHostIdBiosId[hostid], srcGuid, SRCHostIdOSType[hostid]);
                        isMatched = false;
                    }

                    if (isMatched)
                    {
                        if (SRCHostIdOSType[hostid].Contains("windows"))
                        {
                            totalWindowsMatches++;
                        }
                        else
                        {
                            if (swappedGuid.Equals(CSHostIdBiosId[hostid], StringComparison.OrdinalIgnoreCase))
                            {
                                totalLinuxSwappedMatches++;
                            }
                            else
                            {
                                totalLinuxMatches++;
                            }
                        }
                    }
                    else
                    {
                        if (SRCHostIdOSType[hostid].Contains("windows"))
                        {
                            totalWindowsMisMatches++;
                        }
                        else
                        {
                            totalLinuxMisMatches++;
                        }

                    }

                }
            }

            Console.WriteLine("Windows Matches: {0} mismatches: {1}", totalWindowsMatches, totalWindowsMisMatches);
            Console.WriteLine("Linux Matches: {0} Swap Matches: {1} mismatches: {2}", totalLinuxMatches, totalLinuxSwappedMatches, totalLinuxMisMatches);
        }
    }
}