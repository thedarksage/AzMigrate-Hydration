using System;
using System.ServiceProcess;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Main class for ProcessServerMonitor service.
    /// </summary>
    public static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        public static void Main(string[] args)
        {
            if (args.Length > 0)
            {
                try
                {
                    switch (args[0])
                    {
                        case "/i":
                            InstallHelper.InstallService();
                            break;
                        case "/u":
                            InstallHelper.UninstallService();
                            break;
                        default:
                            throw new NotImplementedException(
                                string.Format(
                                "Requested operation is not implemented. Passed args : {0}",
                                string.Join(" ", args)));
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Exception occurred - {0}", ex);
                }
            }
            else
            {
                AppDomain.CurrentDomain.UnhandledException += ProcessServerMonitor.UnhandledExceptionHandler;

                ServiceBase.Run(new ProcessServerMonitor());
            }
        }
    }
}
