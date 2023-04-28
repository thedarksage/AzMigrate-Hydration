using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using RcmAgentCommonLib.ErrorHandling;
using RcmClientLib;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Xml;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup
{
    public class ConfiguratorOutput
    {
        public int ExitCode { get; private set; }

        private readonly RcmAgentError m_agentError;

        private static readonly Lazy<XmlDocument> s_rcmAgentErrorsXml = new Lazy<XmlDocument>(() =>
        {
            XmlDocument toRet = new XmlDocument();
            Assembly currAssembly = Assembly.GetAssembly(typeof(ConfiguratorOutput));
            var resourceName = "Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup.RcmAgentErrors.xml";
            Stream xmlStream = currAssembly.GetManifestResourceStream(resourceName);
            toRet.Load(xmlStream);
            return toRet;
        });

        private ConfiguratorOutput(int exitCode, RcmAgentError rcmAgentError)
        {
            this.ExitCode = exitCode;
            this.m_agentError = rcmAgentError;
        }

        public override string ToString()
        {
            return m_agentError?.ToString();
        }

        public static ConfiguratorOutput Build(
            PSRcmAgentErrorEnum errorCode,
            bool isRetryableError,
            string message = null,
            RcmOperationContext opContext = null,
            Dictionary<string, string> msgParams = null,
            RcmException rcmEx = null)
        {
            try
            {
                if (opContext?.ActivityId != null || opContext?.ClientRequestId != null)
                {
                    msgParams = msgParams ?? new Dictionary<string, string>();
                    msgParams.Add("ActivityId", opContext?.ActivityId);
                    msgParams.Add("ClientRequestId", opContext?.ClientRequestId);
                }

                XmlNamespaceManager nsManager = new XmlNamespaceManager(s_rcmAgentErrorsXml.Value.NameTable);
                nsManager.AddNamespace("srs", "http://schemas.microsoft.com/2012/DisasterRecovery/SRSErrors.xsd");

                XmlNode node = s_rcmAgentErrorsXml.Value.SelectSingleNode($"/srs:SRSErrors/srs:Error[srs:EnumName/text()='{errorCode}']", nsManager);

                if (node != null)
                {
                    return new ConfiguratorOutput(
                        (int)errorCode,
                        new RcmAgentError(
                            errorCode: errorCode.ToString(),
                            // If a specific message is given, override the stock message
                            message: message ?? node?["Message"]?.InnerText,
                            possibleCauses: node?["PossibleCauses"]?.InnerText,
                            recommendedAction: node?["RecommendedAction"]?.InnerText,
                            severity: node?["Severity"]?.InnerText,
                            category: node?["ErrorTags"]?["Category"].InnerText,
                            isRetryableError: isRetryableError,
                            errorMsgParams: msgParams,
                            isRcmReportedError: rcmEx != null,
                            rcmException: rcmEx));
                }
                else
                {
                    Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Couldn't find the mapping for Error code : {0}",
                        errorCode);
                }
            }
            catch (Exception ex)
            {
                Tracers.RcmJobProc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to build RcmAgentError for Error code : {0}{1}{2}",
                    errorCode, Environment.NewLine, ex);
            }

            return new ConfiguratorOutput(
                (int)errorCode,
                new RcmAgentError(
                    errorCode: errorCode.ToString(),
                    message: message,
                    possibleCauses: null,
                    recommendedAction: null,
                    severity: Severity.Error.ToString(),
                    category: null,
                    isRetryableError: isRetryableError,
                    errorMsgParams: msgParams,
                    isRcmReportedError: rcmEx != null,
                    rcmException: rcmEx));
        }

        public static ConfiguratorOutput Build(
            PSRcmAgentErrorEnum errorCode,
            bool isRetryableError,
            RcmOperationContext opContext,
            Exception ex)
        {
            var msgParams = new Dictionary<string, string>()
            {
                ["Exception"] = ex.ToString()
            };

            if (opContext != null)
            {
                msgParams.Add("ActivityId", opContext?.ActivityId);
                msgParams.Add("ClientRequestId", opContext?.ClientRequestId);
            }

            return Build(errorCode, isRetryableError: isRetryableError, msgParams: msgParams);
        }
    }

    [Serializable]
    public class ConfiguratorException : Exception
    {
        public PSRcmAgentErrorEnum ErrorCode { get; }

        public ConfiguratorOutput Output { get; }

        public ConfiguratorException(PSRcmAgentErrorEnum errorCode,
            bool isRetryableError,
            string message = null,
            RcmOperationContext opContext = null,
            Dictionary<string, string> msgParams = null)
        {
            ErrorCode = errorCode;
            Output = ConfiguratorOutput.Build(errorCode, isRetryableError, message, opContext, msgParams, null);
        }

        public override string ToString()
        {
            return $"{Output}{Environment.NewLine}{base.ToString()}";
        }
    }
}
