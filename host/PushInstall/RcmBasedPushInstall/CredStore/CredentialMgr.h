/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	CredentialMgr.h

Description	:   CredentialMgr class provides APIs to get the credentials from CredStore.

+------------------------------------------------------------------------------------+
*/

#ifndef __CRED_MGR_H__
#define __CRED_MGR_H__

// adding NOMINMAX to avoid expanding min() and max() macros
// defined in windows.h leading to build error
#define NOMINMAX
#undef max
#undef min

#include <string>
#include <boost\shared_ptr.hpp>
#include "json_reader.h"
#include "json_writer.h"
#include "json_adapter.h"

#include "CredStoreUnsupportedVersionException.h"
#include "CredStoreAccountNotFoundException.h"
#include "CredStoreCredsFileNotFoundException.h"
#include "CredStorePassphraseEmptyException.h"
#include "RcmPushConfig.h"

class CredentialMgr;
typedef boost::shared_ptr<CredentialMgr> CredentialMgrptr;

/// \brief Credential information stored in credential store.
class Credential
{
public:

	std::string Id;
	std::string FriendlyName;
	std::string UserName;
	std::string Password;

	/// \brief serializes Credential class
	void serialize(JSON::Adapter& adapter)
	{
		JSON::Class root(adapter, "Credential", false);

		JSON_E(adapter, Id);
		JSON_E(adapter, FriendlyName);
		JSON_E(adapter, UserName);
		JSON_T(adapter, Password);
	}
};

class Credentials
{
public:
	std::vector<Credential> Creds;

	/// \brief serializes Credential class
	void serialize(JSON::Adapter& adapter)
	{
		JSON::Class root(adapter, "Credential", false);

		JSON_T(adapter, Creds);
	}
};

/// \brief Credentials stored in credential store's credentials.json file
/// as serialized string.
class CredentialStoreContent
{
public:
	std::string Version;

	std::vector<Credential> Credentials;

	CredentialStoreContent()
	{
		Version = "";
	}

	/// \brief serializes Credentials class
	void serialize(JSON::Adapter& adapter)
	{
		JSON::Class root(adapter, "CredentialStoreContent", false);

		JSON_E(adapter, Version);
		JSON_T(adapter, Credentials);
	}
};

/// \brief CredStore class that provides access to the credentials stored on-prem.
class CredentialMgr
{
public:
	/// \brief returns the credentialmgr singleton instance
	static CredentialMgrptr Instance();

	Credential GetDecryptedAccount(std::string &accountId);

	std::string GetPassphrase();

private:

	/// \brief instantiates a credential manager class
	CredentialMgr();

	/// \brief gets the credentials from credential store
	CredentialStoreContent GetCredentials();

	/// \brief decrypt the password stored in cred store
	std::string DecryptPassword(std::string &accountId, std::string &encryptedPassword, std::string &credStoreVersion);

	/// \brief true if credential manager is initialized
	static bool isCredMgrInitialized;

	std::vector<std::string> supportedCredentialStoreVersions;
};


#endif