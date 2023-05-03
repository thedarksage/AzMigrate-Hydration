//---------------------------------------------------------------
//  <copyright file="Program.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Log upload service main entry point.
//  </summary>
//
//  History:     26-Sep-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using ASRResources;
using ASRSetupFramework;
using DRSetupFramework = Microsoft.DisasterRecovery.SetupFramework;

namespace Microsoft.ASRSetup.LogUploadService
{
    /// <summary>
    /// Main class for LogUpload service.
    /// </summary>
    public static class Program
    {
        /// <summary>
        /// Main entry point.
        /// </summary>
        public static void Main(string[] args)
        {
            try
            {
                Operationprocessor operationprocessor = new Operationprocessor();
                operationprocessor.InitializeTrace();

                if (args.Length > 0)
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
                            Trc.Log(LogLevel.Info,
                                StringResources.LogUploadServiceCmdUsage);
                            break;
                    }
                }
                else
                {
                    ServiceBase[] ServicesToRun;
                    ServicesToRun = new ServiceBase[]
                    {
                    new LogsUploadService()
                    };
                    ServiceBase.Run(ServicesToRun);
                }
            }
            catch(Exception ex)
            {
                Console.WriteLine("Exception occurred - {0}", ex);
            }
        }
    }
}
