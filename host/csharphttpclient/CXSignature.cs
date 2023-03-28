using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Security.Cryptography;

namespace HttpCommunication
{
    public class CXSignature
    {        
        private string passphrase;        
        private string fingerprint;

        public string AccessKeyId { get; private set; }
        public string Version { get; private set; }

        public CXSignature(string accesskeyid, string passphrase, string version)
        {
            this.AccessKeyId = accesskeyid;
            this.passphrase = passphrase;
            this.Version = version;
        }

        public CXSignature(string accesskeyid, string passphrase, string version, string fingerprint)
        {
            this.AccessKeyId = accesskeyid;
            this.passphrase = passphrase;
            this.Version = version;
            this.fingerprint = fingerprint;
        }

        /*
         * Get Function Signature
         */
        public string ComputeSignatureMessageAuth(string requestId, string[] functionNames)
        {

            // Create Plain Text
            string plainText = requestId + Version;
            foreach (string functionName in functionNames)
            {
                plainText += functionName;
            }
            plainText += AccessKeyId + "scout";
            string signature = CreateHMACSHA1(plainText, passphrase);
            byte[] encData_byte = new byte[signature.Length];
            encData_byte = System.Text.ASCIIEncoding.ASCII.GetBytes(signature);
            signature = Convert.ToBase64String(encData_byte);
            return signature;
        }

        /*
         * Computes Signature for ComponentAuth Authentication Method
         */
        public string ComputeSignatureComponentAuth(string requestId, string[] functionNames, Uri uri, string requestMethod)
        {
            char delimiter = ':';
            string strToSign = GetStringToSign(delimiter, requestId, functionNames, requestMethod, uri.AbsolutePath);
            string passphraseHash = CreateSHA256(passphrase);
            string strToSignHash = CreateHMACSHA256(strToSign, passphraseHash);
            string fingerprintHash = "";

            // If the API call is over Secure Channel (HTTPS)
            if (uri.Scheme == Uri.UriSchemeHttps)
            {
                fingerprintHash = CreateHMACSHA256(fingerprint, passphraseHash);
            }
            string signature = CreateHMACSHA256(fingerprintHash + delimiter + strToSignHash, passphraseHash);
            return signature;
        }

        /*
         * Builds String to sign
         */
        private string GetStringToSign(char delimiter, string requestId, string[] functionNames, string requestMethod, string accessFile)
        {
            StringBuilder strToSign = new StringBuilder();
            strToSign.AppendFormat("{0}{1}{2}{1}", requestMethod, delimiter, accessFile);
            foreach (string functionName in functionNames)
            {
                strToSign.Append(functionName);
            }
            strToSign.AppendFormat("{0}{1}{0}{2}", delimiter, requestId, Version);
            return strToSign.ToString();
        }

        /*
         * Create HMACSHA256
         */
        public static string CreateHMACSHA256(string message, string key)
        {
            System.Text.ASCIIEncoding encoding = new System.Text.ASCIIEncoding();
            byte[] keyByte = encoding.GetBytes(key);
            string HMACString;
            using (HMACSHA256 hmacsha256 = new HMACSHA256(keyByte))
            {
                byte[] messageBytes = encoding.GetBytes(message);
                byte[] hashmessage = hmacsha256.ComputeHash(messageBytes);
                HMACString = ByteToString(hashmessage, false);
            }
            return HMACString;
        }

        /*
         * Create HMACSHA-1
         */
        public static string CreateHMACSHA1(string message, string key)
        {
            System.Text.ASCIIEncoding encoding = new System.Text.ASCIIEncoding();
            byte[] keyByte = encoding.GetBytes(key);
            string HMACString;
            using (HMACSHA1 hmacsha1 = new HMACSHA1(keyByte))
            {
                byte[] messageBytes = encoding.GetBytes(message);
                byte[] hashmessage = hmacsha1.ComputeHash(messageBytes);
                HMACString = ByteToString(hashmessage, false);
            }
            return HMACString;
        }

        /*
         * Gives SHA256 hash of the given message.
         */
        public static string CreateSHA256(String message)
        {
            string chksm = "";
            using (SHA256 sha256Managed = new SHA256Managed())
            {
                byte[] msgBytes = System.Text.Encoding.ASCII.GetBytes(message);

                byte[] chksmBytes = sha256Managed.ComputeHash(msgBytes);
                chksm = ByteToString(chksmBytes, false);
            }
            return chksm;
        }

        /*
         * Convert Bytes to String
         */
        public static string ByteToString(byte[] buff, bool toUpperCase)
        {
            string sbinary = "";
            for (int i = 0; i < buff.Length; i++)
            {
                sbinary += buff[i].ToString(toUpperCase ? "X2" : "x2");
            }
            return (sbinary);
        }
    }
}
