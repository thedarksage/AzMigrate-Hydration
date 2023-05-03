using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System.Diagnostics;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    public static class MdsLctlInMageAdminLogV2TraceSourceExtensions
    {
        #region Debug Message

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, string message)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, null, 0, message, null);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, RcmOperationContext opContext, string message)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, opContext, 0, message, null);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, int id, string message)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, null, id, message, null);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, RcmOperationContext opContext, int id, string message)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, opContext, id, message);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, null, 0, format, args);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, RcmOperationContext opContext, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, opContext, 0, format, args);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, int id, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, null, id, format, args);
        }

        [Conditional("DEBUG")]
        public static void DebugAdminLogV2Message(this TraceSource traceSource, RcmOperationContext opContext, int id, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, TraceEventType.Verbose, opContext, id, format, args);
        }

        #endregion Debug Message

        #region Trace Message

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, string message)
        {
            WriteAdminLogV2Message(traceSource, eventType, null, 0, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, string message)
        {
            WriteAdminLogV2Message(traceSource, eventType, opContext, 0, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, int id, string message)
        {
            WriteAdminLogV2Message(traceSource, eventType, null, id, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, string message)
        {
            WriteAdminLogV2Message(traceSource, eventType, opContext, id, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, eventType, null, 0, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, eventType, opContext, 0, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, int id, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, eventType, null, id, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2Message(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, string format, params object[] args)
        {
            WriteAdminLogV2Message(traceSource, eventType, opContext, id, format, args);
        }

        private static void WriteAdminLogV2Message(TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, string format, params object[] args)
        {
            if (opContext?.ClientRequestId != null || opContext?.ActivityId != null)
            {
                WriteAdminLogV2MsgWithExtProps(traceSource, eventType, opContext, id, null, format, args);
                return;
            }

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (traceSource == null)
                {
                    Debug.Assert(false);
                    return;
                }

                // TODO-SanKumar-1903: Is this needed?

                // Start, Stop, Suspend, Resume, Transfer are not recognized
                // error levels at the Telemetry and RCM services.
                if (eventType != TraceEventType.Error &&
                    eventType != TraceEventType.Warning &&
                    eventType != TraceEventType.Information &&
                    eventType != TraceEventType.Verbose)
                {
                    Debug.Assert(false);
                    return;
                }

                if (format != null && args != null && args.Length > 0)
                    traceSource.TraceEvent(eventType, id, format, args);
                else if (format != null)
                    traceSource.TraceEvent(eventType, id, format);
                else
                    traceSource.TraceEvent(eventType, id);
            });
        }

        #endregion Trace Message

        #region Trace Message with Extended properties

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, object extendedProperties, string message)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, null, 0, extendedProperties, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, object extendedProperties, string message)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, opContext, 0, extendedProperties, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, object extendedProperties, string format, params object[] args)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, null, 0, extendedProperties, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, object extendedProperties, string format, params object[] args)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, opContext, 0, extendedProperties, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, int id, object extendedProperties, string message)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, null, id, extendedProperties, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, object extendedProperties, string message)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, opContext, id, extendedProperties, message, null);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, int id, object extendedProperties, string format, params object[] args)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, null, id, extendedProperties, format, args);
        }

        [Conditional("TRACE")]
        public static void TraceAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, object extendedProperties, string format, params object[] args)
        {
            WriteAdminLogV2MsgWithExtProps(traceSource, eventType, opContext, id, extendedProperties, format, args);
        }

        private static void WriteAdminLogV2MsgWithExtProps(this TraceSource traceSource, TraceEventType eventType, RcmOperationContext opContext, int id, object extendedProperties, string format, params object[] args)
        {
            TaskUtils.RunAndIgnoreException(() =>
            {
                if (traceSource == null)
                {
                    Debug.Assert(false);
                    return;
                }

                // TODO-SanKumar-1903: Is this needed?

                // Start, Stop, Suspend, Resume, Transfer are not recognized
                // error levels at the Telemetry and RCM services.
                if (eventType != TraceEventType.Error &&
                    eventType != TraceEventType.Warning &&
                    eventType != TraceEventType.Information &&
                    eventType != TraceEventType.Verbose)
                {
                    Debug.Assert(false);
                    return;
                }

                // TODO-SanKumar-1903: To avoid creation of object, which would
                // only be dropped right away, we could check if the TraceSource
                // would be able to handle this eventType. But if there are
                // custom trace sources using custom switches, their internal
                // logic might be missed.

                //if (traceSource.Switch != null && !traceSource.Switch.ShouldTrace(eventType))
                //    return;

                traceSource.TraceData(eventType, id, new InMageAdminLogV2TracePacket()
                {
                    ExtendedProperties = extendedProperties,
                    FormatString = format,
                    FormatArgs = args,
                    OpContext = opContext
                });
            });
        }

        #endregion Trace Message with Extended properties
    }
}
