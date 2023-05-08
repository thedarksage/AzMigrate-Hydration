//---------------------------------------------------------------
//  <copyright file="Program.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Tool used for periodic discovery triggered by Process Server in InMage.
//  </summary>
//
//  History:     30-Oct-2015   Prmyaka   Created
// -----------------------------------------------------------------------

using System;
using System.Diagnostics;
using System.Net;
using VMware.Client;
using VMware.Vim;

namespace InMageDiscovery
{
    /// <summary>
    /// Console application for InMage Discovery.
    /// </summary>
    public class Program
    {
        /// <summary>
        /// Exit codes enum.
        /// </summary>
        private enum ExitCode
        {
            /// <summary>
            /// Successfull discovery.
            /// </summary>
            Success = 0,

            /// <summary>
            /// Failure while discovery.
            /// </summary>
            Failure = 1,

            /// <summary>
            /// Invalid IP or not reachable.
            /// </summary>
            InvalidIp = 2,

            /// <summary>
            /// Invalid username or password.
            /// </summary>
            InvalidCredentials = 3,

            /// <summary>
            /// Invalid CLI arguments.
            /// </summary>
            InvalidArgs = 5,

            /// <summary>
            /// No VMs or ESXs found
            /// </summary>
            NoVmsFound = 6
        }

        /// <summary>
        /// Entry point.
        /// </summary>
        /// <param name="args">Input arguments.</param>
        /// <returns>Exit code.</returns>
        private static int Main(string[] args)
        {
            int exitCode = (int)ExitCode.Success;
            string tempLogFile = "vContinuum_Scripts_Temp_" +
                DateTime.Now.ToString("yyyyMMddHHmmssfff") + ".log";
            string tempErrorLogFile = "vContinuum_ESX_Temp_" +
                DateTime.Now.ToString("yyyyMMddHHmmssfff") + ".log";
            DiscoveryLogs discoveryLogs = new DiscoveryLogs(tempLogFile, tempErrorLogFile);

            VimTracing.Initialize(
                Diagnostics.Trace,
                Diagnostics.Assert,
                Diagnostics.DebugAssert);

            string messageForHelp =
                "Copyright (c) Microsoft Corporation. All rights reserved." +
                Environment.NewLine +
                "Following options are available for command line operations" +
                Environment.NewLine + Environment.NewLine +
                "--server <vCenter/ESX IP/Hostname>" + Environment.NewLine +
                "--username <Username>" + Environment.NewLine +
                "--password <Password>" + Environment.NewLine +
                "[--ostype <windows/linux/all>]" + Environment.NewLine +
                "[--retrieve <hosts/networks/datastores ...>(seperator !@!@!)]" +
                Environment.NewLine;

            if (args.Length == 0)
            {
                exitCode = (int)ExitCode.InvalidArgs;
                discoveryLogs.LogMessage(
                    TraceEventType.Error,
                    "Please pass the command-line arguments");
                Console.WriteLine("Please pass the command-line arguments");
                Console.WriteLine(messageForHelp);
                discoveryLogs.CloseLogs(tempLogFile, tempErrorLogFile);
                return exitCode;
            }

            string server = null;
            string userName = null;
            string password = null;
            string osType = null;
            string retrieve = null;

            int index = 0;
            foreach (string argument in args)
            {
                switch (argument.ToLower())
                {
                    case "--server":
                        server = args[index + 1].ToString();
                        break;
                    case "--username":
                        userName = args[index + 1].ToString();
                        break;
                    case "--password":
                        password = args[index + 1].ToString();
                        break;
                    case "--ostype":
                        osType = args[index + 1].ToString();
                        break;
                    case "--retrieve":
                        retrieve = args[index + 1].ToString();
                        break;
                    case "--help":
                        Console.WriteLine(messageForHelp);
                        break;
                    case "--h":
                        Console.WriteLine(messageForHelp);
                        break;
                    case "/?":
                        Console.WriteLine(messageForHelp);
                        break;
                }

                index++;
            }

            if (string.IsNullOrEmpty(server) ||
                string.IsNullOrEmpty(userName) ||
                string.IsNullOrEmpty(password))
            {
                exitCode = (int)ExitCode.InvalidArgs;
                discoveryLogs.LogMessage(
                    TraceEventType.Error,
                    "Enter valid values for server, username and password");
                Console.WriteLine(messageForHelp);
                discoveryLogs.CloseLogs(tempLogFile, tempErrorLogFile);
                return exitCode;
            }

            try
            {
                VMwareClient vmwareClient = new VMwareClient(
                    server,
                    userName,
                    password);

                OsType osTypeEnum = vmwareClient.GetOsTypeEnum(osType);

                InfrastructureView esxInfo;
                if (!string.IsNullOrEmpty(retrieve))
                {
                    string[] delimiter = new string[] { "!@!@!" };
                    string[] options =
                        retrieve.Split(delimiter, StringSplitOptions.RemoveEmptyEntries);
                    esxInfo = vmwareClient.GetInfrastructureViewLite(options, osTypeEnum);
                }
                else
                {
                    esxInfo = vmwareClient.GetInfrastructureView(osTypeEnum);
                }

                discoveryLogs.LogMessage(
                    TraceEventType.Information,
                    "Successfully discovered the vCenter/vSphere.");

                if (esxInfo.host.Length == 0)
                {
                    exitCode = (int)ExitCode.NoVmsFound;
                    discoveryLogs.LogMessage(
                        TraceEventType.Error,
                        "No Virtual Machines/Host Systems found.");
                }

                discoveryLogs.WriteIntoXml(esxInfo, server);
            }
            catch (VMwareClientException vce)
            {
                exitCode = (int)ExitCode.Failure;
                discoveryLogs.LogMessage(TraceEventType.Error, vce.Message);
            }
            catch (VimException ve)
            {
                exitCode = (int)ExitCode.Failure;
                if ((ve.MethodFault is InvalidLogin) || (ve.MethodFault is NoPermission))
                {
                    exitCode = (int)ExitCode.InvalidCredentials;
                }

                discoveryLogs.LogMessage(TraceEventType.Error, ve.Message);
            }
            catch (WebException we)
            {
                exitCode = (int)ExitCode.InvalidIp;
                discoveryLogs.LogMessage(TraceEventType.Error, we.Message);
            }
            catch (Exception e)
            {
                exitCode = (int)ExitCode.Failure;
                discoveryLogs.LogMessage(TraceEventType.Error, e.Message);
            }

            discoveryLogs.CloseLogs(tempLogFile, tempErrorLogFile);
            return exitCode;
        }
    }
}
