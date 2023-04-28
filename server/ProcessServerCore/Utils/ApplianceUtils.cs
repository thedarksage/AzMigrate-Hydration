using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using System;
using System.Collections.Generic;
using System.Security;
using VMware.Security.CredentialStore;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    internal static class ApplianceUtils
    {
        public static SecureString GetSecretFromCredStore(string id)
        {
            if (PSInstallationInfo.Default.CSMode != CSMode.Rcm)
                throw new NotSupportedException($"Credential store is not present in {PSInstallationInfo.Default.CSMode} mode");

            var credStore = CredentialStoreFactory.CreateCredentialStore();
            var credential = credStore.GetCredential(id);

            if (credential == null)
                throw new KeyNotFoundException($"{id} couldn't be found in the Credential store");

            return credential.Password;
        }
    }
}
