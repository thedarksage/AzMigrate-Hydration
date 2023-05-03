using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    public static class DisposeUtils
    {
        public static bool TryDispose(this IDisposable disposable)
        {
            return TryDispose(disposable, null);
        }

        public static bool TryDispose(this IDisposable disposable, TraceSource traceSource)
        {
            try
            {
                disposable?.Dispose();
                return true;
            }
            catch (Exception ex)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Exception hit, while disposing the object : {0}{1}{2}",
                    disposable, Environment.NewLine, ex);

                if (Debugger.IsAttached)
                    Debugger.Break();

                return false;
            }
        }

        public static bool TryDisposeAll(params IDisposable[] disposables)
        {
            return TryDisposeAll(disposables, null);
        }

        public static bool TryDisposeAll(IEnumerable<IDisposable> disposables)
        {
            return TryDisposeAll(disposables, null);
        }

        public static bool TryDisposeAll(IEnumerable<IDisposable> disposables, TraceSource traceSource)
        {
            bool allSuccessful = true;

            if (disposables == null)
                return allSuccessful;

            foreach (var currDisposable in disposables)
                allSuccessful &= currDisposable.TryDispose(traceSource);

            return allSuccessful;
        }
    }

    public sealed class TryDisposableCollection : IDisposable
    {
        private readonly List<IDisposable> m_disposables;

        private readonly TraceSource m_traceSource;

        public TryDisposableCollection(params IDisposable[] disposables)
            : this(disposables, traceSource: null)
        {
        }

        public TryDisposableCollection(IEnumerable<IDisposable> disposables)
            : this(disposables, traceSource: null)
        {
        }

        public TryDisposableCollection(IEnumerable<IDisposable> disposables, TraceSource traceSource)
        {
            m_disposables = new List<IDisposable>(disposables);
            m_traceSource = traceSource;
        }

        public void Dispose()
        {
            GC.SuppressFinalize(this);
            DisposeUtils.TryDisposeAll(m_disposables, m_traceSource);
        }
    }
}
