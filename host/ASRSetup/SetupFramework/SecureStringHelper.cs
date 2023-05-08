using System;
using System.Runtime.InteropServices;
using System.Security;

namespace ASRSetupFramework
{
    /// <summary>
    /// Methods to convert string to SecureString and vice versa.
    /// </summary>
    public class SecureStringHelper
    {
        /// <summary>
        /// To Convert SecureString into a string
        /// </summary>
        /// <param name="value">Secure string to be converted</param>
        /// <returns>string object.</returns>
        public static string SecureStringToString(SecureString value)
        {
            IntPtr bstr = Marshal.SecureStringToBSTR(value);

            try
            {
                return Marshal.PtrToStringBSTR(bstr);
            }
            finally
            {
                Marshal.FreeBSTR(bstr);
            }
        }

        /// <summary>
        /// To Convert string to a secure string
        /// </summary>
        /// <param name="value">Value of the string to be converted</param>
        /// <returns>SecureString object.</returns>
        public static SecureString StringToSecureString(string value)
        {
            SecureString secureString = new SecureString();
            char[] valueChars = value.ToCharArray();

            foreach (char c in valueChars)
            {
                secureString.AppendChar(c);
            }

            return secureString;
        }
    }
}
