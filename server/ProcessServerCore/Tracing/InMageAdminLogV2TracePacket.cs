using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System.Globalization;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    public class InMageAdminLogV2TracePacket
    {
        public string FormatString;
        public object[] FormatArgs;
        public object ExtendedProperties;
        public string DiskId;
        public string DiskNumber;
        public string ErrorCode;
        public RcmOperationContext OpContext;

        private static readonly JsonSerializerSettings s_jsonSerSettings =
            JsonUtils.GetStandardSerializerSettings(
                indent: false,
                converters: JsonUtils.GetAllKnownConverters());

        // Since standard .Net TraceListeners could be used by the processes,
        // providing this overload of ToString() to even log correctly in them.
        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();

            if (FormatString != null && FormatArgs != null && FormatArgs.Length > 0)
            {
                sb.AppendFormat(CultureInfo.InvariantCulture, FormatString, FormatArgs);
            }
            else
            {
                sb.Append(FormatString);
            }

            if (OpContext?.ClientRequestId != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(
                    CultureInfo.InvariantCulture,
                    $"{nameof(OpContext.ClientRequestId)}: {{0}}",
                    OpContext.ClientRequestId);
            }

            if (OpContext?.ActivityId != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(
                    CultureInfo.InvariantCulture,
                    $"{nameof(OpContext.ActivityId)}: {{0}}",
                    OpContext.ActivityId);
            }

            if (DiskId != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(CultureInfo.InvariantCulture, $"{nameof(DiskId)}: {{0}}", DiskId);
            }

            if (DiskNumber != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(CultureInfo.InvariantCulture, $"{nameof(DiskNumber)}: {{0}}", DiskNumber);
            }

            if (ErrorCode != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(CultureInfo.InvariantCulture, $"{nameof(ErrorCode)}: {{0}}", ErrorCode);
            }

            if (ExtendedProperties != null)
            {
                if (sb.Length > 0)
                    sb.AppendLine();

                sb.AppendFormat(CultureInfo.InvariantCulture, $"{nameof(ExtendedProperties)}: {{0}}",
                    JsonConvert.SerializeObject(ExtendedProperties, s_jsonSerSettings));
            }

            return sb.ToString();
        }
    }
}
