using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ConfigurationServer
    {
        public ScoutComponentType ComponentType { get; private set; }
        public string HostId { get; private set; }
        public string HostName { get; private set; }
        public IPAddress HostIP { get; private set; }
        public OSType OSType { get; private set; }        
        public string Version { get; private set; }       
        public Dictionary<string, DateTime?> InstalledUpdates { get; private set; }
        // Accounts hold Account Id and its corresponding Account Name
        public Dictionary<string, string> Accounts { get; private set; }

        public ConfigurationServer(FunctionResponse listScoutComponentsResponse)
        {
            if (listScoutComponentsResponse != null && listScoutComponentsResponse.ChildList != null && listScoutComponentsResponse.ChildList.Count > 0)
            {
                ComponentType = Utils.ParseEnum<ScoutComponentType>(listScoutComponentsResponse.GetParameterValue("ComponentType"));
                HostId = listScoutComponentsResponse.GetParameterValue("HostId");
                IPAddress hostIP;
                if (IPAddress.TryParse(listScoutComponentsResponse.GetParameterValue("IPAddress"), out hostIP))
                {
                    HostIP = hostIP;
                }
                HostName = listScoutComponentsResponse.GetParameterValue("HostName");
                OSType = Utils.ParseEnum<OSType>(listScoutComponentsResponse.GetParameterValue("OSType"));
                Version = listScoutComponentsResponse.GetParameterValue("Version");

                ParameterGroup updates = listScoutComponentsResponse.GetParameterGroup("CSUpdateHistory");
                ParameterGroup updateDetails;
                string updateVersion;
                if (updates != null)
                {
                    InstalledUpdates = new Dictionary<string, DateTime?>();
                    foreach (var item in updates.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            updateDetails = item as ParameterGroup;
                            updateVersion = updateDetails.GetParameterValue("Version");
                            if (!String.IsNullOrEmpty(updateVersion))
                            {
                                InstalledUpdates[updateVersion] = Utils.UnixTimeStampToDateTime(updateDetails.GetParameterValue("InstallationTimeStamp"));
                            }
                        }
                    }
                }


                ParameterGroup accounts = listScoutComponentsResponse.GetParameterGroup("Accounts");
                ParameterGroup accountDetails;
                string accountId;
                if (accounts != null && accounts.ChildList.Count > 0)
                {
                    Accounts = new Dictionary<string, string>();
                    foreach (var item in accounts.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            accountDetails = item as ParameterGroup;
                            accountId = accountDetails.GetParameterValue("AccountId");
                            if (!String.IsNullOrEmpty(accountId))
                            {
                                Accounts[accountId] = accountDetails.GetParameterValue("AccountName");
                            }
                        }
                    }
                }
            }
        }
    }
}
