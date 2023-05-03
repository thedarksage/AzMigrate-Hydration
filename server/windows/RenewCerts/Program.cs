using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Diagnostics;
using System.Threading;

namespace RenewCerts
{
    class Program
    {    
        static void Main(string[] args)
        {
            Console.WriteLine("Certificates renewal is in progress. Please wait...");

            // Setting default renew cert status
            RenewCertsStatus renewCertsStatus = PSTaskFactory.GetDefaultRenewCertsStatus();
            var csType = PSInstallationInfo.Default.GetCSType();

            bool status = true;
            string errMessage = string.Empty;
            try
            {
                // Regenerate PS certs
                if (csType == CXType.CSAndPS || csType == CXType.PSOnly)
                {
                    Console.WriteLine("PS Certificates renewal is in progress. Please wait...");

                    renewCertsStatus = PSTaskFactory.RegeneratePSCertsAsync(
                        CancellationToken.None).GetAwaiter().GetResult();

                    if (renewCertsStatus.Status)
                    {
                        Console.WriteLine("PS Certificates renewal is successfully done");
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "RenewCerts : Successfully renewed" +
                            " Process Server Certificates");
                    }
                    else
                    {
                        status = false;
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "RenewCerts : Failed to Renew PS Certificates");
                    }
                }
            }
            catch(Exception ex)
            {
                status = false;
                errMessage = string.Format("Exception in certificate renewal process with error {0}.", ex.Message);
                Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "RenewCerts : Failed to renew PS Certificates with error : {0}{1}",
                            Environment.NewLine,
                            ex);
            }

            if (!status)
            {
                Console.WriteLine("Error description: {0}", renewCertsStatus.ErrorDescription);
                Console.WriteLine("Error code: {0}", renewCertsStatus.ErrorCode);
                Console.WriteLine("Response: {0}", renewCertsStatus.Status);
                Console.WriteLine(errMessage + "Please retry multiple times running the same command or contact Microsoft customer support.");
            }
            else if (csType == CXType.CSAndPS)
            {
                LaunchCommandLineApplication();
            }
        }

            static void LaunchCommandLineApplication()
            {
                //Commnad line arguments
                const string parameter1 = "cspsgencerts.pl";
                
                // Creating ProcessStartInfo object.
                ProcessStartInfo startInfo = new ProcessStartInfo();

                // Gets or sets a value indicating whether to start the processs in a new window.
                startInfo.CreateNoWindow = false;

                // Gets or sets a value indicating whether to use operating system shell to start the process.
                startInfo.UseShellExecute = false;

                //Setting the application document to start.
                startInfo.FileName = "C:\\strawberry\\perl\\bin\\perl.exe";
                startInfo.WindowStyle = ProcessWindowStyle.Hidden;
                startInfo.Arguments = parameter1;

                try
                {
                    // Start the process with the given information.
                    // Call WaitForExit and then the using statement will close.
                    using (Process exeProcess = Process.Start(startInfo))
                    {
                        Console.WriteLine("CS Certificates renewal is in progress. Please wait...");
                        exeProcess.WaitForExit();
                    }
                }
                catch
                {
                    Console.WriteLine("Exception occured. Please re try multiple times, Otherwise, Contact Microsfot support.");
                }
            }
        }
}
