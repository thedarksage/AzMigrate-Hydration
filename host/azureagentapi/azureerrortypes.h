///
///  \file azurerrortypes.h
///
///  \brief categorize errors thrown from azure
///
///

#include <Windows.h>
#include <map>
#include <string>

#include "azurespecialexception.h"
#include "azurecanceloperationexception.h"
#include "azureclientresyncrequiredoperationexception.h"
#include "azureinvalidopexception.h"

namespace AzureAgentInterface
{
	// MARS agent would be doing exponential backoff retries for required errors
	// and Dataprotection should be only be having linear interval retryable errors
	//
	// However, for non retryable errors, special action are required from Dataprotection/CS
	// such as pause the pair OR restart the resync
	// Till we have that implemented, we will map all the non retryable errors to
	// error with exponential backoff

	// Retryable Errors [0x80790100 - 0x807901FF]
	// Retryable Errors with Special Action [0x80790200 - 0x807902FF]

	// Non-Retryable Errors [0x80790300 - 0x807903FF], retry after taking remedial steps - action is retry with exponential delay
	// Non-Retryable Errors [0x80790400 - 0x807904FF] Cancel Operation - action to be taken for now is set resync flag
	// Invalid/Unexpected operation performed [0x80790500 - 0x807905FF] -action to be taken is pause the replication
	// Non-Retryable Errors [0x80790600 - 0x807906FF] Client Resync Required Operation - action to be taken for now is delete all the files in current session and set resync flag

	std::map<unsigned long, std::string> mapOfAzureErrorCodeToString =
	{
		{ 0x80790100, "Service communication failed and immediate retry is required (MT_E_SERVICE_COMMUNICATION_ERROR)" },
		{ 0x80790101, "Storage communication failed and immediate retry is required (MT_E_STORAGE_COMMUNICATION_ERROR)" },
		{ 0x80790104, "Remote server comminucation failure, immediate retry is required (MT_E_RPC_DISCONNECTED)" },
		{ 0x80790200, "MARS agent encountered an error while parsing the logs (MT_E_LOG_PARSING_ERROR)" },
		{ 0x80790201, "Restart resync has been requested because MARS agent might have crashed or lost the session metadata (MT_E_RESTART_RESYNC)" },
		{ 0x80790300, "Service authentication failed (MT_E_SERVICE_AUTHENTICATION_ERROR)" },
		{ 0x80790301, "Storage authentication failed (MT_E_STORAGE_AUTHENTICATION_ERROR)" },
		{ 0x80790302, "Storage quota has been exceeded (MT_E_STORAGE_QUOTA_EXCEEDED)" },
		{ 0x80790303, "Subscription has been expired (MT_E_SUBSCRIPTION_EXPIRED)" },
		{ 0x80790304, "Initialization has been Failed (MT_E_INITIALIZATION_FAILED)" },
		{ 0x80790305, "Registration has been failed (MT_E_REGISTRATION_FAILED)" },
		{ 0x80790306, "DOS limit has been reached (MT_E_DOS_LIMIT_REACHED)" },
		{ 0x80790307, "Azure RP requests throttled (MT_E_AZURE_RP_REQUESTS_THROTTLED)" },
		{ 0x80790308, "Generic Error (MT_E_GENERIC_ERROR)" },
		{ 0x80790400, "Resync is required by target (protection service) (MT_E_RESYNC_REQUIRED)" },
		{ 0x80790500, "VM has been already failed over (MT_E_DS_FAILED_OVER)" },
		{ 0x80790501, "VM is not protected (MT_E_DS_NOT_PROTECTED)" },
		{ 0x80790502, "Log or target storage account is deleted (MT_E_STORAGE_NOT_FOUND)" },
		{ 0x80790503, "Replication has been blocked (MT_E_REPLICATION_BLOCKED)" },
		{ 0x80790600, "Resync is required because MARS agent encountered a zero-sized file to upload (MT_E_RESYNC_REQUIRED_ZERO_SIZED_FILE)" },
		{ 0x80790601, "Resync is required because MARS agent found a mismatch between VDL size and file size (MT_E_RESYNC_REQUIRED_VDL_MISMATCH)" },
		{ 0x80790602, "Resync is required because MARS agent found a mismatch in checksum (MT_E_RESYNC_REQUIRED_CHECKSUM_MISMATCH)" },
		{ 0x80790603, "Resync is required because MARS agent found all zeros in the log file header (MT_E_RESYNC_REQUIRED_INVALID_HEADER)" }
	};

	enum AzureErrorCategory { AZURE_RETRYABLE_ERROR, AZURE_RETRYABLE_ERROR_EXPONENTIAL_DELAY, AZURE_NONRETRYABLE_ERROR_CANCELOP, AZURE_NONRETRYABLE_ERROR_INVALIDOP, AZURE_NONRETRYABLE_ERROR_RESYNCREQUIRED, AZURE_UNKOWN_ERROR };

	inline AzureErrorCategory AzureErrorType(HRESULT hr)
	{
		if ((hr >= 0x80790100) && (hr <= 0x807902FF))
			return AZURE_RETRYABLE_ERROR;

		if ((hr >= 0x80790300) && (hr <= 0x807903FF))
			return AZURE_RETRYABLE_ERROR_EXPONENTIAL_DELAY;

		if ((hr >= 0x80790400) && (hr <= 0x807904FF))
			return AZURE_NONRETRYABLE_ERROR_CANCELOP;

		if ((hr >= 0x80790500) && (hr <= 0x807905FF))
			return AZURE_NONRETRYABLE_ERROR_INVALIDOP;

		if ((hr >= 0x80790600) && (hr <= 0x807906FF))
			return AZURE_NONRETRYABLE_ERROR_RESYNCREQUIRED;

		return AZURE_UNKOWN_ERROR;
	}

	inline void ThrowAzureException(const std::string &msg, HRESULT hr)
	{
		AzureErrorCategory errorType = AzureErrorType(hr);

		if (errorType == AZURE_RETRYABLE_ERROR_EXPONENTIAL_DELAY)
			throw AZURE_SPECIAL_EXCEPTION(hr) << msg;

		if (errorType == AZURE_NONRETRYABLE_ERROR_CANCELOP)
			throw AZURE_CANCELOP_EXCEPTION(hr) << msg;

		if (errorType == AZURE_NONRETRYABLE_ERROR_INVALIDOP)
			throw AZURE_INVALIDOP_EXCEPTION(hr) << msg;

		if (errorType == AZURE_NONRETRYABLE_ERROR_RESYNCREQUIRED)
			throw AZURE_CLIENT_RESYNCREQUIREDOP_EXCEPTION(hr) << msg;

		throw ERROR_EXCEPTION << msg << " (" << hr << ")";
	}

	inline std::string AzureErrorCodeToString(HRESULT hr)
	{
		std::map<unsigned long, std::string>::iterator iter;

		iter = mapOfAzureErrorCodeToString.find(hr);
		if (iter != mapOfAzureErrorCodeToString.end())
			return iter->second;
		else
			return "Unknown Error";
	}
}