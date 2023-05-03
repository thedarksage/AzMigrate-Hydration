//---------------------------------------------------------------
//  <copyright file="VimExtensions.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     28-April-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security;
using VMware.Vim;

namespace VMware.Client
{
    /// <summary>
    /// Helper methods.
    /// </summary>
    public static class VimExtensions
    {
        /// <summary>
        /// Special character map used by EncodeNameSpecialChars.
        /// </summary>
        private static Dictionary<string, string> specialCharsMap = new Dictionary<string, string>
        {
            { "%", "%25" },
            { "&", "&amp;" },
            { "/", "%2f" },
            { "\\", "%5c" },
            { "<", "&lt;" },
            { ">", "$gt;" },
            { "\"", "&quot;" }
        };

        /// <summary>
        /// Case insensitive Contains() comparison.
        /// </summary>
        /// <param name="source">The source string.</param>
        /// <param name="toCheck">The string to check for.</param>
        /// <param name="comp">The comparison type.</param>
        /// <returns>True if string is found. False if not.</returns>
        public static bool Contains(this string source, string toCheck, StringComparison comp)
        {
            return source.IndexOf(toCheck, comp) >= 0;
        }

        /// <summary>
        /// Converts the VMware tools status to a Info.xml friendly string.
        /// </summary>
        /// <param name="toolsStatus">The tools status.</param>
        /// <returns>The friendly string.</returns>
        public static string ToXmlString(this VirtualMachineToolsStatus toolsStatus)
        {
            if (toolsStatus == VirtualMachineToolsStatus.toolsOk)
            {
                return "OK";
            }
                
            if (toolsStatus == VirtualMachineToolsStatus.toolsNotInstalled)
            {
                return "NotInstalled";
            }

            if (toolsStatus == VirtualMachineToolsStatus.toolsNotRunning)
            {
                return "NotRunning";
            }

            if (toolsStatus == VirtualMachineToolsStatus.toolsOld)
            {
                return "OutOfdate";
            }

            return string.Empty;
        }

        /// <summary>
        /// For entity names having special characters, while retrieving getting with decoded
        /// characters.
        /// </summary>
        /// <param name="name">The string to encode.</param>
        /// <returns>The encoded string.</returns>
        public static string EncodeNameSpecialChars(this string name)
        {
            string encodedName = name;
            foreach (var item in VimExtensions.specialCharsMap)
            {
                encodedName = encodedName.Replace(item.Key, item.Value);
            }

            return encodedName;
        }

        /// <summary>
        /// Converts SecureString into a string.
        /// </summary>
        /// <param name="value">Secure string to be converted.</param>
        /// <returns>String value converted from secure string.</returns>
        public static string SecureStringToString(SecureString value)
        {
            IntPtr bstr = IntPtr.Zero;

            try
            {
                bstr = Marshal.SecureStringToBSTR(value);
                return Marshal.PtrToStringBSTR(bstr);
            }
            finally
            {
                if (bstr != IntPtr.Zero)
                {
                    Marshal.FreeBSTR(bstr);
                }
            }
        }
    }
}
