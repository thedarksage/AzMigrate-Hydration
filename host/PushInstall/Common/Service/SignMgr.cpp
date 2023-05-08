#include "SignMgr.h"

// Link with the Wintrust.lib file.
#pragma comment (lib, "wintrust")
#pragma comment (lib, "crypt32")

SignMgr::SignMgr()
{
}


SignMgr::~SignMgr()
{
}
//-------------------------------------------------------------------
//  This example uses the function MyHandleError, a simple error
//  reporting function, to print an error message corresponding 
//  to the error that occured. 
//  For most applications, replace this function with one 
//  that does more extensive error reporting.

void SignMgr::HandleError(TCHAR *s)
{
	DWORD dwErr = GetLastError();
	LPVOID lpMsgBuf;
	
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,                                       // Location of message
		//  definition ignored
		dwErr,                                      // Message identifier for
		//  the requested message    
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Language identifier for
		//  the requested message
		(LPTSTR)&lpMsgBuf,                         // Buffer that receives
		//  the formatted message
		0,                                          // Size of output buffer
		//  not needed as allocate
		//  buffer flag is set
		NULL);                                     // Array of insert values
	DebugPrintf(SV_LOG_DEBUG,"%S\n", lpMsgBuf);
	LocalFree(lpMsgBuf);

}

bool SignMgr::GetCertContext(LPCWSTR szFileName,std::vector<PCCERT_CONTEXT>& vCertContext)
{
	DWORD		dwEncoding = 0;
	DWORD		dwContentType = 0;
	DWORD		dwFormatType = 0;
	HCERTSTORE	hStore = NULL;
	HCRYPTMSG	hMsg = NULL;
	DWORD dwSignerInfo = 0;
	PCMSG_SIGNER_INFO pSignerInfo = NULL;
	CERT_INFO CertInfo;
	PCCERT_CONTEXT pCertContext = NULL;
	bool bGotCertContext = false;

	do
	{
		// Get message handle and store handle from the signed file.
		BOOL fResult = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
										szFileName,
										CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
										CERT_QUERY_FORMAT_FLAG_BINARY,
										0,
										&dwEncoding,
										&dwContentType,
										&dwFormatType,
										&hStore,
										&hMsg,
										NULL);
		if (!fResult)
		{
			DebugPrintf(SV_LOG_ERROR,"CryptQueryObject failed with %x\n", GetLastError());
			break;
		}
		
		//First get the number of signers of this file
		DWORD *pdwNumberOfSigners = NULL;
		DWORD dwNoOfSigners = 0;
		pdwNumberOfSigners = &dwNoOfSigners;
		DWORD dwcbNoOfSigners = 0;
		BOOL bGotNumberOfSigners = CryptMsgGetParam(hMsg,
												CMSG_SIGNER_COUNT_PARAM,
												0,
												NULL,
												&dwcbNoOfSigners);
		if(!bGotNumberOfSigners)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the number of signers.Error = %x",GetLastError());
			CryptMsgClose(hMsg);
			CertCloseStore(hStore,0);
			break;
		}
		else
		{
			bGotNumberOfSigners = CryptMsgGetParam( hMsg,
													CMSG_SIGNER_COUNT_PARAM,
													0,
													(void*)pdwNumberOfSigners,
													&dwcbNoOfSigners);
			if(!bGotNumberOfSigners)
			{
				DebugPrintf(SV_LOG_DEBUG,"Failed to get the number of signers.Error = %s",GetLastError());
				CryptMsgClose(hMsg);
				CertCloseStore(hStore,0);
				break;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Number of signers = %d\n",dwNoOfSigners);
			}
		}
		DWORD dwCount = 0;
		//Get the information for every signer 
		for(dwCount = 0; dwCount< dwNoOfSigners; dwCount++)
		{

			// Get signer information size.
			fResult = CryptMsgGetParam(	hMsg,
										CMSG_SIGNER_INFO_PARAM,
										dwCount,
										NULL,
										&dwSignerInfo);
			if (!fResult)
			{
				DebugPrintf(SV_LOG_DEBUG,"CryptMsgGetParam failed with %x\n", GetLastError());
				continue;
			}

			std::vector<char>vSignerInfoBuffer(dwSignerInfo);
			
			// Get Signer Information.
			fResult = CryptMsgGetParam(	hMsg,
										CMSG_SIGNER_INFO_PARAM,
										dwCount,
										(PVOID)&vSignerInfoBuffer[0]/*pSignerInfo*/,
										&dwSignerInfo);
			if (!fResult)
			{
				DebugPrintf(SV_LOG_DEBUG,"CryptMsgGetParam failed with %x\n", GetLastError());
				continue;
			}
			pSignerInfo = (PCMSG_SIGNER_INFO)&vSignerInfoBuffer[0];
			// Search for the signer certificate in the obtained certificate store hStore 
			CertInfo.Issuer = pSignerInfo->Issuer;
			CertInfo.SerialNumber = pSignerInfo->SerialNumber;

			pCertContext = CertFindCertificateInStore(	hStore,
														X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
														0,
														CERT_FIND_SUBJECT_CERT,
														(PVOID)&CertInfo,
														NULL);
			if (!pCertContext)
			{
				DebugPrintf(SV_LOG_DEBUG,"CertFindCertificateInStore failed with %x\n",GetLastError());
				continue;
			}
			vCertContext.push_back(pCertContext);
			bGotCertContext = true;
		}
		CryptMsgClose(hMsg);
		CertCloseStore(hStore,0);
	}while(false);
	return bGotCertContext;	
}
bool SignMgr::CheckIfRootCAisMS(WINTRUST_DATA * pWinTrustData)
{

	PCCERT_CHAIN_CONTEXT     pChainContext = NULL;
	PCCERT_CONTEXT			 pcCertContext = NULL;
	CERT_CHAIN_POLICY_PARA   ChainPolicy = { 0 };
	CERT_CHAIN_POLICY_STATUS PolicyStatus = { 0 };
	CERT_ENHKEY_USAGE		 EnhKeyUsage;
	CERT_USAGE_MATCH		 CertUsage;
	CERT_CHAIN_PARA          ChainPara = { 0 };
	EnhKeyUsage.cUsageIdentifier = 0;
	EnhKeyUsage.rgpszUsageIdentifier = NULL;

	CertUsage.dwType = USAGE_MATCH_TYPE_AND;
	CertUsage.Usage = EnhKeyUsage;

	ChainPara.cbSize = sizeof(CERT_CHAIN_PARA);
	ChainPara.RequestedUsage = CertUsage;
	DWORD dwFlags = 0;// CERT_CHAIN_TIMESTAMP_TIME | CERT_CHAIN_REVOCATION_CHECK_CHAIN;
	bool bStatus = false;
	std::vector<PCCERT_CONTEXT> vCertContext;

	bool bObtainedCertContxt = GetCertContext(pWinTrustData->pFile->pcwszFilePath,vCertContext);
	if(bObtainedCertContxt)
	{
		std::vector<PCCERT_CONTEXT>::iterator iterCertCntxt = vCertContext.begin();
		for(;(iterCertCntxt != vCertContext.end()) && (!bStatus);iterCertCntxt++)
		{
			//dwFlags = CERT_CHAIN_ENABLE_PEER_TRUST;
			//dwFlags = CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL | CERT_CHAIN_TIMESTAMP_TIME;
			//dwFlags = CERT_CHAIN_DISABLE_PASS1_QUALITY_FILTERING | CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS | CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
			//dwFlags = CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL;
			//dwFlags = CERT_CHAIN_CACHE_END_CERT | CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL;
			//dwFlags = CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL;
			 //dwFlags = CERT_CHAIN_CACHE_END_CERT;
			//dwFlags = CERT_CHAIN_DISABLE_PASS1_QUALITY_FILTERING;
			dwFlags = CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;//WORKING
			//dwFlags = CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL;
			//dwFlags = 0;

			if (CertGetCertificateChain(
				HCCE_LOCAL_MACHINE,                  // use the default chain engine
				(*iterCertCntxt),     // pointer to the end certificate
				NULL,                  // use the default time
				NULL,                  // search no additional stores
				&ChainPara,            // use AND logic and enhanced key usage 
									   //  as indicated in the ChainPara 
									   //  data structure
				dwFlags,
				NULL,                  // currently reserved
				&pChainContext))       // return a pointer to the chain created
			{
				DebugPrintf(SV_LOG_DEBUG,"The chain has been created. \n");
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"The Certificate chain could not be created.Error = %d.Continuing to get Certificate Chain until the last Cert Context...\n",GetLastError());
				continue;
			}
			DebugPrintf(SV_LOG_DEBUG, "\npChainContext->cbSize = %d\tpChainContext->cChain = %d\tpChainContext->dwCreateFlags =%d\n", pChainContext->cbSize, pChainContext->cChain, pChainContext->dwCreateFlags);
			DebugPrintf(SV_LOG_DEBUG, "\npChainContext->TrustStatus.dwErrorStatus = %d\n", pChainContext->TrustStatus.dwErrorStatus);
			DebugPrintf(SV_LOG_DEBUG, "\npChainContext->TrustStatus.dwInfoStatus=%d\n", pChainContext->TrustStatus.dwInfoStatus);
			DebugPrintf(SV_LOG_DEBUG, "\npChainContext->dwRevocationFreshnessTime =%d\n", pChainContext->dwRevocationFreshnessTime);
			DebugPrintf(SV_LOG_DEBUG, "\npChainContext->fHasRevocationFreshnessTime =%d\n", pChainContext->fHasRevocationFreshnessTime);

			//ChainPolicy.dwFlags = MICROSOFT_ROOT_CERT_CHAIN_POLICY_CHECK_APPLICATION_ROOT_FLAG;
			ChainPolicy.cbSize = sizeof(CERT_CHAIN_POLICY_PARA);
			//ChainPolicy.dwFlags = (DWORD)MICROSOFT_ROOT_CERT_CHAIN_POLICY_CHECK_APPLICATION_ROOT_FLAG;// 0;//it can be zero if pszPolicyOID member in CertVerifyCertifcateChainPolicy function is defined as CERT_CHAIN_POLICY_MICROSOFT_ROOT
			//ChainPolicy.dwFlags = (DWORD)(CERT_CHAIN_POLICY_BASE) | CERT_CHAIN_POLICY_IGNORE_CTL_NOT_TIME_VALID_FLAG;
			ChainPolicy.dwFlags = 0;
			ChainPolicy.pvExtraPolicyPara = NULL;

			PolicyStatus.cbSize = sizeof(CERT_CHAIN_POLICY_STATUS);
			//ChainPolicy.cbSize = sizeof(CERT_CHAIN_POLICY_PARA);

			PolicyStatus.cbSize = sizeof(CERT_CHAIN_POLICY_STATUS);

			if (!CertVerifyCertificateChainPolicy(
				CERT_CHAIN_POLICY_MICROSOFT_ROOT, // use the base policy
				pChainContext,          // pointer to the chain    
				&ChainPolicy,
				&PolicyStatus))         // return a pointer to the policy status
			{
				HandleError("CertVerifyCertificateChainPolicy failed.");
				CertFreeCertificateChain(pChainContext);
				continue;
			}
			else
			{
				if (PolicyStatus.dwError != S_OK )
				{
					DebugPrintf(SV_LOG_ERROR,"CertVerifyCertificateChainPolicy: Chain Status Failure:"
						" 0x%x\n", PolicyStatus.dwError);
					CertFreeCertificateChain(pChainContext);
				}
				else
				{
					bStatus = true;
					DebugPrintf(SV_LOG_INFO,"Certificate Chain is verified with Microsoft as Root.\n");
					CertFreeCertificateChain(pChainContext);
					break;
				}
			}
		}
		//Free up the CertContext
		iterCertCntxt = vCertContext.begin();
		for (; (iterCertCntxt != vCertContext.end()); iterCertCntxt++)
		{
			CertFreeCertificateContext(*iterCertCntxt);
		}
	}
	return bStatus;
}

bool SignMgr::VerifyEmbeddedSignature(TCHAR* pszSourceFile)
{
	LONG lStatus;
	DWORD dwLastError;
	// Initialize the WINTRUST_FILE_INFO structure.

	WINTRUST_FILE_INFO FileData;
	memset(&FileData, 0, sizeof(FileData));
	FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
	std::wstring wstrSrcFile = ExtendedLengthPath::name(pszSourceFile);
	FileData.pcwszFilePath = wstrSrcFile.c_str();
	DebugPrintf(SV_LOG_DEBUG, "Verify signature for the file:%S\n", FileData.pcwszFilePath);
	FileData.hFile = NULL;
	FileData.pgKnownSubject = NULL;

	/*
	WVTPolicyGUID specifies the policy to apply on the file
	WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:

	1) The certificate used to sign the file chains up to a root
	certificate located in the trusted root certificate store. This
	implies that the identity of the publisher has been verified by
	a certification authority.

	2) In cases where user interface is displayed (which this example
	does not do), WinVerifyTrust will check for whether the
	end entity certificate is stored in the trusted publisher store,
	implying that the user trusts content from this publisher.

	3) The end entity certificate has sufficient permission to sign
	code, as indicated by the presence of a code signing EKU or no
	EKU.
	*/

	GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_DATA WinTrustData;

	// Initialize the WinVerifyTrust input data structure.

	// Default all fields to 0.
	memset(&WinTrustData, 0, sizeof(WinTrustData));

	WinTrustData.cbStruct = sizeof(WinTrustData);

	// Use default code signing EKU.
	WinTrustData.pPolicyCallbackData = NULL;

	// No data to pass to SIP.
	WinTrustData.pSIPClientData = NULL;

	// Disable WVT UI.
	WinTrustData.dwUIChoice = WTD_UI_NONE;

	// No revocation checking.
	WinTrustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;

	// Verify an embedded signature on a file.
	WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;

	// Verify action.
	WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;

	// Verification sets this value.
	WinTrustData.hWVTStateData = NULL;

	// Not used.
	WinTrustData.pwszURLReference = NULL;

	WinTrustData.dwProvFlags = WTD_REVOKE_WHOLECHAIN |  WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | WTD_CACHE_ONLY_URL_RETRIEVAL;//WTD_CACHE_ONLY_URL_RETRIEVAL;

	// This is not applicable if there is no UI because it changes 
	// the UI to accommodate running applications instead of 
	// installing applications.
	WinTrustData.dwUIContext = 0;

	// Set pFile.
	WinTrustData.pFile = &FileData;
	bool bSigned = false;

	// WinVerifyTrust verifies signatures as specified by the GUID 
	// and Wintrust_Data.
	lStatus = WinVerifyTrust(
		NULL,
		&WVTPolicyGUID,
		&WinTrustData);

	switch (lStatus)
	{
	case ERROR_SUCCESS:
		/*
		Signed file:
		- Hash that represents the subject is trusted.

		- Trusted publisher without any verification errors.

		- UI was disabled in dwUIChoice. No publisher or
		time stamp chain errors.

		- UI was enabled in dwUIChoice and the user clicked
		"Yes" when asked to install and run the signed
		subject.
		*/
		bSigned = true;
		DebugPrintf(SV_LOG_DEBUG,"The file \"%S\" is signed and the signature is verified.\n",
			FileData.pcwszFilePath);
		break;

	case TRUST_E_NOSIGNATURE:
		// The file was not signed or had a signature 
		// that was not valid.

		// Get the reason for no signature.
		dwLastError = GetLastError();
		if (TRUST_E_NOSIGNATURE == dwLastError ||
			TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
			TRUST_E_PROVIDER_UNKNOWN == dwLastError)
		{
			// The file was not signed.
			DebugPrintf(SV_LOG_ERROR,"The file \"%S\" is not signed.\n",
				FileData.pcwszFilePath);
		}
		else
		{
			// The signature was not valid or there was an error 
			// opening the file.
			DebugPrintf(SV_LOG_ERROR,"An unknown error occurred trying to "
				"verify the signature of the \"%S\" file.\n",
				FileData.pcwszFilePath);
		}

		break;

	case TRUST_E_EXPLICIT_DISTRUST:
		// The hash that represents the subject or the publisher 
		// is not allowed by the admin or user.
		DebugPrintf(SV_LOG_ERROR,"The signature is present in the file %S, but specifically "
			"disallowed.\n", FileData.pcwszFilePath);
		break;

	case TRUST_E_SUBJECT_NOT_TRUSTED:
		// The user clicked "No" when asked to install and run.
		DebugPrintf(SV_LOG_ERROR,"The signature is present in the file %S, but not "
			"trusted.\n", FileData.pcwszFilePath);
		break;

	case CRYPT_E_SECURITY_SETTINGS:
		/*
		The hash that represents the subject or the publisher
		was not explicitly trusted by the admin and the
		admin policy has disabled user trust. No signature,
		publisher or time stamp errors.
		*/
		DebugPrintf(SV_LOG_ERROR,"CRYPT_E_SECURITY_SETTINGS - The hash "
			"representing the subject or the publisher wasn't "
			"explicitly trusted by the admin and admin policy "
			"has disabled user trust. No signature, publisher "
			"or timestamp errors in the file %S.\n", FileData.pcwszFilePath);
		break;

	default:
		// The UI was disabled in dwUIChoice or the admin policy 
		// has disabled user trust. lStatus contains the 
		// publisher or time stamp chain error.
		DebugPrintf(SV_LOG_ERROR,"WinVerifyTrust failed for the file %S. Error is: 0x%x.\n",
			FileData.pcwszFilePath,lStatus);
		break;
	}

	// Any hWVTStateData must be released by a call with close.
	WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;

	lStatus = WinVerifyTrust(
		NULL,
		&WVTPolicyGUID,
		&WinTrustData);
	if (!bSigned)
	{
		return false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Checking if the certificate's Root CA is Microsoft or not...\n");

	bSigned = CheckIfRootCAisMS(&WinTrustData);
	if (bSigned)
	{
		DebugPrintf(SV_LOG_DEBUG, "This certificate for the file %S is signed with  a valid certificate and it's Root CA is Microsoft.\n", FileData.pcwszFilePath);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "This file %S is NOT signed by a certificate whose Root CA is Microsoft.\n", FileData.pcwszFilePath);
	}

	return bSigned;
}