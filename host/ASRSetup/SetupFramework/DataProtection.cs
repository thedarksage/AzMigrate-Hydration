using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;

namespace ASRSetupFramework
{
    /// <summary>
    /// Handles encryption and decryption using DPAPI
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.OrderingRules",
        "SA1201:ElementsMustAppearInTheCorrectOrder",
        Justification = "Native methods being imported.")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1600:ElementsMustBeDocumented",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.ReadabilityRules",
        "SA1121:UseBuiltInTypeAlias",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1307:AccessibleFieldsMustBeginWithUpperCaseLetter",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1306:FieldNamesMustBeginWithLowerCaseLetter",
        Justification = "DLL imported methods")]
    public class DataProtection
    {
        /// <summary>
        /// Encrypt the data using CryptProtect API.
        /// </summary>
        /// <param name="plainText">plain text to be encrypted.</param>
        /// <returns>Encrypted bytes of data.</returns>
        public static byte[] Encrypt(byte[] plainText)
        {
            bool retVal = false;
            DATA_BLOB plainTextBlob = new DATA_BLOB();
            DATA_BLOB cipherTextBlob = new DATA_BLOB();
            DATA_BLOB entropyBlob = new DATA_BLOB();
            byte[] cipherText = null;

            CRYPTPROTECT_PROMPTSTRUCT prompt = new CRYPTPROTECT_PROMPTSTRUCT();
            InitPromptstruct(ref prompt);
            int dwFlags = CryptProtectLocalMachine | CryptProtectUIForbidden;

            try
            {
                int bytesSize = plainText.Length;
                plainTextBlob.pbData = Marshal.AllocHGlobal(bytesSize);
                if (IntPtr.Zero == plainTextBlob.pbData)
                {
                    throw new Exception("Unable to allocate plaintext buffer.");
                }

                plainTextBlob.cbData = bytesSize;
                Marshal.Copy(plainText, 0, plainTextBlob.pbData, bytesSize);

                retVal = CryptProtectData(
                    ref plainTextBlob,
                    string.Empty,
                    ref entropyBlob,
                    IntPtr.Zero,
                    ref prompt,
                    dwFlags,
                    ref cipherTextBlob);
                if (false == retVal)
                {
                    throw new Exception("Encryption failed, with win32 error :" + Marshal.GetLastWin32Error());
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception encrypting. " + ex.Message);
            }
            finally
            {
                if (IntPtr.Zero != plainTextBlob.pbData)
                {
                    Marshal.FreeHGlobal(plainTextBlob.pbData);
                }

                if (IntPtr.Zero != entropyBlob.pbData)
                {
                    Marshal.FreeHGlobal(entropyBlob.pbData);
                }

                if (IntPtr.Zero != cipherTextBlob.pbData)
                {
                    cipherText = new byte[cipherTextBlob.cbData];
                    Marshal.Copy(cipherTextBlob.pbData, cipherText, 0, cipherTextBlob.cbData);
                    Marshal.FreeHGlobal(cipherTextBlob.pbData);
                }
            }

            return cipherText;
        }

        /// <summary>
        /// Decrypt the data using CryptUnprotectData API.
        /// </summary>
        /// <param name="cipherText">cipher text to be decrypted.</param>
        /// <returns>Decrypted bytes of data.</returns>
        public static byte[] Decrypt(byte[] cipherText)
        {
            bool retVal = false;
            DATA_BLOB plainTextBlob = new DATA_BLOB();
            DATA_BLOB cipherBlob = new DATA_BLOB();
            DATA_BLOB entropyBlob = new DATA_BLOB();
            CRYPTPROTECT_PROMPTSTRUCT prompt = new
            CRYPTPROTECT_PROMPTSTRUCT();
            InitPromptstruct(ref prompt);
            byte[] plainText = null;

            try
            {
                int cipherTextSize = cipherText.Length;
                cipherBlob.pbData = Marshal.AllocHGlobal(cipherTextSize);
                if (IntPtr.Zero == cipherBlob.pbData)
                {
                    throw new Exception("Unable to allocate cipherText buffer.");
                }

                cipherBlob.cbData = cipherTextSize;
                Marshal.Copy(
                    cipherText, 
                    0, 
                    cipherBlob.pbData,
                    cipherBlob.cbData);

                int dwFlags = CryptProtectLocalMachine | CryptProtectUIForbidden;
                retVal = CryptUnprotectData(
                    ref cipherBlob,
                    null,
                    ref entropyBlob,
                    IntPtr.Zero,
                    ref prompt,
                    dwFlags,
                    ref plainTextBlob);
                if (false == retVal)
                {
                    throw new Exception("Decryption failed with win32 error" + Marshal.GetLastWin32Error());
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception decrypting. " + ex.Message);
            }
            finally
            {
                if (IntPtr.Zero != cipherBlob.pbData)
                {
                    Marshal.FreeHGlobal(cipherBlob.pbData);
                }

                if (IntPtr.Zero != entropyBlob.pbData)
                {
                    Marshal.FreeHGlobal(entropyBlob.pbData);
                }

                if (IntPtr.Zero != plainTextBlob.pbData)
                {
                    plainText = new byte[plainTextBlob.cbData];
                    Marshal.Copy(plainTextBlob.pbData, plainText, 0, plainTextBlob.cbData);
                    Marshal.FreeHGlobal(plainTextBlob.pbData);
                }
            }

            return plainText;
        }

        /// <summary>
        /// Initialize CRYPTPROTECT_PROMPTSTRUCT structure with defaults, this is used by both encrypt and decrypt API's
        /// </summary>
        /// <param name="ps">ref to CRYPTPROTECT_PROMPTSTRUCT which needs to initialized</param>
        private static void InitPromptstruct(ref CRYPTPROTECT_PROMPTSTRUCT ps)
        {
            ps.cbSize = Marshal.SizeOf(typeof(CRYPTPROTECT_PROMPTSTRUCT));
            ps.dwPromptFlags = 0;
            ps.hwndApp = NullPtr;
            ps.szPrompt = null;
        }

        #region DPAPI's Native Wrapper

        [DllImport("Crypt32.dll", SetLastError = true,
            CharSet = System.Runtime.InteropServices.CharSet.Auto)]
        private static extern bool CryptProtectData(
            ref DATA_BLOB pDataIn,
            string szDataDescr,
            ref DATA_BLOB pOptionalEntropy,
            IntPtr pvReserved,
            ref CRYPTPROTECT_PROMPTSTRUCT
            pPromptStruct,
            int dwFlags,
            ref DATA_BLOB pDataOut);

        [DllImport("Crypt32.dll", SetLastError = true,
                    CharSet = System.Runtime.InteropServices.CharSet.Auto)]
        private static extern bool CryptUnprotectData(
            ref DATA_BLOB pDataIn,
            string szDataDescr,
            ref DATA_BLOB pOptionalEntropy,
            IntPtr pvReserved,
            ref CRYPTPROTECT_PROMPTSTRUCT
            pPromptStruct,
            int dwFlags,
            ref DATA_BLOB pDataOut);

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        internal struct DATA_BLOB
        {
            public int cbData;
            public IntPtr pbData;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        internal struct CRYPTPROTECT_PROMPTSTRUCT
        {
            public int cbSize;
            public int dwPromptFlags;
            public IntPtr hwndApp;
            public string szPrompt;
        }

        private static IntPtr NullPtr = (IntPtr)(int)0;
        private const int CryptProtectUIForbidden = 0x1;
        private const int CryptProtectLocalMachine = 0x4;

        #endregion
    }
}