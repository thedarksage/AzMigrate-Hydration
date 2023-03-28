//---------------------------------------------------------------
//  <copyright file="CredentialMgr.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  CredentialMgr class implementation.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------


#include "CredentialMgr.h"
#include "securityutils.h"
#include "defaultdirs.h"
#include "pushconfig.h"
#include "crypto.h"
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <boost/thread/mutex.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include <dpapi.h>

using namespace PI;
using namespace boost::property_tree;

/// \brief singleton credentialmgr instance
static CredentialMgrptr credMgrInstance;

bool CredentialMgr::isCredMgrInitialized = false;

/// \brief mutex to initialize proxy implementation
boost::mutex g_credMgrInitMutex;

static std::string credStorePassphraseFriendlyName = "RcmProxyAgentPassphrase";

CredentialMgr::CredentialMgr()
{
	supportedCredentialStoreVersions.push_back("2.0");
	supportedCredentialStoreVersions.push_back("2.1");
}

CredentialMgrptr CredentialMgr::Instance()
{
	if (!CredentialMgr::isCredMgrInitialized)
	{
		boost::mutex::scoped_lock guard(g_credMgrInitMutex);
		if (!CredentialMgr::isCredMgrInitialized)
		{
			credMgrInstance.reset(new CredentialMgr());
			CredentialMgr::isCredMgrInitialized = true;
		}
	}

	return credMgrInstance;
}

std::string CredentialMgr::GetPassphrase()
{
	Credential passphraseCred = GetDecryptedAccount(credStorePassphraseFriendlyName);

	if (passphraseCred.Password == "")
	{
		throw CredStorePassphraseEmptyException(RcmPushConfig::Instance()->credStoreCredsFilePath());
	}

	return passphraseCred.Password;
}

CredentialStoreContent CredentialMgr::GetCredentials()
{
	std::string version = "";
	ptree pt;
	CredentialStoreContent content;

	if (!boost::filesystem::exists(RcmPushConfig::Instance()->credStoreCredsFilePath()))
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"%s: Credstore credentials file not found at path : %s.\n",
			__FUNCTION__,
			RcmPushConfig::Instance()->credStoreCredsFilePath());

		throw CredStoreCredsFileNotFoundException(RcmPushConfig::Instance()->credStoreCredsFilePath());
	}

	read_json(RcmPushConfig::Instance()->credStoreCredsFilePath(), pt);
	version = pt.get<std::string>("Version");
	
	auto it = std::find(supportedCredentialStoreVersions.begin(), supportedCredentialStoreVersions.end(), version);
	if (it != supportedCredentialStoreVersions.end())
	{
		/*std::ifstream t(RcmPushConfig::Instance()->credStoreCredsFilePath());
		std::stringstream buffer;
		buffer << t.rdbuf();*/

		content.Version = version;

		Credentials creds;

		BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, pt.get_child("Credentials"))
		{
			Credential cred;
			cred.Id = valueType.second.get<std::string>("Id");
			cred.FriendlyName = valueType.second.get<std::string>("FriendlyName");
			cred.UserName = valueType.second.get<std::string>("UserName");
			cred.Password = valueType.second.get<std::string>("Password");
			creds.Creds.push_back(cred);
		}

		content.Credentials.assign(creds.Creds.begin(), creds.Creds.end());

		return content;
	}

	// cred store was implemented using password obfuscation in version 1.0.
	// so if the version here is greater than 1, then algorithm has changed 
	// and the following code is no longer valid

	DebugPrintf(
		SV_LOG_DEBUG,
		"%s: CredStore version unidentified. Version : %s.\n",
		__FUNCTION__,
		version.c_str());
	throw CredStoreUnsupportedVersionException(version, supportedCredentialStoreVersions);
}

std::string CredentialMgr::DecryptPassword(std::string &accountId, std::string &encryptedPassword, std::string &credStoreVersion)
{
	std::string password = "";
	DATA_BLOB entropyBlob;

	if (credStoreVersion == "2.0")
	{
		unsigned char digest[MD5_DIGEST_LENGTH];
		MD5((unsigned char const *)accountId.c_str(), accountId.length(), digest);
		entropyBlob.pbData = (BYTE*)digest;
		entropyBlob.cbData = (DWORD)MD5_DIGEST_LENGTH;
		password = securitylib::systemDecrypt(
			securitylib::base64Decode(
				encryptedPassword.c_str(),
				(int)encryptedPassword.size()),
			&entropyBlob);
	}
	else if (credStoreVersion == "2.1")
	{
		unsigned char digest[SHA256_DIGEST_LENGTH];
		SHA256((unsigned char const *)accountId.c_str(), accountId.length(), digest);
		entropyBlob.pbData = (BYTE*)digest;
		entropyBlob.cbData = (DWORD)SHA256_DIGEST_LENGTH;
		password = securitylib::systemDecrypt(
			securitylib::base64Decode(
				encryptedPassword.c_str(),
				(int)encryptedPassword.size()),
			&entropyBlob);
	}

	return password;
}

Credential CredentialMgr::GetDecryptedAccount(std::string &accountId)
{
	CredentialStoreContent credList = GetCredentials();

	Credential account;
	bool accFound = false;

	for (Credential acc : credList.Credentials)
	{
		if (acc.Id == accountId)
		{
			acc.Password = DecryptPassword(acc.Id, acc.Password, credList.Version);
			account = acc;
			accFound = true;
			break;
		}
	}

	if (!accFound)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"%s: Account not found with id : %s\n",
			__FUNCTION__,
			accountId.c_str());
		throw CredStoreAccountNotFoundException(accountId);
	}

	return account;
}