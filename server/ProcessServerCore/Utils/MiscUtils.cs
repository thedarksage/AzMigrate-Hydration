using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Newtonsoft.Json;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Miscellaneous utilities
    /// </summary>
    public static class MiscUtils
    {
        private static readonly PhpFormatter s_phpFormatter = new PhpFormatter(serializeObjectAsArray: true);

        /// <summary>
        /// Attempts loading all the DLLs refered by the PS projects. Serves
        /// well during the dev testing to verify the packaging, assembly
        /// manifest, etc.
        /// </summary>
        public static void AllDllsLoadTest()
        {
            // Newtonsoft.Json.dll
            Console.WriteLine(new Newtonsoft.Json.JsonSerializer());

            // System.ValueTuple.dll
            Console.WriteLine((1, 2));

            // ClientLibHelpers.dll
            Console.WriteLine(new ClientLibHelpers.ClientLibException("Test Exception"));

            // CredentialStore.dll
            Console.WriteLine(
                VMware.Security.CredentialStore.
                CredentialStoreFactory.CreateCredentialStore());

            // Microsoft.Identity.Client.dll
            Console.WriteLine(new Identity.Client.MsalServiceException(
                nameof(RcmErrorCode.OperationNotAllowed), "Test Exception"));

            // Microsoft.IdentityModel.Abstractions.dll
            Console.WriteLine("loglevel : {0}", IdentityModel.Abstractions.EventLogLevel.Informational);

            // RcmAgentCommonLib.dll
            Console.WriteLine(new RcmAgentCommonLib.ErrorHandling.RcmAgentError());

            // RcmContract.dll
            Console.WriteLine(
                new RcmContract.RegisterProcessServerInput());

            // RcmClientLib.dll
            Console.WriteLine(
                new RcmClientLib.RcmException(new RcmContract.RcmError(),
                System.Net.HttpStatusCode.BadRequest));

            // ServiceBus.dll
            Console.WriteLine(new ServiceBus.Messaging.EventData());

            // Microsoft.ApplicationInsights.dll
            Console.WriteLine(new ApplicationInsights.DataContracts.TraceTelemetry());

            // Microsoft.AI.ServerTelemetryChannel.dll
            Console.WriteLine(
                new ApplicationInsights.WindowsServer.TelemetryChannel.ServerTelemetryChannel());
        }

        public enum EventChannelType
        {
            PSCritical,
            PSInformation,
            HostCritical,
            HostInformation
        }

        public static Dictionary<string, string> PlaceholderToDictionary(object placeholders)
        {
            var json = JsonConvert.SerializeObject(placeholders);
            return JsonConvert.DeserializeObject<Dictionary<string, string>>(json);
        }

        public static string SerializeObjectToPHPFormat(object input)
        {
            string serializedData = null;

            TaskUtils.RunAndIgnoreException(() =>
            {
                using (MemoryStream ms = new MemoryStream())
                {
                    s_phpFormatter.Serialize(ms, input);
                    ms.Seek(0, SeekOrigin.Begin);
                    using (StreamReader sr = new StreamReader(ms))
                    {
                        serializedData = sr.ReadToEnd();
                    }
                }
            }, Tracers.Misc);

            return serializedData;
        }

        public static void ForEach<T>(this IEnumerable<T> enumeration, Action<T> action)
        {
            foreach (T item in enumeration)
            {
                action(item);
            }
        }
    }
}
