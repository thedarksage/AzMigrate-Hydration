//---------------------------------------------------------------
//  <copyright file="VMwarePushInstallException.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Push Install Exception class for push installation via VMware Vim SDK.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VMwareClient
{
    enum VMwarePushInstallErrorCode
    {
        // Parsing and input validation errors
        ReadingCommandArgumentsFailed,
        OperationTypeNotFoundInInputs,
        OperationTypeInvalid,
        ValidatingOperationInputsFailed,

        // CS account errors
        GetAccountsFromCSFailed,
        GetVcenterAccountFromCSFailed,
        GetVmAccountFromCSFailed,

        // Operation errors (common bucket)
        ConnectToVCenterFailed,
        FindVmInVCenterFailed,
        CopyFileToVmFailed,
        InvokeScriptOnVmFailed,
        FetchFileFromVmFailed,

        // VMware errors
        // VCenter issues
        VCenterInvalidLoginCreds,
        VCenterCredsNoPermission,
        VCenterIpNotReachable,
        UnableToConnectTovCenter,
        VCenterLicenseRestrictedVersion,
        // VM issues
        VmInvalidGuestLoginCreds,
        VmGuestCredsNoPermission,
        VmGuestComponentsOutOfDate,
        VmGuestComponentsNotRunning,
        VmPoweredOff,
        VmInvalidState,
        VmOperationDisabledByGuest,
        VmOperationUnsupportedOnGuest,
        VmBusy,
        VmFileCannotBeAccessed,

        // Miscellaneous
        VmNotFoundInVCenter,
        MultipleVmsFoundInVCenter,
        ScriptInvocationTimedOutOnVm,
        FileToBeFetchedNotAvailableInVm,
        ScriptInvocationInvalidExitCode,

        // Credential Store issues
        CredentialStoreCredsFilePathEmpty,
        CredentialStoreCredsFileNotFound,
        CredentialStoreVersionMismatch,

        // Bios id mismatch
        VmBiosIdMismatch
    }

    class VMwarePushInstallException : Exception
    {
        public string componentErrorCode;

        public VMwarePushInstallErrorCode errorCode;

        public string errorMessage;

        public VMwarePushInstallException(
            string compErrorCode,
            VMwarePushInstallErrorCode errorCodeName,
            string message)
            : base (message)
        {
            componentErrorCode = compErrorCode;
            errorCode = errorCodeName;
            errorMessage = message;
        }

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("ComponentErrorCode: " + componentErrorCode);
            sb.AppendLine("ErrorCode: " + errorCode.ToString());
            sb.AppendLine("ErrorMessage: " + errorMessage);

            return sb.ToString();
        }
    }
}
