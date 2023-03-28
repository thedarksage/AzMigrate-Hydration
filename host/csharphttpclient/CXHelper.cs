using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HttpCommunication
{
    public static class CXHelper
    {       
        public const string RequestVersion = "1.0";
        const string ClientNonceTimeStamp = "ts:";

        public static string GenerateRequestId()
        {
            return GenerateRandomNonce(32, true);
        }

        public static string GenerateRandomNonce(int count, bool includeTimestamp)
        {
            string randNum = "";
            DateTime dt = DateTime.Now;
            TimeSpan timeSpan = dt.Subtract(new DateTime(1970, 1, 1, 0, 0, 0, System.DateTimeKind.Utc));
            long timeStamp = (long)timeSpan.TotalSeconds;
            int seed = (int)(timeSpan.Seconds * 1000 + timeSpan.Milliseconds);
            Random random = new Random(seed);
            if (includeTimestamp)
            {
                randNum = ClientNonceTimeStamp + timeStamp + "-";
            }
            while(randNum.Length < count)
            {
                randNum += random.Next().ToString("x");
            }
            return randNum.Substring(0, count);
        }
    }
}
