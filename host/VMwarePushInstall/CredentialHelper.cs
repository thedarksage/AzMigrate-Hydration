//---------------------------------------------------------------
//  <copyright file="CredentialHelper.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Helper module to get the credentials.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Security;

using VMware.Security.CredentialStore;
using InMage.APIHelpers;
using Newtonsoft.Json;

namespace VMwareClient
{
    public class LogonAccount
    {
        public string AccountId;

        public string Domain;

        public string Username;

        public string Password;
    };

    public class CredentialHelper
    {
        /// <summary>
        /// Version of the credential store.
        /// </summary>
        public static string CredentialStoreVersion = "1.0";

        /// <summary>
        /// Gets the account information from CS.
        /// </summary>
        /// <param name="csIP">IP of the CS machine.</param>
        /// <param name="csPort">Port to access on CS.</param>
        /// <param name="useHttps">Decide the way to communicate with CS.</param>
        /// <returns>Account info corresponding to the accountId.</returns>
        public static List<LogonAccount> getCSAccounts(
            string csIP,
            int csPort,
            bool useHttps)
        {
            CXClient cxclient = new CXClient(
                csIP,
                csPort,
                useHttps ? CXProtocol.HTTPS : CXProtocol.HTTP);
            ResponseStatus response = new ResponseStatus();
            List<UserAccount> csAccountsList = cxclient.ListAccounts(response);
            List<LogonAccount> logonAccounts = new List<LogonAccount>();

            csAccountsList.ForEach(x => logonAccounts.Add(x.ToLogonAccount()));

            return logonAccounts;
        }

        /// <summary>
        /// Gets the credentials from the credential store.
        /// </summary>
        /// <param name="credStoreCredsFilePath">Path to the cred store credentials file.</param>
        /// <returns></returns>
        public static List<LogonAccount> getAccountsFromCredStore(string credStoreCredsFilePath)
        {
            if (string.IsNullOrEmpty(credStoreCredsFilePath))
            {
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.CredentialStoreCredsFilePathEmpty.ToString(),
                    VMwarePushInstallErrorCode.CredentialStoreCredsFilePathEmpty,
                    "Credential store file path has not been provided or is empty");
            }

            if (!System.IO.File.Exists(credStoreCredsFilePath))
            {
                throw new VMwarePushInstallException(
                    VMwarePushInstallErrorCode.CredentialStoreCredsFileNotFound.ToString(),
                    VMwarePushInstallErrorCode.CredentialStoreCredsFileNotFound,
                    $"Credentials store file {credStoreCredsFilePath} is not found.");
            }

            System.IO.FileInfo credsFileInfo = new System.IO.FileInfo(credStoreCredsFilePath);
            ICredentialStore credStore = CredentialStoreFactory.CreateCredentialStore(credsFileInfo);
            var creds = credStore.ListCredentials(ListCredentialsType.None);

            List<LogonAccount> logonAccounts = new List<LogonAccount>();
            foreach (var cred in creds)
            {
                logonAccounts.Add(cred.ToLogonAccount());
            }

            return logonAccounts;
        }
    }
}
