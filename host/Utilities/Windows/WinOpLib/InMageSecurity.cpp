#include "stdafx.h"
#include "InMageSecurity.h"
#include "inmsafecapis.h"
#include "genpassphrase.h"


#define INMAGE_FAILOVER_KEY		_T("SAM\\SV Systems\\Failover\\")
#define INMAGE_FAILOVER_DB_KEY	_T("SAM\\SV Systems\\Failover\\DB\\")
#define ALGM_ID					_T("Algmid")
#define DOMAIN_NAME				_T("DomainName")
#define DOMAIN_USER_NAME		_T("DomainUserName")
#define PASSWORD_KEY			_T("Password")
#define PORTNUMBER_KEY			_T("PortNumber")


std::string EnterPassword();
int SecurityMain(int argc, char* argv[])
{
	USES_CONVERSION;
	BOOL bEncryptDbLogin		= FALSE;
	int iParsedOptions			= 2;
	DWORD dwPwdLength			= 0;
	int OperationFlag			= -1;
	bool bFlag					= true;
	std::string strDomainName		= "";
	std::string strDomainUserName    = "";
	int portNumber = 0;
	int response;
		
	if(argc == 2)
	{
		Security_usage();
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_ENCRYPT) == 0)
			{
				//break;
			}
			else if(stricmp((argv[iParsedOptions]+1),OPTION_ENCRYPT_DB_CREDENTIALS) == 0)
			{
				bEncryptDbLogin = TRUE;
			}
			else
			{
				printf("Incorrect argument passed\n");
				Security_usage();
				exit(1);
			}
		}
		else
		{
			printf("Incorrect argument passed\n");
			Security_usage();
			exit(1);
		}
	}

	//if(!bEncrypt)
	//{
	//	printf("\n Password value is missing .");
	//	Security_usage();
	//	exit(1);
	//}

	//Encrypt the User Id and Password of DB server
	if(bEncryptDbLogin)
	{
		std::cout<<"Do you want to use the save port number to connect to DB server?(Default is No )[Y/N]:";
		response = getchar();
		if(response == 0x0D)//enter character
		{
			response = 'n';
			portNumber = 0;
		}
		if(response == 'Y' || response == 'y')
		{
			std::cout<<"Enter Port Number:";
			std::cin>>portNumber;
			unsigned short uRepeatCount = 0;
			while((portNumber < 0) && (uRepeatCount < 3))
			{
				std::cout<<std::endl<<"Please enter a valid Port Number.\nEnter Port Number:";
				std::cin>>portNumber;
				uRepeatCount++;
			}
			if(uRepeatCount >= 3)
			{
				std::cout<<std::endl<<"The port number should be a valid(positive) number.!"<<std::endl;
				return -1;
			}
		}
		std::cout<<"Enter DB server UserName:";
		std::cin>>strDomainUserName;
		if(strDomainUserName.empty())
		{
			std::cout<<"\n Empty DB server User Name :";
			std::cout<<"\n exiting...";
			return -1;
		}
	}
	//Encrypt Windows Logon UserId and Password 
	else
	{
		//If Domain name is not specified
		std::cout<<"\nEnter Domain name:";
		std::cin>>strDomainName;
		if(strDomainName.empty())
		{
			std::cout<<"\n Empty Domain Name :";
			std::cout<<"\n exiting...";
			return -1;
		}
		std::cout<<"Enter Domain User:";
		std::cin>>strDomainUserName;
		if(strDomainUserName.empty())
		{
			std::cout<<"\n Empty domain user Name :";
			std::cout<<"\n exiting...";
			return -1;
		}
	}
	std::string strPassword,strConfrmPwd;
	printf("Enter Password:");
	strPassword =  EnterPassword();
	
	if(!strPassword.empty())
	{
		printf("\nReenter Password:");
		strConfrmPwd = EnterPassword();
	}
	else if(strPassword.empty())
	{
		printf("\n Invalid entries in the Password\n");
		return -1;
	}
	if(strPassword == strConfrmPwd)
	{
		DWORD encryptedPasswordLen = 512;
		std::string encryptionKey = "";
        std::string encryptedPassword;
		std::string errorMsg = "";
		if(bEncryptDbLogin)
		{
			bFlag = EncryptCredentialsForDb(portNumber, strDomainUserName, strPassword, encryptionKey, encryptedPassword, true, errorMsg);
		}
		else
		{
			bFlag = EncryptCredentials(strDomainName, strDomainUserName, strPassword, encryptionKey, encryptedPassword, true, errorMsg);
		}
	}
	else
	{
		printf("\n Mismatch in the Passwords");
		bFlag = FALSE;
	}	
	////Clean UP Code
	//if(bDomainName)
	//{
	//	delete[] pDomainName;
	//}
	//if(bDomainUser)
	//{
	//	delete[] pDomainUserName;
	//}
    
	if(bFlag == FALSE)
	{
		//Need to call clean up code i.e say if 2nd call failed,we need to clear data of previous Calls
		//Erase The Written Entires...
		return -1;
	}	
	return 0;
}

void Security_usage()
{
	printf("Usage:\n");
	printf("Winop.exe  SECURITY -encrypt [-db]\n");
}

std::string EnterPassword()
{
    std::string numAsString = "";
    char ch = getch();
    while ((ch != '\r'))
	{
		if(ch == '\b')
		{	
			if(numAsString.length() != 1) //Handling scenaio when first backspace is entered!!
			{
				numAsString.erase(numAsString.length()-1);
			}
			ch = getch();
			continue;
		}
		else if(ch == '\n')
		{
			break;
		}
        numAsString += ch;
        ch = getch();
    }
    return numAsString;
}
static BOOL EncryptPassword(const DWORD &algId, const std::string &szPlainPassword, std::vector<BYTE> &encryptedPassword, std::string &encryptionKeyStr, std::string &errorMsg)
{
	BOOL bResult = TRUE;
    HCRYPTPROV hCryptoServiceProvider = NULL;
    HCRYPTKEY hCrytoKey = NULL;
	HCRYPTHASH hHash = NULL;
	DWORD dwSecretKeyLength;
	LPCSTR UserName= NULL;        // optionally enter the user's name 
		                          //  here to be used as the key 
			                      //  container name (limited to
				                  //  100 characters)

	//Crypto api flag/algid
	DWORD dwProvType;
	DWORD dwFlags;
	DWORD deriveKeydwFlags;
	ALG_ID createHashAlgId;
	ALG_ID deriveKeyAlgId;

	if(AES256 == algId) //currently supported encryption algorithm is AES256
	{
		dwProvType = PROV_RSA_AES;
		dwFlags = CRYPT_VERIFYCONTEXT;
		deriveKeydwFlags = CRYPT_CREATE_SALT;
		createHashAlgId = CALG_SHA_256;
		deriveKeyAlgId = CALG_AES_256;
	}

	//with default flag
	if(CryptAcquireContext(&hCryptoServiceProvider,	NULL, NULL, dwProvType, dwFlags))
	{
		//printf("A new key container has been created.\n");
	}
	else
	{ 
		//If error occurs create a new default key container. 
		if(CryptAcquireContext(&hCryptoServiceProvider, UserName, NULL, dwProvType, CRYPT_NEWKEYSET)) 
		{
			//printf("A new key container has been created.\n");
		}
		else
		{
			errorMsg = "Could not create a new key container. Error = " + ToString(GetLastError());
			return FALSE;
		}
	} 	    
		
	if (CryptCreateHash(hCryptoServiceProvider, createHashAlgId, 0, 0, &hHash))
	{
		securitylib::encryptKey_t encryptKey = securitylib::genEncryptKey();
		if (CryptHashData(hHash, (BYTE *)&encryptKey[0], encryptKey.size(), 0))
		{
			if (CryptDeriveKey(hCryptoServiceProvider, deriveKeyAlgId, hHash, deriveKeydwFlags, &hCrytoKey))
			{
                encryptedPassword.resize(INM_MAX_PASSWD_LEN);
				inm_memcpy_s(&encryptedPassword[0],INM_MAX_PASSWD_LEN, szPlainPassword.c_str(), szPlainPassword.size());
				DWORD dwEncryptedPasswordLength = szPlainPassword.size();
				if (CryptEncrypt(hCrytoKey, 0, TRUE, 0, &encryptedPassword[0], &dwEncryptedPasswordLength, INM_MAX_PASSWD_LEN))
				{
                    encryptedPassword.resize(dwEncryptedPasswordLength);
					encryptionKeyStr.assign(encryptKey.begin(), encryptKey.end());
				}
				else
				{
					bResult = FALSE;
					errorMsg = "Error occured while performing encryption. ErrorCode = " + ToString(GetLastError());
				}
				CryptDestroyKey(hCrytoKey);  // Release provider handle.
			}
			else // Error during CryptDeriveKey
			{				
				bResult = FALSE;
				errorMsg = "Error occured while generating keys. ErrorCode = " + ToString(GetLastError());
			}
		}
		else //Error during CryptHashData
		{
			bResult = FALSE;
			errorMsg = "Error occured while performing hash operation. ErrorCode = " + ToString(GetLastError());
		}
		//delete session key.
		CryptDestroyHash(hHash); 
	}
	else //Error during CryptCreateHash
	{
		bResult = FALSE;
		errorMsg = "Error occured while creating hash. ErrorCode = " + ToString(GetLastError());
	}	
	CryptReleaseContext(hCryptoServiceProvider, 0);
	return bResult;
}
static BOOL DecryptPassword(const DWORD &algId, const std::string &szEncryptedPassword, const std::string &encryptionKeyFileName, std::vector<BYTE> &decryptedPassword, std::string &errorMsg)
{
	BOOL bResult = TRUE;

    HCRYPTPROV hCryptoServiceProvider = NULL;
    HCRYPTKEY hCrytoKey = NULL;
	HCRYPTHASH hHash = NULL;
	DWORD dwLength;
	LPCSTR UserName= NULL;        // optionally enter the user's name 
		                          //  here to be used as the key 
			                      //  container name (limited to
				                  //  100 characters)

	std::string secretKey = "";

	DWORD dwProvType;
	DWORD dwFlags;
	DWORD deriveKeydwFlags;
	ALG_ID createHashAlgId;
	ALG_ID deriveKeyAlgId;
	 
	
	if(algId == AES256)
	{	
		dwProvType = PROV_RSA_AES;
		dwFlags = CRYPT_VERIFYCONTEXT;
		deriveKeydwFlags = CRYPT_CREATE_SALT;
		createHashAlgId = CALG_SHA_256;
		deriveKeyAlgId = CALG_AES_256;			

		if(securitylib::isEncryptionKeyFileExists(encryptionKeyFileName.c_str()))
		{
			securitylib::encryptKey_t encryptKey = securitylib::getEncryptionKey(encryptionKeyFileName.c_str());
			secretKey = std::string(encryptKey.begin(), encryptKey.end());
		}
		else
		{
			bResult = FALSE;
			errorMsg = "Encryption key file not found in default location.\n";
			return bResult;
		}
	}
	else if(algId == RC4)//used only in upgrade case
	{
		dwProvType = PROV_RSA_FULL;
		dwFlags = 0;
		createHashAlgId = CALG_SHA;
		deriveKeyAlgId = CALG_RC4;
		deriveKeydwFlags = CRYPT_EXPORTABLE;
		//This key is used only in case of upgrading UA to decrypt already stored credentials in registry.
		//While upgrading UA, hostconfig will try to read algmid from registry. If the key is not found,
		//the password is decrypted using this hardcoded key to generate plaintext password.
		//Now hostconfig will encrypt this plaintext password using random generated passphrase and AES256
		//and store it back in the registry for further encryption/decryption. After that this key will not be used.
		bResult = FALSE;
		errorMsg = "RC4 encryption is deprecated.\n";
		return bResult;
	}
	//with default flag
	if(CryptAcquireContext(&hCryptoServiceProvider,	NULL, NULL, dwProvType, dwFlags))
	{
		//printf("A new key container has been created.\n");
	}
	else
	{ 
		//If error occurs create a new default key container. 
		if(CryptAcquireContext(&hCryptoServiceProvider, UserName, NULL, dwProvType, CRYPT_NEWKEYSET)) 
		{
			//printf("A new key container has been created.\n");
		}
		else
		{
			errorMsg = "Could not create a new key container.Error = " + ToString(GetLastError());
			return FALSE;
		}
	}
	if (CryptCreateHash(hCryptoServiceProvider, createHashAlgId, 0, 0, &hHash))
	{	
		if (CryptHashData(hHash, (BYTE *)secretKey.c_str(), secretKey.size(), 0))
		{
			if (CryptDeriveKey(hCryptoServiceProvider, deriveKeyAlgId, hHash, deriveKeydwFlags, &hCrytoKey))
			{
                decryptedPassword.resize(szEncryptedPassword.size());
				inm_memcpy_s(&decryptedPassword[0], decryptedPassword.capacity(), szEncryptedPassword.c_str(), szEncryptedPassword.size());
				DWORD decryptedPasswordLen = szEncryptedPassword.size();
				if (!CryptDecrypt(hCrytoKey, 0, TRUE, 0, &decryptedPassword[0], &decryptedPasswordLen))
				{
					bResult = FALSE;
					errorMsg = "Error occured while opening CryptDecrypt. ErrorCode = " + ToString(GetLastError());
				}
				else
				{
					decryptedPassword.resize(decryptedPasswordLen);
				}
				CryptDestroyKey(hCrytoKey);  // Release provider handle.
			}
			else // Error during CryptDeriveKey
			{				
				bResult = FALSE;
				errorMsg = "Error occured while generating keys = " + ToString(GetLastError());
			}
		}
		else //Error during CryptHashData
		{
			bResult = FALSE;
			errorMsg = "Error occured while performing hash operation. ErrorCode = " + ToString(GetLastError());
		}
		//delete session key.
		CryptDestroyHash(hHash); 
	}
	else //Error during CryptCreateHash
	{
		bResult = FALSE;
		errorMsg = "Error occured while creating hash. ErrorCode = " + ToString(GetLastError());
	}	
	CryptReleaseContext(hCryptoServiceProvider, 0);
	return bResult;
}

BOOL EncryptCredentials(const std::string &domainName, const std::string &userName, const std::string &szPlainPassword, std::string &encryptionKey, std::string &encryptedPasswordStr, bool bSaveInRegistry, std::string &errorMsg)
{
	BOOL bResult = TRUE;
	
	//algorithm id
	//1 - RC4
	//2 - AES256
	HKEY hRegKey = NULL;
	LONG resRegOpenKey;
	DWORD algId = AES256;
	DWORD dwDisposn;
	std::vector<BYTE> encryptedPassword;

	resRegOpenKey = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_KEY, 0, KEY_ALL_ACCESS, &hRegKey);

	if(resRegOpenKey == ERROR_SUCCESS)
	{
		DWORD dwType = REG_DWORD;
		DWORD dwSize = sizeof(DWORD);
		resRegOpenKey = ::RegQueryValueEx(hRegKey, ALGM_ID, 0, &dwType, (LPBYTE) &algId, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			algId = AES256;
		}
	}    

	bResult = EncryptPassword(algId, szPlainPassword, encryptedPassword,  encryptionKey, errorMsg);
	if(bResult)
    {
        encryptedPasswordStr.assign(encryptedPassword.begin(), encryptedPassword.end());
        if(bSaveInRegistry)
	    {
		    if(resRegOpenKey == ERROR_FILE_NOT_FOUND)
		    {
			    resRegOpenKey = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, &dwDisposn);
		    }
		    if(resRegOpenKey == ERROR_SUCCESS)
		    {
			    bResult = SaveEncryptedCredentials(hRegKey, domainName, userName, encryptedPasswordStr, algId, errorMsg);
			    if(bResult)
			    {
				    if(!securitylib::writeDomainEncryptionKey(encryptionKey))
				    {
					    bResult = FALSE;
					    errorMsg = "Failed to write encryption key to file\n";
				    }
			    }
		    }
		    else
		    {
			    bResult = FALSE;
			    errorMsg = "Failed to create/open Failover key. ErrorCode = " + ToString(resRegOpenKey);
		    }
        }
	}
	::RegCloseKey(hRegKey);

	return bResult;
}
BOOL DecryptCredentials(std::string &domainName, std::string &userName, std::string &decryptedPasswordStr, DWORD &algId, std::string &errorMsg)
{
	BOOL bResult = TRUE;

	HKEY hRegKey = NULL;
	LONG resRegOpenKey;
	char lszDomainNme[MAX_PATH];
	char lszDomainUserNme[MAX_PATH];
    BYTE lszEncryptedPassword[INM_MAX_PASSWD_LEN];
	DWORD dwEncryptedPasswordLength = INM_MAX_PASSWD_LEN;
    std::vector<BYTE> decryptedPassword;
	DWORD decryptedPasswordLen;

	resRegOpenKey = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_KEY, 0, KEY_QUERY_VALUE, &hRegKey);
	
	if(resRegOpenKey == ERROR_SUCCESS)
	{
		DWORD dwType = REG_DWORD;
		DWORD dwSize = sizeof(DWORD);
		if(::RegQueryValueEx(hRegKey, ALGM_ID, 0, &dwType, (PBYTE)&algId, &dwSize) != ERROR_SUCCESS)
		{
			//if "Algmid" key not found, assume RC4. Possible only in upgrade case
			algId = RC4;
		}
	}
	else
	{
		bResult = FALSE;
		errorMsg = "Failed to read Failover key. ErrorCode = " + ToString(GetLastError());
		return bResult;
	}

	DWORD dwType = REG_SZ;
	DWORD dwSize = MAX_PATH;

	do
	{
		resRegOpenKey = ::RegQueryValueEx(hRegKey, DOMAIN_NAME, NULL, &dwType, (LPBYTE)&lszDomainNme, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read DomainName key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}	
		else
		{
			domainName = std::string(lszDomainNme);
		}
		dwSize = MAX_PATH;
		resRegOpenKey = ::RegQueryValueEx(hRegKey, DOMAIN_USER_NAME, NULL, &dwType, (LPBYTE)&lszDomainUserNme, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read DomainUserName key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}
		else
		{
			userName = std::string(lszDomainUserNme);
		}
		dwType = REG_BINARY;
		resRegOpenKey = ::RegQueryValueEx(hRegKey, PASSWORD_KEY, NULL, &dwType, lszEncryptedPassword, &dwEncryptedPasswordLength);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read Password key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}
	}while(0);
	::RegCloseKey(hRegKey);
	if(resRegOpenKey == ERROR_SUCCESS)
	{
        std::string passwordStr = std::string(&lszEncryptedPassword[0], &lszEncryptedPassword[0] + dwEncryptedPasswordLength);
        std::string filename = securitylib::getPrivateDir();
		filename += securitylib::DOMAINPASSWORD_ENCRYPTION_KEY_FILENAME;	
		if(DecryptPassword(algId, passwordStr, filename, decryptedPassword, errorMsg))
		{			
			decryptedPasswordStr.assign(decryptedPassword.begin(), decryptedPassword.end());
		}
		else
		{
			bResult = FALSE;
		}
	}
	return bResult;
}
BOOL EncryptCredentialsForDb(const DWORD &portNumber, const std::string &userName, const std::string &szPlainPassword, std::string &encryptionKey, std::string &encryptedPasswordStr, bool bSaveInRegistry, std::string &errorMsg)
{
	BOOL bResult = TRUE;

	//algorithm id
	//1 - RC4
	//2 - AES256
	HKEY hRegKey = NULL;
	LONG resRegOpenKey;
	DWORD algId = AES256;
	DWORD dwDisposn;
    std::vector<BYTE> encryptedPassword;

	resRegOpenKey = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_DB_KEY , 0, KEY_ALL_ACCESS, &hRegKey);

	if(resRegOpenKey == ERROR_SUCCESS)
	{
		DWORD dwType = REG_DWORD;
		DWORD dwSize = sizeof(DWORD);		
		resRegOpenKey = ::RegQueryValueEx(hRegKey, ALGM_ID, 0, &dwType, (LPBYTE) &algId, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			algId = AES256;
		}
	}
	bResult = EncryptPassword(algId, szPlainPassword, encryptedPassword, encryptionKey, errorMsg);
	if(bResult)
	{
        encryptedPasswordStr.assign(encryptedPassword.begin(), encryptedPassword.end());
        if(bSaveInRegistry)
	    {
		    if(resRegOpenKey == ERROR_FILE_NOT_FOUND)
		    {
			    resRegOpenKey = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_DB_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, &dwDisposn);
		    }
		    if(resRegOpenKey == ERROR_SUCCESS)
		    {
			    bResult = SaveEncryptedCredentialsForDb(hRegKey, portNumber, userName, encryptedPasswordStr, algId, errorMsg);
			    if(bResult)
			    {
				    std::string fileName = securitylib::getPrivateDir();
				    fileName += securitylib::USERPASSWORD_DB_ENCRYPTION_KEY_FILENAME;
				    if(!securitylib::writeEncryptionKey(encryptionKey, fileName))
				    {
					    bResult = FALSE;
					    errorMsg = "Failed to write encryption key to file";
				    }
			    }
		    }
		    else
		    {
			    bResult = FALSE;
			    errorMsg = "Failed to create/open Failover key. ErrorCode = " + ToString(resRegOpenKey);
		    }
        }
	}
	::RegCloseKey(hRegKey);

	return bResult;
}
BOOL DecryptCredentialsForDb(DWORD &portNumber, std::string &userName, std::string &decryptedPasswordStr, DWORD &algId, std::string &errorMsg)
{
	BOOL bResult = TRUE;

	HKEY hRegKey = NULL;
	LONG resRegOpenKey;
	char lszDomainUserNme[MAX_PATH];
    BYTE lszEncryptedPassword[INM_MAX_PASSWD_LEN];
	DWORD dwEncryptedPasswordLength = INM_MAX_PASSWD_LEN;
    std::vector<BYTE> decryptedPassword;
	DWORD decryptedPasswordLen;
    
	resRegOpenKey = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILOVER_DB_KEY, 0, KEY_QUERY_VALUE, &hRegKey);
	
	if(resRegOpenKey == ERROR_SUCCESS)
	{
		DWORD dwType, dwSize;		
		if(::RegQueryValueEx(hRegKey, ALGM_ID, 0, &dwType, (LPBYTE) &algId, &dwSize) != ERROR_SUCCESS)
		{
			//if "Algmid" key not found, assume RC4. Possible in upgrade case
			algId = RC4;
		}
	}
	else
	{
		bResult = FALSE;
		errorMsg = "Failed to read Failover key. ErrorCode = " + ToString(GetLastError());
		return bResult;
	}

	DWORD dwType = REG_DWORD;
	DWORD dwSize = MAX_PATH;

	do
	{
		resRegOpenKey = ::RegQueryValueEx(hRegKey, PORTNUMBER_KEY, NULL, &dwType, (LPBYTE)&portNumber, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read PortNumber key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}

		dwType = REG_SZ;
		dwSize = MAX_PATH;
		resRegOpenKey = ::RegQueryValueEx(hRegKey, DOMAIN_USER_NAME, NULL, &dwType, (LPBYTE)&lszDomainUserNme, &dwSize);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read DomainUserName key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}
		else
		{
			userName = std::string(lszDomainUserNme);
		}
		dwType = REG_BINARY;
		resRegOpenKey = ::RegQueryValueEx(hRegKey, PASSWORD_KEY, NULL, &dwType, lszEncryptedPassword, &dwEncryptedPasswordLength);
		if(resRegOpenKey != ERROR_SUCCESS)
		{
			bResult = FALSE;
			errorMsg = "Failed to read Password key. ErrorCode = " + ToString(resRegOpenKey);
			break;
		}
	}while(0);
	::RegCloseKey(hRegKey);

	if(resRegOpenKey == ERROR_SUCCESS)
	{
        std::string passwordStr = std::string(&lszEncryptedPassword[0], &lszEncryptedPassword[0] + dwEncryptedPasswordLength);
        std::string filename = securitylib::getPrivateDir();
        filename += securitylib::USERPASSWORD_DB_ENCRYPTION_KEY_FILENAME;	
		if(DecryptPassword(algId, passwordStr, filename, decryptedPassword, errorMsg))
		{	
			decryptedPasswordStr.assign(decryptedPassword.begin(), decryptedPassword.end());
		}
		else
		{
			bResult = FALSE;
		}
	}
	return bResult;
}

BOOL SaveEncryptedCredentials(HKEY hRegKey, const std::string &szDomainName, const std::string &szUserName, const std::string &encryptedPassword, const DWORD &algId, std::string &errorMsg)
{
	// Write data to registry.
	DWORD dwType = REG_SZ;
	LONG regSetVal;
	
	// Add the Domain name.
	regSetVal = ::RegSetValueEx(hRegKey, DOMAIN_NAME, 0, dwType, (LPBYTE) szDomainName.c_str(), szDomainName.size());
	if(regSetVal != ERROR_SUCCESS)
	{		
		errorMsg = "Could not set the DomainName key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	//Add the Domain user name
	regSetVal = ::RegSetValueEx(hRegKey, DOMAIN_USER_NAME, 0, dwType, (LPBYTE) szUserName.c_str(), szUserName.size());
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the DomainUserName key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	//Add the encrypted password
	dwType = REG_BINARY;
    regSetVal = ::RegSetValueEx(hRegKey, PASSWORD_KEY, 0, dwType, (LPBYTE) encryptedPassword.c_str(), encryptedPassword.size());
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the Password key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	// Add the algorithm id
	dwType = REG_DWORD;
	regSetVal = ::RegSetValueEx(hRegKey, ALGM_ID, 0, dwType, (PBYTE)&algId, sizeof(DWORD));
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the Algmid key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}
	return TRUE;
}

BOOL SaveEncryptedCredentialsForDb(HKEY hRegKey, const DWORD &dwPortNumber, const std::string &szUserName, const std::string &encryptedPassword, const DWORD &algId, std::string &errorMsg)
{
	// Write data to registry.
	DWORD dwType = REG_SZ;
	LONG regSetVal;
	
	//Add the Domain user name
	regSetVal = ::RegSetValueEx(hRegKey, DOMAIN_USER_NAME, 0, dwType, (LPBYTE)szUserName.c_str(), szUserName.size());
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the DomainUserName key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	//Add the encrypted password
	dwType = REG_BINARY;
	regSetVal = ::RegSetValueEx(hRegKey, PASSWORD_KEY, 0, dwType, (LPBYTE)encryptedPassword.c_str(), encryptedPassword.size());
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the Password key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	// Add the algorithm id
	dwType = REG_DWORD;
	regSetVal = ::RegSetValueEx(hRegKey, ALGM_ID, 0, dwType, (PBYTE)&algId, sizeof(DWORD));
	if(regSetVal != ERROR_SUCCESS)
	{
		errorMsg = "Could not set the Algmid key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}
	
	// Add the Port number	
	regSetVal = ::RegSetValueEx(hRegKey, PORTNUMBER_KEY, 0, dwType, (PBYTE)&dwPortNumber, sizeof(DWORD));
	if(regSetVal != ERROR_SUCCESS)
	{		
		errorMsg = "Could not set the PortNumber key in the registry. ErrorCode = " + ToString(regSetVal);
		return FALSE;
	}

	return TRUE;
}
/*************************************************************************************************************
_In_		algId --> algorithm id (1 = RC4, 2 = AES256)
_In_Out_	szPassword --> password to be encrypted and upon success will have encrypted password.
					  note: encrypted password size will be larger than plain text password.
_In_Out_	passwordLen --> length of plain password and upon success will have length of encrypted password.
_Out_		encryptionKey --> The hash/session key used for encryption.
*************************************************************************************************************/
