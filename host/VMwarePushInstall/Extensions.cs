//---------------------------------------------------------------
//  <copyright file="Extensions.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements extension methods.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Security;

using VMware.Interfaces.ExceptionHandling;
using VMware.VSphere.Management.Operations;
using Vim25Api;
using VMware.VSphere.Management.Common;
using VMware.Security.CredentialStore;

using InMage.APIHelpers;

namespace VMwareClient
{
    public static class Extensions
    {
        /// <summary>
        /// Checks if the thrown exception is retryable.
        /// </summary>
        /// <param name="ex">Exception that was thrown.</param>
        /// <returns>True if exception is retryable, false otherwise.</returns>
        public static bool IsRetryableException(this Exception ex)
        {
            if (ex is VMwarePushInstallException)
            {
                return false;
            }

            if (ex is VimException)
            {
                VimException ve = ex as VimException;
                if (ve.MethodFault is InvalidLogin ||
                    ve.MethodFault is NoPermission ||
                    ve.MethodFault is RestrictedVersion ||
                    ve.MethodFault is InvalidGuestLogin ||
                    ve.MethodFault is FileNotFound ||
                    ve.MethodFault is GuestComponentsOutOfDate ||
                    ve.MethodFault is GuestOperationsUnavailable ||
                    ve.MethodFault is GuestPermissionDenied ||
                    ve.MethodFault is InvalidPowerState ||
                    ve.MethodFault is InvalidState ||
                    ve.MethodFault is OperationDisabledByGuest ||
                    ve.MethodFault is OperationNotSupportedByGuest)
                {
                    return false;
                }
            }

            if (ex is VMwareClientException)
            {
                VMwareClientException clientEx = ex as VMwareClientException;
                if (clientEx.ErrorCode == VMwareClientErrorCode.VcenterConnectionFailedInvalidCredentials ||
                    clientEx.ErrorCode == VMwareClientErrorCode.VcenterConnectionFailedIpNotReachable)
                {
                    return false;
                }
            }

            if (ex is System.ServiceModel.FaultException)
            {
                if (ex.ToString().Contains("Failed to authenticate with the guest operating system using the supplied credentials"))
                {
                    return false;
                }
            }

            return true;
        }

        public static string Concat(this string[] stringArray)
        {
            string concatenatedString = String.Empty;
            foreach (string s in stringArray)
            {
                concatenatedString = String.Concat(concatenatedString, s);
                concatenatedString += ";";
            }
            return concatenatedString;
        }

        public static string Concat(this List<string> stringList)
        {
            string concatenatedString = String.Empty;
            foreach (string s in stringList)
            {
                concatenatedString = String.Concat(concatenatedString, s);
                concatenatedString += ";";
            }
            return concatenatedString;
        }

        public static bool IsSocketException(this Exception ex)
        {
            if (ex is System.Net.WebException)
            {
                if (ex.ToString().Contains("System.Net.Sockets.SocketException"))
                {
                    return true;
                }
            }
            return false;
        }

        public static bool IsInternalServerError(this Exception ex)
        {
            if (ex is System.Net.WebException)
            {
                if (ex.ToString().Contains("Internal Server Error"))
                {
                    return true;
                }
            }
            return false;
        }

        public static string GetUsername(this LogonAccount account)
        {
            string domain = String.Empty;
            string username = String.Empty;

            account.Domain = account.Domain.Trim();
            account.Username = account.Username.Trim();

            domain = account.Domain;

            if (account.Domain == String.Empty ||
                account.Domain == "." ||
                account.Domain == "\\" ||
                account.Domain == ".\\")
            {
                domain = String.Empty;
            }

            username = account.Username;

            int index = account.Username.IndexOf("\\");
            if (index != -1)
            {
                domain = account.Username.Substring(0, index);
                username = account.Username.Substring(index + 1);
            }

            index = account.Username.IndexOf("@");
            if (index != -1)
            {
                username = account.Username.Substring(0, index);
                domain = account.Username.Substring(index + 1);
            }

            if (domain == String.Empty)
            {
                return username;
            }
            else
            {
                return domain + "\\" + username;
            }
        }

        public static string ToNormalString(this SecureString value)
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

        public static LogonAccount ToLogonAccount(this Credential credential)
        {
            LogonAccount account = new LogonAccount
            {
                AccountId = credential.Id,
                Domain = string.Empty,
                Username = credential.UserName,
                Password = credential.Password.ToNormalString()
            };

            return account;
        }

        public static LogonAccount ToLogonAccount(this UserAccount userAccount)
        {
            LogonAccount account = new LogonAccount
            {
                AccountId = userAccount.AccountId,
                Domain = userAccount.Domain,
                Username = userAccount.UserName,
                Password = userAccount.Password
            };

            return account;
        }
    }
}
