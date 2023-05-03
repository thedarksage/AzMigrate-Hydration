using NetFwTypeLib;
using System;
using System.Globalization;
using System.IO;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities to interact with Windows Firewall
    /// </summary>
    public static class FirewallUtils
    {
        /// <summary>
        /// Get firewall rule named as <paramref name="ruleName"/>.
        /// Note that, there can be more than one firewall rule with the same name.
        /// </summary>
        /// <param name="ruleName">Name of the firewall rule</param>
        /// <returns>Firewall rule COM object</returns>
        public static INetFwRule GetFirewallRule(string ruleName)
        {
            var fwPolicy = (INetFwPolicy2)Activator.CreateInstance(
                Type.GetTypeFromProgID("HNetCfg.FwPolicy2", throwOnError: true));

            try
            {
                return fwPolicy.Rules.Item(ruleName);
            }
            catch (FileNotFoundException)
            {
                return null;
            }
        }

        /// <summary>
        /// Remove firewall rule(s) named as <paramref name="ruleName"/>
        /// </summary>
        /// <param name="ruleName">Name of the firewall rule</param>
        /// <param name="ignoreIfNotPresent">Don't throw, if the rule couldn't be found</param>
        public static void RemoveFirewallRule(string ruleName, bool ignoreIfNotPresent)
        {
            var fwPolicy = (INetFwPolicy2)Activator.CreateInstance(
                Type.GetTypeFromProgID("HNetCfg.FwPolicy2", throwOnError: true));

            try
            {
                fwPolicy.Rules.Remove(ruleName);
            }
            catch (FileNotFoundException)
            {
                if (!ignoreIfNotPresent)
                    throw;
            }
        }

        /// <summary>
        /// Remove firewall rules in group named as <paramref name="groupName" />
        /// It will remove all the rules that have group name as <paramref name="groupName" />
        /// even if more than one rules have same name.
        /// </summary>
        /// <param name="groupName">Name of firewall rule group</param>
        public static void RemoveFirewallRules(string groupName)
        {
            var fwPolicy = (INetFwPolicy2)Activator.CreateInstance(
                Type.GetTypeFromProgID("HNetCfg.FwPolicy2", throwOnError: true));
            if(fwPolicy.Rules != null)
            {
                foreach (var rule in fwPolicy.Rules)
                {
                    if (rule != null && (rule as INetFwRule).Grouping == groupName)
                        fwPolicy.Rules.Remove((rule as INetFwRule).Name);
                }
            }
        }

        /// <summary>
        /// Add firewall rule for incoming connections to <paramref name="tcpPort"/>
        /// for the application in <paramref name="applicationName"/> path for
        /// all domains, all local IPs, all network interfaces and all remote
        /// ports & IPs.
        /// Note that calling this method repeatedly will add duplicate rules.
        /// </summary>
        /// <param name="ruleName">Name of the firewall rule</param>
        /// <param name="description">Description of the firewall rule (optional)</param>
        /// <param name="groupName">Group name (optional)</param>
        /// <param name="tcpPort">Listening TCP port</param>
        /// <param name="applicationName">Full path of application</param>
        public static void AddFirewallRuleForTcpListener(
            string ruleName, string description, string groupName,
            int tcpPort, string applicationName)
        {
            // NOTE-SanKumar-1904: All the COM types used must be available
            // starting from the least supported OS.
            var fwRuleToAdd = (INetFwRule)Activator.CreateInstance(
                Type.GetTypeFromProgID("HNetCfg.FwRule", throwOnError: true));

            fwRuleToAdd.ApplicationName = applicationName;
            fwRuleToAdd.Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW;
            if (!string.IsNullOrWhiteSpace(description))
                fwRuleToAdd.Description = description;
            fwRuleToAdd.Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN;
            //fwRuleToAdd.EdgeTraversal // Disabled (default)
            fwRuleToAdd.Enabled = true;
            if (!string.IsNullOrWhiteSpace(groupName))
                fwRuleToAdd.Grouping = groupName;
            //fwRuleToAdd.IcmpTypesAndCodes

            //fwRuleToAdd.Interfaces
            // TODO-SanKumar-1909: We actually know the NIC that the VMs will
            // replicate to. But this rule is applied by passing the friendly
            // name of the NIC. Could it change?

            fwRuleToAdd.InterfaceTypes = "All"; // RemoteAccess, Wireless, Lan
            fwRuleToAdd.LocalAddresses = "*";
            // Protocol must be set before settings any ports
            fwRuleToAdd.Protocol = (int)NET_FW_IP_PROTOCOL_.NET_FW_IP_PROTOCOL_TCP;
            fwRuleToAdd.LocalPorts = tcpPort.ToString(CultureInfo.InvariantCulture);
            fwRuleToAdd.Name = ruleName;
            fwRuleToAdd.Profiles = (int)NET_FW_PROFILE_TYPE2_.NET_FW_PROFILE2_ALL;
            fwRuleToAdd.RemoteAddresses = "*";
            fwRuleToAdd.RemotePorts = "*";
            // TODO-SanKumar-1908: Explicitly providing cxprocessserver doesn't work, even when
            // tried manually through UI. To be debuggged.
            fwRuleToAdd.serviceName = "*";

            var fwPolicy = (INetFwPolicy2)Activator.CreateInstance(
                Type.GetTypeFromProgID("HNetCfg.FwPolicy2", throwOnError: true));

            // Add the new firewall settings
            fwPolicy.Rules.Add(fwRuleToAdd);
        }
    }
}
