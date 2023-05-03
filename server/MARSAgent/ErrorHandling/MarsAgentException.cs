using System;
using System.Collections.Generic;
using System.Globalization;
using System.Resources;
using System.Text;
using System.Text.RegularExpressions;

// Defining that the fallback language is inside a satellite assembly.
[assembly: NeutralResourcesLanguage("en", UltimateResourceFallbackLocation.Satellite)]

namespace MarsAgent.ErrorHandling
{
    /// <summary>
    /// This class represents exceptions thrown by the service.
    /// </summary>
    public partial class MarsAgentException : Exception
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentException"/> class.
        /// </summary>
        /// <param name="errorCode">The error code to be associated with the exception.</param>
        /// <param name="errorMsgParams">Parameters required to construct the error message.
        /// </param>
        public MarsAgentException(
            MarsAgentErrorCode errorCode,
            Dictionary<string, string> errorMsgParams)
        {
            this.ErrorCode = errorCode;
            this.MessageParameters = errorMsgParams ?? new Dictionary<string, string>();
            this.Data.Add("ErrorCode", this.ErrorCode);
            this.ErrorTags = new Dictionary<string, string>(ErrorCodeToTagsDict[this.ErrorCode]);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentException"/> class.
        /// This constructor is used to create an instance of
        /// <see cref="MarsAgentException"/> with an inner exception.
        /// </summary>
        /// <param name="exception">Instance of <see cref="MarsAgentException"/> from which
        /// this exception is to be initialized.</param>
        /// <param name="innerException">Inner exception to be associated with this instance.
        /// </param>
        public MarsAgentException(
            MarsAgentException exception,
            Exception innerException)
            : base(exception.Message, innerException)
        {
            this.ErrorCode = exception.ErrorCode;
            this.MessageParameters =
                exception.MessageParameters ?? new Dictionary<string, string>();
            this.Data.Add("ErrorCode", this.ErrorCode);
            this.ErrorTags = new Dictionary<string, string>(ErrorCodeToTagsDict[this.ErrorCode]);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentException" /> class.
        /// Creates an instance with error code as UnhandledException or
        /// with the one specified.
        /// </summary>
        /// <param name="innerException">
        /// The UnhandledException from which <see cref="MarsAgentException" /> is
        /// created.</param>
        /// <param name="errorCode">The error code to use, if specified.</param>
        public MarsAgentException(
            Exception innerException,
            MarsAgentErrorCode errorCode = MarsAgentErrorCode.UnhandledException)
            : base(string.Empty, innerException)
        {
            this.ErrorCode = errorCode;
            this.MessageParameters = new Dictionary<string, string>();
            this.Data.Add("ErrorCode", this.ErrorCode);
        }

        /// <summary>
        /// Enumeration used to determine the relevant resource string ID for a given error from
        /// the resource file.
        /// </summary>
        private enum ResourceType
        {
            /// <summary>
            /// The message associated with the error.
            /// </summary>
            Message,

            /// <summary>
            /// The possible causes for the error.
            /// </summary>
            PossibleCauses,

            /// <summary>
            /// The recommended action for resolving the error.
            /// </summary>
            RecommendedAction
        }

        /// <summary>
        /// Error tag types.
        /// </summary>
        private enum ErrorTagName
        {
            /// <summary>
            /// Error category.
            /// </summary>
            Category
        }

        /// <summary>
        /// Gets the error code associated with the exception.
        /// </summary>
        public MarsAgentErrorCode ErrorCode { get; private set; }

        /// <summary>
        /// Gets the message parameters needed to form the error message.
        /// </summary>
        public Dictionary<string, string> MessageParameters { get; private set; }

        /// <summary>
        /// Gets the error tags including category associated with the exception.
        /// </summary>
        public Dictionary<string, string> ErrorTags { get; private set; }

        /// <summary>
        /// Gets the error message. Use GetErrorMessage if you want to specify culture.
        /// </summary>
        public override string Message
        {
            get
            {
                return this.GetErrorMessage(CultureInfo.InstalledUICulture.Name);
            }
        }

        /// <summary>
        /// Get error category.
        /// </summary>
        /// <returns>Error category.</returns>
        public string GetErrorCategory()
        {
            MarsAgentEnum.ErrorTagCategory errorCategory;
            string category;
            if (!ErrorCodeToTagsDict[this.ErrorCode].TryGetValue(
                ErrorTagName.Category.ToString(),
                out category))
            {
                errorCategory = MarsAgentEnum.ErrorTagCategory.SystemError;
            }
            else
            {
                if (!Enum.TryParse(category, out errorCategory))
                {
                    errorCategory = MarsAgentEnum.ErrorTagCategory.SystemError;
                }
            }

            return errorCategory.ToString();
        }

        /// <summary>
        /// Get error severity.
        /// </summary>
        /// <returns>Error severity.</returns>
        public string GetErrorSeverity()
        {
            MarsAgentEnum.ErrorSeverity errorSeverity;

            string errorLevel = MarsAgentException.ErrorCodeToSeverityDict[this.ErrorCode];
            const string Info = "Info";

            // As per schema http://schemas.microsoft.com/2012/DisasterRecovery/SRSErrors.xsd
            // Information is represented as string 'Info' in error xml.
            if (string.Compare(errorLevel, Info) == 0)
            {
                errorSeverity = MarsAgentEnum.ErrorSeverity.Information;
            }
            else
            {
                if (!Enum.TryParse(errorLevel, out errorSeverity))
                {
                    errorSeverity = MarsAgentEnum.ErrorSeverity.Error;
                }
            }

            return errorSeverity.ToString();
        }

        /// <summary>
        /// Get the error message defined for the error in the specified culture.
        /// </summary>
        /// <param name="culture">The specific culture in which the message is needed.</param>
        /// <returns>The error message defined for the error.</returns>
        public string GetErrorMessage(string culture)
        {
            string messageResourceId = GetResourceId(this.ErrorCode, ResourceType.Message);
            return MessageFormatter.GetFormattedMessageString(
                messageResourceId,
                this.MessageParameters,
                culture);
        }

        /// <summary>
        /// Get the possible causes defined for the error in the specified culture.
        /// </summary>
        /// <param name="culture">The specific culture in which the message is needed.</param>
        /// <returns>The possible causes defined for the error.</returns>
        public string GetPossibleCauses(string culture)
        {
            string messageResourceId = GetResourceId(this.ErrorCode, ResourceType.PossibleCauses);
            return MessageFormatter.GetFormattedMessageString(
                messageResourceId,
                this.MessageParameters,
                culture);
        }

        /// <summary>
        /// Get the recommended action defined for the error in the specified culture.
        /// </summary>
        /// <param name="culture">The specific culture in which the message is needed.</param>
        /// <returns>The recommended action defined for the error.</returns>
        public string GetRecommendedAction(string culture)
        {
            string messageResourceId =
                GetResourceId(this.ErrorCode, ResourceType.RecommendedAction);
            return MessageFormatter.GetFormattedMessageString(
                messageResourceId,
                this.MessageParameters,
                culture);
        }

        /// <summary>
        /// Get the resource ID used in the resource file.
        /// </summary>
        /// <param name="errorCode">The error code whose resource ID is needed.</param>
        /// <param name="errorResourceType">The resource type whose resource ID is needed.</param>
        /// <returns>The resource ID used in the resource file.</returns>
        private static string GetResourceId(
            MarsAgentErrorCode errorCode,
            ResourceType errorResourceType)
        {
            switch (errorResourceType)
            {
                case ResourceType.Message:
                    return string.Format("Error_{0}_Message", (int)errorCode);
                case ResourceType.PossibleCauses:
                    return string.Format("Error_{0}_Causes", (int)errorCode);
                case ResourceType.RecommendedAction:
                    return string.Format("Error_{0}_Action", (int)errorCode);
                default:
                    return null;
            }
        }

        /// <summary>
        /// Message formatter class for creating messages in the requested culture.
        /// </summary>
        private class MessageFormatter
        {
            /// <summary>
            /// If a message parameter is not passed, it will be replaced with this string.
            /// </summary>
            private const string NoParameterMarker = "";

            /// <summary>
            /// For having a % character in the message, its better to use %% in the message.
            /// The %% in the message is replaced with this for safe parsing, and replaced back
            /// with %.
            /// </summary>
            private const string ANSIIMarkerForPercent = "&#37;";

            /// <summary>
            /// Regular expression for matching parameter names (e.g. %PrimaryCloudName;) in the
            /// message.
            /// </summary>
            private static readonly Regex ParameterRegex = new Regex(
                @"%[^%]*?;",
                RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.Compiled);

            /// <summary>
            /// Resource manager instance for fetching the messages from the resource file.
            /// </summary>
            private static ResourceManager resourceManager;

            /// <summary>
            /// Initializes static members of the <see cref="MessageFormatter"/> class.
            /// </summary>
            static MessageFormatter()
            {
                resourceManager =
                    new ResourceManager(
                        "MarsAgent.ErrorHandling.Resources.Errors",
                        typeof(MarsAgentException).Assembly);
            }

            /// <summary>
            /// Formats and returns the message string in the given locale.
            /// </summary>
            /// <param name="resourceId">The resource ID.</param>
            /// <param name="parameters">Collection of named parameters for creating message.
            /// </param>
            /// <param name="culture">The culture, in which the string is needed.</param>
            /// <returns>
            /// Formatted message in the given locale. Empty string if resource not found.
            /// </returns>
            public static string GetFormattedMessageString(
                string resourceId,
                IDictionary<string, string> parameters,
                string culture)
            {
                string formatString = GetResourceString(resourceId, culture);
                return FormatString(formatString, parameters);
            }

            /// <summary>
            /// Get resource string for the specified resource ID. Empty string if resource is
            /// not found.
            /// </summary>
            /// <param name="resourceId">The resource ID.</param>
            /// <param name="culture">The culture, in which the string is needed.</param>
            /// <returns>Resource string in the given locale. Null if resource not found.
            /// </returns>
            private static string GetResourceString(
                string resourceId,
                string culture)
            {
                CultureInfo cultureInfo = null;
                try
                {
                    cultureInfo = new CultureInfo(culture ?? string.Empty);
                }
                catch (CultureNotFoundException)
                {
                    // Client can send a locale which is not supported.
                    // Unsupported culture will default to 'en-US'.
                    cultureInfo = new CultureInfo("en-US");
                }

                return resourceManager.GetString(resourceId, cultureInfo);
            }

            /// <summary>
            /// Replace all the message parameters with that in the list of parameters passed.
            /// </summary>
            /// <param name="formatString">The formatted message in which the values need to be
            /// replaced.</param>
            /// <param name="parameters">A key value pair of message parameters and values.</param>
            /// <returns>The string with all the parameters replaced.</returns>
            private static string FormatString(
                string formatString,
                IDictionary<string, string> parameters)
            {
                if (string.IsNullOrWhiteSpace(formatString))
                {
                    return string.Empty;
                }

                StringBuilder sb = new StringBuilder();

                sb.Append(formatString);

                // hide percentage escape characters using their ANSI marker
                sb.Replace("%%", ANSIIMarkerForPercent);

                foreach (Match param in MessageFormatter.ParameterRegex.Matches(formatString))
                {
                    string key = param.Value.Substring(1, param.Length - 2);
                    string parameter;
                    if (parameters == null || !parameters.TryGetValue(key, out parameter))
                    {
                        parameter = MessageFormatter.NoParameterMarker;
                    }

                    sb.Replace(param.Value, parameter);
                }

                sb.Replace(ANSIIMarkerForPercent, "%");

                return sb.ToString();
            }
        }
    }
}
