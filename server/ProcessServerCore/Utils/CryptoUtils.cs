using System;
using System.Globalization;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities for cryptographic operations
    /// </summary>
    internal static class CryptoUtils
    {
        /// <summary>
        /// Get hexadecimal dump of array of given bytes
        /// </summary>
        /// <param name="bytes">Byte array to convert</param>
        /// <returns>Converted hexadecimal dump string</returns>
        public static string GetHexDump(this byte[] bytes)
        {
            return string.Concat(
                bytes.Select(b => b.ToString("x2", CultureInfo.InvariantCulture)));
        }

        /// <summary>
        /// Get SHA256 hash of input string
        /// </summary>
        /// <param name="input">Input string</param>
        /// <returns>SHA256 hash string</returns>
        public static string GetSha256Hash(this string input)
        {
            byte[] hashBytes;

            // TODO-SanKumar-1906: Use native CNG version for performance instead of default managed?
            using (var sha256 = SHA256.Create())
            {
                hashBytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(input));
            }

            return hashBytes.GetHexDump();
        }

        /// <summary>
        /// Compute HmacSha256 of the input string with the key
        /// </summary>
        /// <param name="input">Input string</param>
        /// <param name="key">Key string</param>
        /// <returns>Hmac256 string</returns>
        public static string GetHmacSha256Hash(this string input, string key)
        {
            byte[] hashBytes;

            using (var hmacSha256 = HMAC.Create("HMACSHA256"))
            {
                hmacSha256.Key = Encoding.UTF8.GetBytes(key);
                hashBytes = hmacSha256.ComputeHash(Encoding.ASCII.GetBytes(input));
            }

            return hashBytes.GetHexDump();
        }

        public static string GetMd5Checksum(string content)
        {
            byte[] hashBytes;

            using (MD5 md5 = MD5.Create())
                hashBytes = md5.ComputeHash(Encoding.UTF8.GetBytes(content));

            return Convert.ToBase64String(hashBytes);

        }
    }
}
