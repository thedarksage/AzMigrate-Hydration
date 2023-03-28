#ifndef _SECURITY_H
#define _SECURITY_H

#include <string>
#include <Wincrypt.h>
#include <atlconv.h>
#include <conio.h>
#include <iostream>
#include <vector>

#define OPTION_ENCRYPT				"encrypt"
#define OPTION_ENCRYPT_DB_CREDENTIALS	"db"
//#define OPTION_DOMAIN_NAME			"domain"
//#define OPTION_DOMAIN_USER_NAME     "duser"
////#define OPTION_DECRYPT			"decrypt"

#define INM_MAX_PASSWD_LEN		256

enum emSecurityOperation
{
	OPERATION_ENCRYPT = 0,
	OPERATION_DECRYPT
};

enum emAlgorithmId
{
	RC4 = 1,	
	AES256
};

template <typename T>
std::string ToString(const T &val)
{
	std::ostringstream outStr;
	outStr << val;
	return outStr.str();
}
int SecurityMain(int argc, char* argv[]);
void Security_usage();



BOOL EncryptCredentials(const std::string &domainName, const std::string &userName, const std::string &szPlainPassword, std::string &encryptionKey, std::string &encryptedPasswordStr, bool bSaveInRegistry, std::string &errorMsg);
BOOL DecryptCredentials(std::string &domainName, std::string &userName, std::string &decryptedPasswordStr, DWORD &algId, std::string &errorMsg);

BOOL EncryptCredentialsForDb(const DWORD &portNumber, const std::string &userName, const std::string &szPlainPassword, std::string &encryptionKey, std::string &encryptedPasswordStr, bool bSaveInRegistry, std::string &errorMsg);
BOOL DecryptCredentialsForDb(DWORD &portNumber, std::string &userName, std::string &decryptedPasswordStr, DWORD &algId, std::string &errorMsg);

BOOL SaveEncryptedCredentials(HKEY hRegKey, const std::string &szDomainName, const std::string &szUserName, const std::string &encryptedPassword, const DWORD &algId, std::string &errorMsg);
BOOL SaveEncryptedCredentialsForDb(HKEY hRegKey, const DWORD &dwPortNumber, const std::string &szUserName, const std::string &encryptedPassword, const DWORD &algId, std::string &errorMsg);

#endif //_SECURITY_H