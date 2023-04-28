using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    // NOTE-SanKumar-1903: Amazing post for reference:
    // http://web.archive.org/web/20090928112609/http://dotnet.org.za/markn/archive/2008/10/04/handling-passwords.aspx

    /// <summary>
    /// Windows DPAPI helpers
    /// </summary>
    internal static class DataProtectionApi
    {
        /// <summary>
        /// Encrypt <paramref name="plainText"/> to system-local cipher text using DPAPI
        /// </summary>
        /// <param name="plainText">Plain text to protect</param>
        /// <param name="encoding">Enncoding to use</param>
        /// <returns>System-local cipher text</returns>
        public static string ProtectData(SecureString plainText, Encoding encoding)
        {
            string cipherText = null;
            IntPtr plainTextUnmgdCopy = IntPtr.Zero;

            // Guaranteeing zeroing and freeing of the allocated unmanaged
            // plain text copy at any cost
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                code: (userData) => // try
                {
                    // Ensuring allocated memory is assigned at any cost
                    RuntimeHelpers.PrepareConstrainedRegions();

                    try { }
                    finally
                    {
                        // Copy the plain text to unmanaged space to avoid
                        // creating a managed array object instead.
                        plainTextUnmgdCopy = Marshal.SecureStringToGlobalAllocUnicode(plainText);
                    }

                    cipherText = ConvertPlainTextUnmgdToCipherText(plainTextUnmgdCopy, plainText.Length, encoding);
                },
                backoutCode: (userData, exceptionThrown) => // finally
                {
                    if (plainTextUnmgdCopy != IntPtr.Zero)
                    {
                        // Zero and free the copied plain text in unmanaged space
                        Marshal.ZeroFreeGlobalAllocUnicode(plainTextUnmgdCopy);
                        plainTextUnmgdCopy = IntPtr.Zero;
                    }
                },
                userData: null
                );

            return cipherText;
        }

        private static string ConvertPlainTextUnmgdToCipherText(IntPtr plainTextUnmgdCopy, int length, Encoding encoding)
        {
            string cipherText = null;

            // NOTE-Sankumar-1903: Faster than actually passing the char array.
            // Due to this, might allocate a few extra bytes.
            int plainBytesMaxLength = Encoding.Unicode.GetMaxByteCount(length);

            IntPtr plainBytes = IntPtr.Zero;

            // Guaranteeing zeroing and freeing of the Unicode-encoded plain bytes at any cost
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                 code: (userData) => // try
                 {
                     // Ensuring allocated memory is assigned at any cost
                     RuntimeHelpers.PrepareConstrainedRegions();

                     // Allocate and encode the plain bytes in the unmanaged space
                     // to avoid creating a managed array
                     try { }
                     finally
                     {
                         plainBytes = Marshal.AllocHGlobal(plainBytesMaxLength);
                     }

                     int plainBytesActualLength;

                     unsafe
                     {
                         plainBytesActualLength = encoding.GetBytes(
                             chars: (char*)plainTextUnmgdCopy,
                             charCount: length,
                             bytes: (byte*)plainBytes,
                             byteCount: plainBytesMaxLength);
                     }

                     // TODO-SanKumar-1904: Should we allocated even this object
                     // in the unmanaged space, since this object would expose
                     // the length of the plain bytes?
                     var plainBlob = new NativeMethods.DATA_BLOB
                     {
                         cbData = plainBytesActualLength,
                         pbData = plainBytes
                     };

                     cipherText = ConvertPlainBlobToCipherText(plainBlob);
                 },
                 backoutCode: (userData, exceptionThrown) => // finally
                 {
                     if (plainBytes != IntPtr.Zero)
                     {
                         // Zero and free the encoded plain bytes in unmanaged space
                         NativeMethods.RtlSecureZeroMemory(plainBytes, plainBytesMaxLength);
                         Marshal.FreeHGlobal(plainBytes);
                         plainBytes = IntPtr.Zero;
                     }
                 },
                 userData: null
                 );

            return cipherText;
        }

        private static string ConvertPlainBlobToCipherText(NativeMethods.DATA_BLOB plainBlob)
        {
            string cipherText = null;
            var cipherBlob = new NativeMethods.DATA_BLOB();

            // Guaranteeing the freeing of the generated cipher bytes at any cost
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                code: (userData) => // try
                {
                    if (!NativeMethods.CryptProtectData(
                                       ref plainBlob, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero,
                                       NativeMethods.CRYPTPROTECT_FLAGS.LOCAL_MACHINE | NativeMethods.CRYPTPROTECT_FLAGS.UI_FORBIDDEN,
                                       out cipherBlob))
                    {
                        throw new Win32Exception();
                    }

                    // Cipher text. Using managed array, since no additional security is needed
                    byte[] cipherBytes = new byte[cipherBlob.cbData];

                    Marshal.Copy(
                        source: cipherBlob.pbData,
                        destination: cipherBytes,
                        startIndex: 0,
                        length: cipherBytes.Length);

                    cipherText = Convert.ToBase64String(cipherBytes, Base64FormattingOptions.None);
                },
                backoutCode: (userData, exceptionThrown) => // finally
                {
                    if (cipherBlob.pbData != IntPtr.Zero)
                    {
                        // Cipher text. No need to zero it out.
                        Marshal.FreeHGlobal(cipherBlob.pbData);
                        cipherBlob.pbData = IntPtr.Zero;
                        cipherBlob.cbData = 0;
                    }
                },
                userData: null);

            return cipherText;
        }

        /// <summary>
        /// Decrypt system-local <paramref name="cipherText"/> to plain text using DPAPI
        /// </summary>
        /// <param name="cipherText">System-local cipher text</param>
        /// <param name="encoding">Encoding to use</param>
        /// <returns>Corresponding Plain text</returns>
        public static SecureString UnprotectData(string cipherText, Encoding encoding)
        {
            SecureString plainText = null;

            byte[] cipherBytes = Convert.FromBase64String(cipherText);

            var cipherBlob = new NativeMethods.DATA_BLOB();

            // Guaranteeing freeing of the unmanaged copy of cipher bytes at any cost
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                code: (userData) => // try
                    {
                        // Ensuring allocated memory is assigned at any cost
                        RuntimeHelpers.PrepareConstrainedRegions();

                        try { }
                        finally
                        {
                            cipherBlob.pbData = Marshal.AllocHGlobal(cipherBytes.Length);
                        }

                        cipherBlob.cbData = cipherBytes.Length;

                        Marshal.Copy(
                            source: cipherBytes,
                            startIndex: 0,
                            destination: cipherBlob.pbData,
                            length: cipherBytes.Length);

                        plainText = ConvertCipherBlobToPlainText(cipherBlob, encoding);
                    },
                backoutCode: (userData, exceptionThrown) => // finally
                    {
                        if (cipherBlob.pbData != IntPtr.Zero)
                        {
                            // Cipher text. No need to zero it out.
                            Marshal.FreeHGlobal(cipherBlob.pbData);
                            cipherBlob.pbData = IntPtr.Zero;
                            cipherBlob.cbData = 0;
                        }
                    },
                userData: null
                );

            return plainText;
        }

        private static SecureString ConvertCipherBlobToPlainText(NativeMethods.DATA_BLOB cipherBlob, Encoding encoding)
        {
            SecureString plainText = null;
            var plainBlob = new NativeMethods.DATA_BLOB();

            // Guaranteeing zeroing and freeing of the DPApi generated plain bytes
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                code: (userData) => // try
                {
                    if (!NativeMethods.CryptUnprotectData(ref cipherBlob, IntPtr.Zero, IntPtr.Zero,
                        IntPtr.Zero, IntPtr.Zero, NativeMethods.CRYPTPROTECT_FLAGS.UI_FORBIDDEN,
                        out plainBlob))
                    {
                        throw new Win32Exception();
                    }

                    plainText = ConvertPlainBytesToPlainText(
                        plainBytes: plainBlob.pbData,
                        length: plainBlob.cbData,
                        encoding: encoding);
                },
                backoutCode: (userData, exceptionThrown) => // finally
                {
                    if (plainBlob.pbData != IntPtr.Zero)
                    {
                        // Zero and free the plain bytes returned by the DP API
                        NativeMethods.RtlSecureZeroMemory(plainBlob.pbData, plainBlob.cbData);

                        Marshal.FreeHGlobal(plainBlob.pbData);
                        plainBlob.pbData = IntPtr.Zero;
                        plainBlob.cbData = 0;
                    }
                },
                userData: null
                );

            return plainText;
        }

        private static SecureString ConvertPlainBytesToPlainText(IntPtr plainBytes, int length, Encoding encoding)
        {
            SecureString plainText = null;
            int maxCharCount = encoding.GetMaxCharCount(length);
            IntPtr plainTextUnmgdCopy = IntPtr.Zero;

            // Guaranteeing zeroing and freeing of the unmanaged copy of the plain text
            RuntimeHelpers.ExecuteCodeWithGuaranteedCleanup(
                code: (userData) => // try
                {
                    // Ensuring that allocated memory is assigned at any cost
                    RuntimeHelpers.PrepareConstrainedRegions();

                    try { }
                    finally
                    {
                        // Allocating unmanaged memory for decoding the plain text
                        // to avoid allocating managed array.
                        plainTextUnmgdCopy = Marshal.AllocHGlobal(maxCharCount);
                    }

                    int charCount;

                    unsafe
                    {
                        charCount = encoding.GetChars(
                            bytes: (byte*)plainBytes, byteCount: length,
                            chars: (char*)plainTextUnmgdCopy, charCount: maxCharCount);

                        plainText = new SecureString((char*)plainTextUnmgdCopy, charCount);
                        plainText.MakeReadOnly();
                    }
                },
                backoutCode: (userData, exceptionThrown) => // finally
                {
                    if (plainTextUnmgdCopy != IntPtr.Zero)
                    {
                        // Zeroing and freeing the unmanaged buffer allocated to cache plain text
                        NativeMethods.RtlSecureZeroMemory(plainTextUnmgdCopy, maxCharCount);
                        Marshal.FreeHGlobal(plainTextUnmgdCopy);
                        plainTextUnmgdCopy = IntPtr.Zero;
                    }
                },
                userData: null
                );

            return plainText;
        }

        private static class NativeMethods
        {
            // TODO-SanKumar-1904: This call is not available, since the function has been inlined.

            //[DllImport("kernel32.dll", CallingConvention = CallingConvention.Winapi)]
            //public static extern void RtlSecureZeroMemory(IntPtr ptr, [MarshalAs(UnmanagedType.SysUInt)] IntPtr size);

            public static void RtlSecureZeroMemory(IntPtr ptr, int size)
            {
                if (ptr == IntPtr.Zero)
                    return;

                // TODO-SanKumar-1904: The whole point of trying to invoke
                // RtlSecureZeroMemory is to get reliable cleaning of memory,
                // irrespective of compiler optimizations. So, manually setting
                // every byte, since I'm not really sure, if a block set of 0
                // would be optimized and avoided beneath.
                for (int ind = 0; ind < size; ind++)
                    Marshal.WriteByte(ptr, ind, 0);
            }

            [Flags]
            public enum CRYPTPROTECT_FLAGS : UInt32
            {
                NONE = 0,

                //
                // CryptProtectData and CryptUnprotectData dwFlags
                //

                // for remote-access situations where ui is not an option
                // if UI was specified on protect or unprotect operation, the call
                // will fail and GetLastError() will indicate ERROR_PASSWORD_RESTRICTION
                UI_FORBIDDEN = 0x1,

                // per machine protected data -- any user on machine where CryptProtectData
                // took place may CryptUnprotectData
                LOCAL_MACHINE = 0x4,

                // force credential synchronize during CryptProtectData()
                // Synchronize is only operation that occurs during this operation
                //CRED_SYNC = 0x8,

                // Generate an Audit on protect and unprotect operations
                AUDIT = 0x10,

                // Protect data with a non-recoverable key
                NO_RECOVERY = 0x20,

                // Verify the protection of a protected blob
                VERIFY_PROTECTION = 0x40
            }

            [DllImport("Crypt32.dll", CallingConvention = CallingConvention.Winapi, CharSet = CharSet.Unicode, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern bool CryptProtectData(
                [In]            ref DATA_BLOB pDataIn,
                [In] [Optional] IntPtr szDataDescr,
                [In] [Optional] IntPtr pOptionalEntropy,
                [In] [Optional] IntPtr pvReserved,
                [In] [Optional] IntPtr pPromptStruct,
                [In]            CRYPTPROTECT_FLAGS dwFlags,
                [Out]           out DATA_BLOB pDataOut);

            [DllImport("Crypt32.dll", CallingConvention = CallingConvention.Winapi, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern bool CryptUnprotectData(
                [In]                ref DATA_BLOB pDataIn,
                [Out] [Optional]    IntPtr ppszDataDescr,
                [In] [Optional]     IntPtr pOptionalEntropy,
                [In] [Optional]     IntPtr pvReserved,
                [In] [Optional]     IntPtr pPromptStruct,
                [In]                CRYPTPROTECT_FLAGS dwFlags,
                [Out]               out DATA_BLOB pDataOut);

            [StructLayout(LayoutKind.Sequential)]
            public struct DATA_BLOB
            {
                public Int32 cbData;
                public IntPtr pbData;
            }
        }
    }
}
