using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace CSAuthModuleNS
{
    internal static class Signature
    {        
        
        internal static String ComputeSignatureComponentAuth(String requestId, String functionRequest, Uri uri, String requestMethod, String version)
        {

            char delimiter = ':';
            String thumbprint = "";
            String passphrase = "";
            SecurityInfo.ReadPassphrase(ref passphrase);

            String passphraseHash = CreateSHA256(passphrase);


            String thumbprintHash = "";

            // if the API call is over Secure Channel (HTTPS)
            if (uri.Scheme == Uri.UriSchemeHttps)
            {
                SecurityInfo.ReadFingerprint(ref thumbprint);
                thumbprintHash = CreateHMACSHA256(thumbprint, passphraseHash);
            }


            String strToSign = GetStringToSign(delimiter, requestId, functionRequest, requestMethod, uri.AbsolutePath, version);
            String strToSignHash = CreateHMACSHA256(strToSign, passphraseHash);


            String signature = CreateHMACSHA256(thumbprintHash + delimiter + strToSignHash, passphraseHash);
            return signature;
        }

        private static String CreateSHA256(String message)
        {
            string chksm = "";
            // Replaced SHA256Managed with SHA256CryptoServiceProvider for FIPS compliance.
            // See http://stackoverflow.com/questions/3554882/difference-between-sha256cryptoserviceprovider-and-sha256managed.
            using (SHA256CryptoServiceProvider sha256Provider = new SHA256CryptoServiceProvider())
            {
                byte[] msgBytes = System.Text.Encoding.ASCII.GetBytes(message);

                byte[] chksmBytes = sha256Provider.ComputeHash(msgBytes);
                chksm = ByteToString(chksmBytes, false);
            }
            return chksm;
        }

        private static String CreateHMACSHA256(String message, String key)
        {

            System.Text.ASCIIEncoding encoding = new System.Text.ASCIIEncoding();
            byte[] keyByte = encoding.GetBytes(key);
            String HMACString;
            using (HMACSHA256 hmacsha256 = new HMACSHA256(keyByte))
            {
                byte[] messageBytes = encoding.GetBytes(message);
                byte[] hashmessage = hmacsha256.ComputeHash(messageBytes);
                HMACString = ByteToString(hashmessage, false);
            }
            return HMACString;

        }

        private static String ByteToString(byte[] buff, bool toUpperCase)
        {
            try
            {
                String sbinary = "";
                for (int i = 0; i < buff.Length; i++)
                {
                    sbinary += buff[i].ToString(toUpperCase ? "X2" : "x2");
                }
                return (sbinary);
            }
            catch(Exception)
            { 
                throw; 
            }
        }

        private static String GetStringToSign(char delimiter, String requestId, String functionRequest, String requestMethod, String accessFile, String version)
        {
            StringBuilder strToSign = new StringBuilder();
            strToSign.AppendFormat("{0}{1}{2}{1}{3}", requestMethod, delimiter, accessFile, functionRequest);
            strToSign.AppendFormat("{0}{1}{0}{2}", delimiter, requestId, version);
            return strToSign.ToString();
        }
    }
}
