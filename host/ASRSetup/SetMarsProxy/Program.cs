using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Management;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using System.Collections;
using System.Collections.ObjectModel;
using System.Security;
using System.Security.Cryptography;
using Microsoft.Win32;

namespace SetMarsProxy
{
    class Program
    {
        static int Main(string[] args)
        {
            int returnCode = 1;
            try
            {
                Console.WriteLine("Args Length: {0}", args.Length);
                // Show usage when no argugments are passed.
                if (args.Length < 1)
                {
                    Console.WriteLine("No arguments are passed. Please re-try by passing arguments.");
                    ShowUsage();
                    return returnCode;
                }

                // Show usage when /? is passed.
                if (args[0] == "/?" || args[0] == "-?")
                {
                    ShowUsage();
                    return 0;
                }

                // Bail out if number of arguments are less than 1 or greater than 5.
                if (args.Length < 1 || args.Length > 5)
                {
                    Console.WriteLine("Invalid number of arguments passed.");
                    ShowUsage();
                    return returnCode;
                }

                // Bail our if first argument does not start with http://
                if (args.Length > 1 && !args[0].StartsWith("http://"))
                {
                    Console.WriteLine("Invalid proxy address passed. It should be in http://<address> format.");
                    ShowUsage();
                    return returnCode;
                }

                // Construct a dictionary with all command line arguments.
                Dictionary<string, object> paramCollection = new Dictionary<string, object>();

                if (args.Length == 1)
                {
                    if (!string.Equals("noproxy", args[0], StringComparison.OrdinalIgnoreCase))
                    {
                        Console.WriteLine("Invalid argument passed. NoProxy option should be passed");
                        ShowUsage();
                        return returnCode;
                    }
                    Console.WriteLine("proxy setting : {0}", args[0]);
                    paramCollection.Add("NoProxy", null);
                }
                else
                {
                    // Read address and port command line arguments.
                    string address = args[0];
                    string port = args[1];
                    Console.WriteLine("address: {0}", address);
                    Console.WriteLine("port: {0}", port);

                    // Read username and password command line arguments.
                    string username = "";
                    string password = "";
                    var spassword = new SecureString();
                    if (args.Length >= 4)
                    {
                        username = args[2];
                        Console.WriteLine("username: {0}", username);
                        password = args[3];
                        spassword = StringToSecureString(password);
                        Console.WriteLine("password: {0}", spassword);
                    }

                    // Read bypass proxy urls from command line arguments.
                    string bypassURLs = "";
                    if (args.Length == 3 || args.Length == 5)
                    {
                        bypassURLs = args[args.Length - 1];
                    }

                    paramCollection.Add("ProxyServer", address);
                    paramCollection.Add("ProxyPort", int.Parse(port));

                    if (!string.IsNullOrEmpty(username))
                    {
                        paramCollection.Add("ProxyUsername", username);
                        paramCollection.Add("ProxyPassword", spassword);
                    }

                    if (!string.IsNullOrEmpty(bypassURLs))
                    {
                        paramCollection.Add("BypassUrls", bypassURLs);
                    }
                }

                // Get MARS agent PS module path.
                string marsAgentPSModulePath = GetMARSAgentPSModulePath();
                Console.WriteLine("marsAgentPSModulePath: {0}", marsAgentPSModulePath);
                string MarsAgentSetSettingsCmdlet = "Set-OBMachineSetting";

                // Execute Set-OBMachineSetting cmdlet.
                ExecuteCmdlet(
                     new string[] { GetMARSAgentPSModulePath() },
                     MarsAgentSetSettingsCmdlet,
                     paramCollection);
                Console.WriteLine("MARS proxy has been set/reset successfully.");
                return 0;
            }
            catch (Exception exp)
            {
                Console.WriteLine("Failed to configure MARS proxy. Exception occurred - {0}", exp.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Executes cmdlet on powershell after loading the requiredModules.
        /// </summary>
        /// <param name="requiredModules">Modules to be loaded</param>
        /// <param name="cmdletName">Cmdlet to be executed</param>
        /// <param name="cmdletParams">Params to be passed to cmdlet</param>
        /// <returns>Cmdlet result</returns>
        public static Collection<PSObject> ExecuteCmdlet(string[] requiredModules, string cmdletName, Dictionary<string, object> cmdletParams)
        {
            Console.WriteLine("Starting execution of cmdlet {0}", cmdletName);
            InitialSessionState initial = InitialSessionState.CreateDefault2();
            initial.ImportPSModule(requiredModules);
            Runspace runspace = RunspaceFactory.CreateRunspace(initial);
            runspace.Open();

            System.Management.Automation.PowerShell ps = System.Management.Automation.PowerShell.Create();
            ps.Runspace = runspace;
            ps.Commands.AddCommand(cmdletName);

            foreach (KeyValuePair<string, object> cmdletParam in cmdletParams)
            {
                Console.WriteLine("Params : {0}, {1}", cmdletParam.Key, cmdletParam.Value);
                if (cmdletParam.Value != null)
                {
                    ps.Commands.AddParameter(cmdletParam.Key, cmdletParam.Value);
                }
                else
                {
                    // Add switch param.
                    ps.Commands.AddParameter(cmdletParam.Key);
                }
            }

            Collection<PSObject> obj = null;

            try
            {
                obj = ps.Invoke();
            }
            catch (Exception ex)
            {
                foreach (object errors in (ArrayList)runspace.SessionStateProxy.PSVariable.GetValue("Error"))
                {
                    Console.Write("Errors while while executing cmdlet: {0}", errors.ToString());
                }

                Console.Write("Hit exception while executing cmdlet: ", ex.ToString());
                throw;
            }

            runspace.Close();
            ps.Runspace.Close();

            return obj;
        }

        /// <summary>
        /// Gets MARS agent PS module loading name.
        /// </summary>
        /// <returns>MARS agent module.</returns>
        public static string GetMARSAgentPSModulePath()
        {
            try
            {
                String installationPath;
                using (var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                using (var key = hklm.OpenSubKey(@"SOFTWARE\Microsoft\Windows Azure Backup\Setup"))
                {
                    installationPath = (String)key.GetValue("InstallPath");
                }

                Console.WriteLine("MARS installation path : {0}", installationPath);

                string MarsPSModuleRelativePath = @"bin\Modules\MSOnlineBackup\MSOnlineBackup.psd1";
                string MarsAgentPSModule = "MSOnlineBackup";

                if (string.IsNullOrEmpty(installationPath))
                {
                    return MarsAgentPSModule;
                }
                else
                {
                    return System.IO.Path.Combine(
                        installationPath,
                        MarsPSModuleRelativePath);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Hit exception while getting GetMARSAgentPSModulePath.", ex);
                throw;
            }
        }

        /// <summary>
        /// To Convert string to a secure string
        /// </summary>
        /// <param name="value">Value of the string to be converted</param>
        /// <returns>SecureString object.</returns>
        public static SecureString StringToSecureString(string value)
        {
            SecureString secureString = new SecureString();
            char[] valueChars = value.ToCharArray();

            foreach (char c in valueChars)
            {
                secureString.AppendChar(c);
            }

            return secureString;
        }

        /// <summary>
        /// Dispay usage
        /// </summary>
        public static void ShowUsage()
        {
            Console.WriteLine("Usage: SetMarsProxy.exe <proxy address in http://address format> <proxy port> [proxy username] [proxy password] [<bypass-url>[;bypass-url]...]");
            Console.WriteLine("Usage: SetMarsProxy.exe <NoProxy>");
            Console.WriteLine("Note: [proxy username], [proxy password], [bypass-proxy urls] are optional arguments.");
            Console.WriteLine("Example: SetMarsProxy.exe \"http://myproxy.com\" \"8080\"");
            Console.WriteLine("Example: SetMarsProxy.exe \"http://myproxy.com\" \"8080\" \"byproxy-urls\"");
            Console.WriteLine("Example: SetMarsProxy.exe \"http://myproxy.com\" \"8080\" \"administrator\" \"Password1!\"");
            Console.WriteLine("Example: SetMarsProxy.exe \"http://myproxy.com\" \"8080\" \"administrator\" \"Password1!\" \"www.bing.com;www.microsoft.com\"");
        }
    }
}