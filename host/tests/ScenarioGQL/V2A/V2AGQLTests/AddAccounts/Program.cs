using InMage.APIHelpers;
using System;
using System.Collections.Generic;

namespace AddAccounts
{
    class Program
    {
        private static CXClient client;

        static void Usage()
        {
            Console.WriteLine(".\addAccounts.exe --csip <CS IP> --csport <CS Port> --friendlyname <User friendly name> --user <UserName> --pass <Password>");
        }
        static int Main(string[] args)
        {
            string csIP = null;
            int csPort = 0;
            string friendlyName = null;
            string username = null;
            string password = null;
            bool returnStatus = false;
            int ret = 1;

            if (args.Length > 0)
            {
                int index = 0;
                foreach (string argument in args)
                {
                    switch (argument.ToLower())
                    {
                        case "--csip":
                            csIP = args[index + 1].ToString();
                            break;

                        case "--csport":
                            int.TryParse(args[index + 1], out csPort);
                            break;

                        case "--friendlyname":
                            friendlyName = args[index + 1].ToString();
                            break;

                        case "--user":
                            username = args[index + 1].ToString();
                            break;

                        case "--pass":
                            password = args[index + 1].ToString();
                            break;
                    }

                    index++;
                }
                
                if (index != 10)
                {
                    Usage();
                    return 1;
                }
            }

            try
            {
                ResponseStatus responseStatus = new ResponseStatus();
                List<UserAccount> userAccountList = new List<UserAccount>();
                UserAccount userAccount = new UserAccount();
                userAccount.AccountName = friendlyName;
                userAccount.UserName = username;
                userAccount.Password = password;

                userAccountList.Add(userAccount);

                client = new CXClient(
                    csIP,
                    csPort,
                    CXProtocol.HTTPS
                    );

                returnStatus = client.CreateAccounts(userAccountList, responseStatus);
                if (responseStatus.ReturnCode != 0)
                {
                    if (responseStatus.Message.Contains("Account already exist"))
                    {
                        Console.WriteLine("Successfully added user(" + friendlyName + ")");
                        ret = 0;
                    }
                    else
                    {
                        Console.WriteLine("Error:" + responseStatus.ReturnCode);
                        Console.WriteLine("Error:" + responseStatus.Message);
                    }
                }
                else
                {
                    ret = 0;
                    Console.WriteLine("Successfully added user(" + friendlyName + ")");
                }
            }
            catch (Exception e)
            {
                Console.WriteLine("Got Exception:" + e);
            }

            return ret;
        }
    }
}
