using System;
using System.Globalization;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities around Random functionalities
    /// </summary>
    internal static class RandUtils
    {
        private static readonly Random randObj = new Random();
        private static readonly SemaphoreSlim randSemaphore = new SemaphoreSlim(1);

        /// <summary>
        /// Generate random nonce (request Id for Legacy CS interaction)
        /// </summary>
        /// <param name="count">
        /// Minimum expected count of chars in nonce
        /// </param>
        /// <param name="includeTimestamp">
        /// If true, prepends timestamp to nonce
        /// </param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>Nonce string</returns>
        public static async Task<string> GenerateRandomNonce(
            int count, bool includeTimestamp, CancellationToken ct)
        {
            // When two random objects created in close succession, they
            // tend to possibly give same sequence of random numbers.
            // https://docs.microsoft.com/en-us/dotnet/api/system.random?view=netframework-4.7.2#avoiding-multiple-instantiations
            // Instead, we should use a single object under lock (since
            // it's not thread-safe for generating long sequence of operations).

            StringBuilder nonce = new StringBuilder(count + 8 + (includeTimestamp ? 15 : 0));

            if (includeTimestamp)
            {
                long secsSinceEpoch = DateTimeUtils.GetSecondsAfterEpoch();
                nonce.AppendFormat(CultureInfo.InvariantCulture, "ts:{0}-", secsSinceEpoch);
            }

            while (nonce.Length < count)
            {
                int next;

                // Lightweight alternative for lock()
                await randSemaphore.WaitAsync(ct).ConfigureAwait(false);
                try
                {
                    next = randObj.Next();
                }
                finally
                {
                    randSemaphore.Release();
                }

                ct.ThrowIfCancellationRequested();

                // TODO-SanKumar-1903: We could reduce the time taken in this method
                // by introducing padding but that would introduce a lot 0's and
                // possibly reducing the randomness. Also, the current implementation
                // is exactly the perl implementation
                nonce.AppendFormat(CultureInfo.InvariantCulture, "{0:x}", next);
            }

            // NOTE-SanKumar-1902: This will actually generate 1 char less than
            // count always but carrying forward the legacy implementation from perl code.
            return nonce.ToString(0, count - 1);
        }

        public static async Task<int> GetNextIntAsync(int upperLimit, CancellationToken ct)
        {
            int next;
            await randSemaphore.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                next = randObj.Next(upperLimit);
            }
            finally
            {
                randSemaphore.Release();
            }

            return next;
        }
    }
}
