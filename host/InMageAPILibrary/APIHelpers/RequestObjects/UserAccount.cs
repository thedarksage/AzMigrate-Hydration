using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
//using System.DirectoryServices.AccountManagement;

namespace InMage.APIHelpers
{
    public class UserAccount
    {
        public string AccountId { get; set; }
        public string AccountName { get; set; }
        public string Domain { get; set; }
        public string UserName { get; set; }
        public string Password { get; set; }
        public string AccountType { get; set; }

        public bool ValidateDomainCredentials()
        {
            if (!String.IsNullOrEmpty(Domain))
            {
                //using (PrincipalContext principalContext = new PrincipalContext(ContextType.Domain, Domain, UserName, Password))
                //{
                //    string userName = Domain + "\\" + UserName;                                                            
                //    return principalContext.ValidateCredentials(username, Password);
                //}
            }
            return false;
        }

        public void SetDomainAndUserName(string domainUserName)
        {
            if (!String.IsNullOrEmpty(domainUserName))
            {
                string[] tokens = domainUserName.Split(new char[] { '\\' }, StringSplitOptions.RemoveEmptyEntries);

                if (tokens != null && tokens.Length == 2)
                {
                    Domain = tokens[0];
                    UserName = tokens[1];
                }
                else
                {
                    UserName = domainUserName;
                    Domain = null;
                }
            }
        }
    }    
}
