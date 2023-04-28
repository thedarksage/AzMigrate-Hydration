using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Diagnostics;


namespace Com.Inmage
{
    [Serializable]  
    public struct Credentials
    {
        internal string name;
        internal string domain;
        internal string userName;
        internal string password;


        public void Print()
        {
            Trace.WriteLine("Name = " + name, "Domain = " + domain + "UserName" + userName + "Password" + password);
        }

    }
    [Serializable]
    public class CredentialsList
    {
        internal ArrayList Credlist;


        public CredentialsList()
        {
            Credlist = new ArrayList();

        }

        internal bool AddCredentials(string inName, string inDomain, string inUserName, string inPassWord)
        {

            if (inName == null || inName.Length < 1 )
            {
                return false;
            }

            Credentials cred    = new Credentials();

            cred.name           = inName;
            cred.domain         = inDomain;
            cred.userName       = inUserName;
            cred.password       = inPassWord;

            Credlist.Add(cred);

            return true;
        }

        //public bool GetCredentials(string inName, string inDomain, string inUserName, string inPassWord)
        //{
        //    foreach (Credentials cred in Credlist)
        //    {
        //        if (cred.name.CompareTo(inName) == 0 )
        //        {
        //            inDomain    = cred.domain;
        //            inUserName  = cred.userName;
        //            inPassWord  = cred.password;
        //            return true;
        //        }
        //    }

        //    return false;
        //}

        internal bool Print()
        {

            foreach (Credentials cred in Credlist)
            {
                //Trace.WriteLine("Name = " + cred.name + "Domain = " + cred.domain + "Username = " + cred.userName + "Password = " + cred.password);

            }
            return true;
        }





    }
}
