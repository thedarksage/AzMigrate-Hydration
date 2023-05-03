//---------------------------------------------------------------
//  <copyright file="mtprepareforaadmigrationjobhandler.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Define class to handle MTPrepareForAadMigrationJob job.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------

#include <windows.h>
#include <Cryptuiapi.h>
#include <string>

#include "mtprepareforaadmigrationjobhandler.h"
#include "csjoberror.h"
#include "csjoberrorexception.h"
#include "securityutils.h"
#include "createpaths.h"
#include "defaultdirs.h"
#include "unmarshal.h"
#include "converterrortostringminor.h"
#include "getpassphrase.h"
#include "cdputil.h"

#include "serializeinitialsettings.h"

///  <summary>
///  Job specific handler routine.
///  </summary>
/// <param name="job">The job to be processed.</param>
void MTPrepareForAadMigrationJobHandler::ProcessCsJob(CsJob &job)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    try
    {
        MTPrepareForAadMigrationInput input;
        std::string certContent;
        std::string hiveContent;

        std::string registryKey = "SOFTWARE\\Microsoft\\Windows Azure Backup\\Config\\InMageMT";
        std::string certLocation = securitylib::getMTCertPath();
        std::string hiveLocation = securitylib::getMTHivePath();

        // Deserialize the input.
        input = DeSerializeInput(job.InputPayload);

        // Certificate import has been commneted out as this functionality is not required.
        // Bug (1977406) - ACS to AAD: MT should do the migration without certificate import.

        // Extract the container certificate and registry content.
        // certContent = DecodeInput("certificate", input.certData);
        hiveContent = DecodeInput("registry data", input.registryData);

        // Copy certificate and registry contents to local files.
        // CopyContent("certificate data", certContent, certLocation);
        CopyContent("registry data", hiveContent, hiveLocation);

        // Verify copy was successful by readig back the contents.
        // VerifyContent("certificate data", certContent, certLocation);
        VerifyContent("registry data", hiveContent, hiveLocation);

        // Import the certificate.
        // ImportCert(certLocation, securitylib::getPassphrase());

        // Import the registry key.
        ImportHive(hiveLocation, registryKey);

        // update job status to completed and send status to CS.
        job.JobStatus = CsJobStatusCompleted;
        m_Configurator->updateJobStatus(job);
    }
    catch (CsJobErrorException &cse)
    {
        CsJobError jobError;
        cse.ToCsJobError(jobError);
        job.Errors.push_back(jobError);
        job.JobStatus = CsJobStatusFailed;

        std::stringstream jobDescription;
        jobDescription << job << std::endl;
        DebugPrintf(SV_LOG_ERROR, "Job %s failed.\n", jobDescription.str().c_str());

        m_Configurator->updateJobStatus(job);
    }
    catch (std::exception &e)
    {
        CsJobError jobError;
        jobError.ErrorCode = CsJobUnhandledException;
        jobError.Message = CsJobErrorDefaultMessage;
        jobError.PossibleCauses = CsJobErrorDefaultPossibleCauses;
        jobError.RecommendedAction = CsJobErrorDefaultRecommendedAction;

        job.Errors.push_back(jobError);
        job.JobStatus = CsJobStatusFailed;

        std::stringstream jobDescription;
        jobDescription << job << std::endl;
        DebugPrintf(
            SV_LOG_ERROR,
            "Job %s failed with exception %s.\n",
            jobDescription.str().c_str(),
            e.what());

        m_Configurator->updateJobStatus(job);
    }
}

MTPrepareForAadMigrationInput MTPrepareForAadMigrationJobHandler::DeSerializeInput(
    const std::string & serializedInput)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    MTPrepareForAadMigrationInput deserializedValue;
    try
    {
        deserializedValue = unmarshal<MTPrepareForAadMigrationInput>(serializedInput.c_str());
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s hit exception %s.\n", __FUNCTION__, e.what());

        std::map<std::string, std::string> params;
        params["exception"] = e.what();

        throw CsJobErrorException(
            CsJobInputParsingFailed,
            CsJobBadInputMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    DebugPrintf(SV_LOG_DEBUG, "Input deserialized.\n");
    return deserializedValue;
}

std::string MTPrepareForAadMigrationJobHandler::DecodeInput(
    const std::string & contentType,
    const std::string & base64EncodedContent)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);
    std::string decodedContent;
    try
    {
        decodedContent = securitylib::base64Decode(
            base64EncodedContent.c_str(),
            base64EncodedContent.size());
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s hit exception %s.\n", __FUNCTION__, e.what());

        std::map<std::string, std::string> params;
        params["exception"] = e.what();
        params["contenttype"] = contentType;

        throw CsJobErrorException(
            CsJobBadInput,
            CsJobBadInputMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }


    if (decodedContent.empty())
    {

        std::map<std::string, std::string> params;
        params["exception"] = "Recieved empty content from CS.";
        params["contenttype"] = contentType;

        throw CsJobErrorException(
            CsJobBadInput,
            CsJobBadInputMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    DebugPrintf(SV_LOG_DEBUG, "Input %s decoded.\n", contentType.c_str());
    return decodedContent;
}

void MTPrepareForAadMigrationJobHandler::CopyContent(
    const std::string contentType,
    std::string & content,
    const std::string & location)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    securitylib::backup(location);

    CreatePaths::createPathsAsNeeded(location);
    std::string tmpPath = location + ".tmp";

    std::ofstream outFile(tmpPath.c_str(), std::ios::binary | std::ios::trunc);
    if (outFile.good())
    {
        outFile << content;
    }

    if (!outFile.good())
    {
        std::string exception = boost::lexical_cast<std::string>(errno);
        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) while copying %s to location %s.\n",
            __FUNCTION__,
            exception.c_str(),
            contentType.c_str(),
            tmpPath.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = exception;
        params["contenttype"] = contentType;
        params["location"] = tmpPath;

        throw CsJobErrorException(
            CsJobFileCopyInternalError,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    outFile.close();
    boost::filesystem::rename(tmpPath, location);
    DebugPrintf(
        SV_LOG_DEBUG,
        "%s copied to location %s.\n",
        contentType.c_str(),
        location.c_str());
}

void MTPrepareForAadMigrationJobHandler::VerifyContent(
    const std::string contentType,
    const std::string & content,
    const std::string & location)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    std::ifstream inFile(location.c_str(), std::ios::binary);
    if (!inFile.good())
    {
        std::string exception = boost::lexical_cast<std::string>(errno);
        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) while reading contents of %s.\n",
            __FUNCTION__,
            exception.c_str(),
            location);

        std::map<std::string, std::string> params;
        params["exception"] = exception;
        params["contenttype"] = contentType;
        params["location"] = location;

        throw CsJobErrorException(
            CsJobFileMissingInternalError,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    inFile.seekg(0, inFile.end);
    std::streampos length = inFile.tellg();
    inFile.seekg(0, inFile.beg);
    std::vector<char> verifyContent((size_t)length);
    inFile.read(&verifyContent[0], length);
    if (content.length() == length && (0 == memcmp(content.c_str(), &verifyContent[0], length)))
    {
        DebugPrintf(
            SV_LOG_DEBUG,
            "%s at location %s verified.\n",
            contentType.c_str(),
            location.c_str());
    }
    else
    {
        std::string exception = boost::lexical_cast<std::string>(errno);
        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) while copying %s to location %s.\n",
            __FUNCTION__,
            exception.c_str(),
            contentType.c_str(),
            location.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = exception;
        params["contenttype"] = contentType;
        params["location"] = location;

        throw CsJobErrorException(
            CsJobFileCopyInternalError,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }
}

void MTPrepareForAadMigrationJobHandler::ImportHive(
    const std::string & fileLocation,
    const std::string & registryPath)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    bool	rv = true;
    DWORD	d;
    HKEY	hhKey;

    SetPrivilege(SE_RESTORE_NAME, true);
    SetPrivilege(SE_BACKUP_NAME, true);


    d = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        registryPath.c_str(),
        0,
        NULL,
        REG_OPTION_BACKUP_RESTORE,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL,
        &hhKey,
        NULL);

    if (d != ERROR_SUCCESS)
    {
        std::string err;
        convertErrorToString(d, err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) when creating registry key %s.\n",
            __FUNCTION__,
            err.c_str(),
            registryPath.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = err;
        params["location"] = registryPath;

        throw CsJobErrorException(
            CsJobRegKeyCreateFailed,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    ON_BLOCK_EXIT(&RegCloseKey, hhKey);
    d = RegRestoreKey(hhKey, fileLocation.c_str(), REG_FORCE_RESTORE);
    if (d != ERROR_SUCCESS)
    {
        std::string err;
        convertErrorToString(d, err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) when importing registry key %s.\n",
            __FUNCTION__,
            err.c_str(),
            registryPath.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = err;
        params["location"] = registryPath;

        throw CsJobErrorException(
            CsJobRegKeyImportFailed,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    DebugPrintf(SV_LOG_DEBUG, "Registry %s import done.\n", registryPath.c_str());

    // in case of exception, priveleges will not be reset
    // this is ok for now, as process is going to exit on exception
    SetPrivilege(SE_RESTORE_NAME, false);
    SetPrivilege(SE_BACKUP_NAME, false);
}

void MTPrepareForAadMigrationJobHandler::SetPrivilege(LPCSTR privilege, bool set)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);
    bool rv = true;

    TOKEN_PRIVILEGES tp;
    HANDLE hToken;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        std::string err;
        convertErrorToString(GetLastError(), err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) when checking for current process privileges.\n",
            __FUNCTION__,
            err.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = err;
        params["privilege"] = privilege;

        throw CsJobErrorException(
            CsJobInsufficientPrivilege,
            CsJobInsufficientPrivilegeMsg,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }


    ON_BLOCK_EXIT(&CloseHandle, hToken);
    if (!LookupPrivilegeValue(NULL, privilege, &luid))
    {
        std::string err;
        convertErrorToString(GetLastError(), err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) during lookup of process privilege.\n",
            __FUNCTION__,
            err.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = err;
        params["privilege"] = privilege;

        throw CsJobErrorException(
            CsJobInsufficientPrivilege,
            CsJobInsufficientPrivilegeMsg,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;

    if (set)
    {
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    }
    else
    {
        tp.Privileges[0].Attributes = 0;
    }

    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS)
    {
        std::string err;
        convertErrorToString(GetLastError(), err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) when %s %s privilege .\n",
            __FUNCTION__,
            err.c_str(),
            set ? "setting" : "resetting",
            privilege);

        std::map<std::string, std::string> params;
        params["exception"] = err;
        params["privilege"] = privilege;

        throw CsJobErrorException(
            CsJobInsufficientPrivilege,
            CsJobInsufficientPrivilegeMsg,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    DebugPrintf(
        SV_LOG_DEBUG,
        "Process privilege %s for %s.\n",
        set ? "set" : "reset",
        privilege);
}

void MTPrepareForAadMigrationJobHandler::ImportCert(
    const std::string & fileLocation,
    const std::string & passphrase)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    CRYPTUI_WIZ_IMPORT_SRC_INFO importSrc;
    memset(&importSrc, 0, sizeof(CRYPTUI_WIZ_IMPORT_SRC_INFO));
    importSrc.dwSize = sizeof(CRYPTUI_WIZ_IMPORT_SRC_INFO);
    importSrc.dwSubjectChoice = CRYPTUI_WIZ_IMPORT_SUBJECT_FILE;
    std::wstring widePfx(fileLocation.begin(), fileLocation.end());
    importSrc.pwszFileName = widePfx.c_str();
    std::wstring widePassphrase(passphrase.begin(), passphrase.end());
    importSrc.pwszPassword = widePassphrase.c_str();
    importSrc.dwFlags = CRYPT_EXPORTABLE | CRYPT_MACHINE_KEYSET;
    if (0 == CryptUIWizImport(
        CRYPTUI_WIZ_NO_UI | CRYPTUI_WIZ_IMPORT_TO_LOCALMACHINE,
        0,
        0,
        &importSrc,
        0))
    {
        std::string err;
        convertErrorToString(GetLastError(), err);

        DebugPrintf(SV_LOG_ERROR,
            "%s hit error (%s) importing  certificate to local store.\n",
            __FUNCTION__,
            err.c_str());

        std::map<std::string, std::string> params;
        params["exception"] = err;

        throw CsJobErrorException(
            CsJobCertificateImportFailed,
            CsJobErrorDefaultMessage,
            CsJobErrorDefaultPossibleCauses,
            CsJobErrorDefaultRecommendedAction,
            params);
    }

    DebugPrintf(SV_LOG_DEBUG, "Certificate imported to local store.\n");
}