#include <fstream>
#include <string>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

#include <windows.h>

#include "InMageSecurity.h"
#include "SignMgr.h"
#include "pushclient.h"
#include "converterrortostringminor.h"
#include "errorexceptionmajor.h"
#include "mq.h"
#include "LsaLookup.h"
#include "Ntsecapi.h"

namespace PI {

	void PushClient::persistcredentials(const std::string & domain,
		const std::string & username,
		const std::string & encryptedpassword_path)
	{

		// persist domain user credentials
		std::string encrypted_passwd;
		std::ifstream pwdFile(encryptedpassword_path);
		if (pwdFile.good()) {
			pwdFile >> encrypted_passwd;
		}
		else {
			throw ERROR_EXCEPTION << "internal error, unable to read password";
		}

		std::string errmsg;
		HKEY hRegKey = NULL;
		DWORD dwDisposn;
		LONG resRegCreateKey;
		const char* INMAGE_FAILOVER_KEY = _T("SAM\\SV Systems\\Failover\\");
		resRegCreateKey = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, &dwDisposn);
		if (resRegCreateKey != ERROR_SUCCESS)
			throw ERROR_EXCEPTION << "internal error, failed to create/open registry entry. error = " << resRegCreateKey;

		std::string strUserName = username;
		if (!strUserName.empty()) {

			if (strUserName.find_first_of("@") != std::string::npos) {
				strUserName = username.substr(0, username.find_first_of("@"));
			}
			else if (strUserName.find_first_of("\\") != std::string::npos) {
				strUserName = username.substr(username.find_first_of("\\") + 1);
			}
		}

		std::string strDomainName = domain;
		if (strcmpi(domain.c_str(), "workgroup") == 0)
		{
			char localHostName[512];
			if (gethostname(localHostName, sizeof(localHostName)) == 0){
				strDomainName = std::string(localHostName);
			}
			else{
				strDomainName = ".";
			}
		}	

		if (FALSE == SaveEncryptedCredentials(hRegKey, strDomainName, strUserName, encrypted_passwd, AES256, errmsg))
			throw ERROR_EXCEPTION << "internal error, failed to persist credentials to registry. error: " << errmsg;
		

		// todo: handle exception scenario for closing handles.
		RegCloseKey(hRegKey);

		std::string plainPassword;
		DWORD algId;
		if (TRUE == DecryptCredentials(strDomainName, strUserName, plainPassword, algId, errmsg)){
			if (GrantLogOnRightsToUser(strDomainName, strUserName)){
				HANDLE agentLogonUserToken = NULL;
				if (VerifyScoutAgentServiceUserLogon(strUserName, strDomainName, plainPassword, agentLogonUserToken))
				{
					RevertToSelf();
					if (agentLogonUserToken != NULL)
						CloseHandle(agentLogonUserToken);
				}
			}			
		}
	}

	void PushClient::verifysw(const std::string & installer)
	{
		SignMgr verifier;
		if (!verifier.VerifyEmbeddedSignature((TCHAR*)installer.c_str())) {
			throw ERROR_EXCEPTION << "signature check for " << installer << "failed";
		}
	}

	void InitializeInmLsaString(PLSA_UNICODE_STRING inmLsaUnicodeString, LPWSTR inmLsaString)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		DWORD inmLsaStringLength;
		if (inmLsaString == NULL)
		{
			inmLsaUnicodeString->Buffer = NULL;
			inmLsaUnicodeString->Length = 0;
			inmLsaUnicodeString->MaximumLength = 0;
			return;
		}
		inmLsaStringLength = wcslen(inmLsaString);
		inmLsaUnicodeString->Buffer = inmLsaString;
		inmLsaUnicodeString->Length = (USHORT)inmLsaStringLength * sizeof(WCHAR);
		inmLsaUnicodeString->MaximumLength = (USHORT)(inmLsaStringLength + 1) * sizeof(WCHAR);
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	}

	HRESULT GetAgentUserAccountSid(LPCWSTR inmUserAccountName, PSID * inmUserAccountSid)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		std::string errStr;
		HRESULT hr = MQ_OK;
		
		// Validate the input parameters.
		if (inmUserAccountName == NULL || inmUserAccountSid == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "The user account name is empty.\n");
			return MQ_ERROR_INVALID_PARAMETER;
		}

		// Create buffers that may be large enough.
		// If a buffer is too small, the count parameter will be set to the size needed.
		const DWORD INM_INITIAL_SIZE = 32;
		DWORD inmSidCountBytes = 0;

		DWORD inmDwSidBufferSize = INM_INITIAL_SIZE;
		DWORD inmUserDomainName = 0;

		DWORD inmDomainNameBufSize = INM_INITIAL_SIZE;
		WCHAR *inmDomainNameW = NULL;

		SID_NAME_USE inmSidType;
		DWORD inmDwErrorCode = 0;


		// Create buffers for the SID and the domain name.
		*inmUserAccountSid = (PSID) new BYTE[inmDwSidBufferSize];
		if (*inmUserAccountSid == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "Insufficient resource to allocate memory for SID.\n");
			return MQ_ERROR_INSUFFICIENT_RESOURCES;
		}
		memset(*inmUserAccountSid, 0, inmDwSidBufferSize);
		inmDomainNameW = new WCHAR[inmDomainNameBufSize];
		if (inmDomainNameW == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "Insufficient resource to allocate memory for domain name.\n");
			return MQ_ERROR_INSUFFICIENT_RESOURCES;
		}
		memset(inmDomainNameW, 0, inmDomainNameBufSize*sizeof(WCHAR));

		// Obtain the SID for the account name passed.
		for (;;)
		{
			// Set the count variables to the buffer sizes and retrieve the SID.
			inmSidCountBytes = inmDwSidBufferSize;
			inmUserDomainName = inmDomainNameBufSize;
			if (LookupAccountNameW(NULL, inmUserAccountName, *inmUserAccountSid, &inmSidCountBytes, inmDomainNameW, &inmUserDomainName, &inmSidType))
			{
				if (IsValidSid(*inmUserAccountSid) == FALSE)
				{
					DebugPrintf(SV_LOG_ERROR, "The security identifier is not valid.\n");
				}
				break;
			}
			inmDwErrorCode = GetLastError();

			// Check if one of the buffers was too small.
			if (inmDwErrorCode == ERROR_INSUFFICIENT_BUFFER)
			{
				if (inmSidCountBytes > inmDwSidBufferSize)
				{
					// Reallocate memory for the SID buffer.
					//wprintf(L"The SID buffer was too small. It will be reallocated.\n");
					FreeSid(*inmUserAccountSid);
					*inmUserAccountSid = (PSID) new BYTE[inmSidCountBytes];
					if (*inmUserAccountSid == NULL)
					{
						DebugPrintf(SV_LOG_ERROR, "Insufficient resource to reallocate memory for SID.\n");
						return MQ_ERROR_INSUFFICIENT_RESOURCES;
					}
					memset(*inmUserAccountSid, 0, inmSidCountBytes);
					inmDwSidBufferSize = inmSidCountBytes;
				}
				if (inmUserDomainName > inmDomainNameBufSize)
				{
					// Reallocate memory for the domain name buffer.
					// wprintf(L"The domain name buffer was too small. It will be reallocated.\n");
					delete[] inmDomainNameW;
					inmDomainNameW = new WCHAR[inmUserDomainName];
					if (inmDomainNameW == NULL)
					{
						DebugPrintf(SV_LOG_ERROR, "Insufficient resource to reallocate memory for domain name.\n");
						return MQ_ERROR_INSUFFICIENT_RESOURCES;
					}
					memset(inmDomainNameW, 0, inmUserDomainName*sizeof(WCHAR));
					inmDomainNameBufSize = inmUserDomainName;
				}
			}
			else
			{
				DWORD dwErr = GetLastError();
				DebugPrintf(SV_LOG_ERROR, "Unable to get the security identifier for given user account. ErrorCode = %lu\n", dwErr);
				std::string err;
				convertErrorToString(dwErr, err);
				DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", err.c_str());
				hr = HRESULT_FROM_WIN32(inmDwErrorCode);
				break;
			}
		}
		if (inmDomainNameW)
			delete[] inmDomainNameW;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return hr;
	}

	bool PushClient::VerifyScoutAgentServiceUserLogon(const std::string& userName, const std::string& domain, const std::string& password, HANDLE &agentLogonUserToken)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		bool bLoggedOn = false;
		std::string errStr;

		if (userName.empty() == false && domain.empty() == false && password.empty() == false)
		{
			//Logon user provided in host config ui
			if (LogonUser(userName.c_str(), domain.c_str(), password.c_str(), LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT, &agentLogonUserToken) == TRUE)
			{
				//Check for a user token using the user name
				if (ImpersonateLoggedOnUser(agentLogonUserToken))
				{
					bLoggedOn = true;
				}
				else
				{
					DWORD dwErr = GetLastError();
					DebugPrintf(SV_LOG_ERROR, "Error occured during Impersonate LoggedOnUser. ErrorCode = %lu\n", dwErr);
					std::string err;
					convertErrorToString(dwErr, err);
					DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", err.c_str());
				}
			}
			else
			{
				DWORD dwErr = GetLastError();
				DebugPrintf(SV_LOG_ERROR, "Error occured during LogOnUser. ErrorCode = %lu\n", dwErr);
				std::string err;
				convertErrorToString(dwErr, err);
				DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", err.c_str());
			}
		}
		else
		{
			agentLogonUserToken = NULL;
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return bLoggedOn;
	}

	bool PushClient::GrantLogOnRightsToUser(const std::string &strDomainName, const std::string &strUserName)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		std::string errStr;
		USES_CONVERSION;
		HRESULT hr = MQ_ERROR;
		static SECURITY_QUALITY_OF_SERVICE inmSecurityQos = { sizeof SECURITY_QUALITY_OF_SERVICE, SecurityImpersonation, SECURITY_DYNAMIC_TRACKING, FALSE };
		static LSA_OBJECT_ATTRIBUTES inmLsaObjAtt = { sizeof LSA_OBJECT_ATTRIBUTES, NULL, NULL, 0, NULL, &inmSecurityQos };
		NTSTATUS inmNtStat;
		LSA_HANDLE inmLsaHandle;

		//Open Security policy
		if (inmNtStat = LsaOpenPolicy(NULL, &inmLsaObjAtt, GENERIC_READ | GENERIC_EXECUTE | POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, &inmLsaHandle) != ERROR_SUCCESS)
		{
			DWORD dwErr = GetLastError();
			DebugPrintf(SV_LOG_ERROR, "Unable to open policy object. ErrorCode = %lu\n", dwErr);
			std::string err;
			convertErrorToString(dwErr, err);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", err.c_str());
			return false;
		}
		PSID inmUserAccountSid;
		std::wstring inmAgentSvcAccName;
		std::string inmUserAccName = strDomainName + "\\" + strUserName;
		inmAgentSvcAccName = A2W(inmUserAccName.c_str());

		//Get the  SID corresponding to account Name
		hr = GetAgentUserAccountSid(inmAgentSvcAccName.c_str(), &inmUserAccountSid);
		if (MQ_OK != hr)
		{
			if (inmLsaHandle)
				LsaClose(inmLsaHandle);
			// Free memory allocated for SID.
			if (inmUserAccountSid != NULL)
				delete inmUserAccountSid;
			return false;
		}

		LPWSTR inmAgentSvcPriv = L"SeServiceLogonRight";        // privilege to grant (Unicode)
		LSA_UNICODE_STRING inmAgentSvcPrivStr;

		//Create a LSA_UNICODE_STRING for the privilege name.
		InitializeInmLsaString(&inmAgentSvcPrivStr, inmAgentSvcPriv);

		// grant the privilege
		if ((inmNtStat = LsaAddAccountRights(inmLsaHandle, inmUserAccountSid, &inmAgentSvcPrivStr, 1)) != ERROR_SUCCESS)
		{
			DWORD dwErr = GetLastError();
			DebugPrintf(SV_LOG_ERROR, "Unable to assign privileges to user account. ErrorCode = %lu", dwErr);
			std::string err;
			convertErrorToString(dwErr, err);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", err.c_str());
		}

		// Close the policy handle and Free memory allocated for SID.
		LsaClose(inmLsaHandle);
		if (inmUserAccountSid != NULL)
		{
			delete inmUserAccountSid;
			inmUserAccountSid = NULL;
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return true;
	}	
}