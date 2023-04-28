using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    public class DateTimeUtils
    {
        /// <summary>
        /// Return seconds elapsed after epoch
        /// </summary>
        /// <returns>Seconds elapsed since epoch</returns>
        public static long GetSecondsAfterEpoch()
        {
            return DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        }

        /// <summary>
        /// Returns the seconds elapsed since epoch for a given Datetime
        /// </summary>
        /// <param name="dt">DateTime object</param>
        /// <returns>Seconds elapsed since epoch</returns>
        public static long GetSecondsSinceEpoch(DateTime dt)
        {
            DateTimeOffset dto = new DateTimeOffset(dt);
            return dto.ToUnixTimeSeconds();
        }

        /// <summary>
        /// Returns the seconds elapsed since January 1, 1970
        /// </summary>
        /// <param name="dt">DateTime object</param>
        /// <returns>Seconds elapsed since epoch</returns>
        public static long GetTimeInSecSinceEpoch1970()
        {
            return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds;
        }
    }
}
