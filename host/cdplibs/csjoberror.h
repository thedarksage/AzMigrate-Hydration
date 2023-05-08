//---------------------------------------------------------------
//  <copyright file="csjoberror.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines error codes used for reporting job failures to CS.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------

#ifndef CSJOBERROR_H
#define CSJOBERROR_H

const std::string CsJobInputParsingFailed = "MasterTarget-CsJobInputParsingFailed";
const std::string CsJobBadInput = "MasterTarget-CsJobBadInput";
const std::string CsJobFileCopyInternalError = "MasterTarget-CsJobFileCopy-InternalError";
const std::string CsJobFileMissingInternalError = "MasterTarget-CsJobFileMissing-InternalError";
const std::string CsJobInsufficientPrivilege = "MasterTarget-CsJobInsufficientPrivilege";
const std::string CsJobRegKeyCreateFailed = "MasterTarget-CsJobRegKeyCreateFailed-InternalError";
const std::string CsJobRegKeyImportFailed = "MasterTarget-CsJobRegKeyImportFailed-InternalError";
const std::string CsJobCertificateImportFailed = "MasterTarget-CsJobCertificateImportFailed-InternalError";
const std::string CsJobUnhandledException = "MasterTarget-CsJobUnhandledException";

const std::string CsJobBadInputMessage = "The operation failed due to an internal error (bad input).";
const std::string CsJobInsufficientPrivilegeMsg = "The operation failed due to insufficient privileges.";

const std::string CsJobErrorDefaultMessage = "The operation failed due to an internal error.";
const std::string CsJobErrorDefaultPossibleCauses = "The operation failed due to an internal error.";
const std::string CsJobErrorDefaultRecommendedAction = "Retry the operation. If the issue persists, contact support.";

#endif
