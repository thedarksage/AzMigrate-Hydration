#include <windows.h>
#include <Wincrypt.h>
#include <tchar.h>
#include <atlconv.h>
#include <strsafe.h>
#include "WinIoCtl.h"
#include <vector>
#include <iostream>
#include <string>
#include <iostream>
#include "portablehelpersmajor.h"
#include "virtualvolume.h"
#include "../drivers/invirvol/win32/VVDevControl.h"
#include "VsnapUser.h"
#include "file.h"
#include "../inmagesdkutil.h"
#include <boost/lexical_cast.hpp>
#include "../inmsdkutil.h"
#include "../inmsdkglobals.h"
#include "inmageex.h"
#include "InMageSecurity.h"
#include "../../disklayoutmgr/dlwindows.h"
#include "inmsafecapis.h"

TCHAR InmszLocalPassword[] = _T("InMage@01&#2UIQdnpKl");
#define INM_MAX_PASSWD_LEN		256
#define InmINMAGE_FAILOVER_KEY _T("Software\\SV Systems\\Failover\\")
#define InmINMAGE_FAILOVER_DB_KEY _T("Software\\SV Systems\\Failover\\DB\\")
BOOL InmReadCredentials(std::string &strDomainName,std::string &strUserName,std::string &strPassword, BOOL bForDb = FALSE,DWORD *pdwPortNumber = NULL) ;
BOOL InmReadPasswordFromRegistry(TCHAR *szPassword, DWORD &dwPwdLength, BOOL bForDb)
{
	BOOL bResult = TRUE;

	TCHAR szKey[256] = {0};
	HKEY hRegKey = NULL;
	DWORD dwUserNameLen = 100;    // length of the buffer
	LPCSTR UserName= NULL;        // optionally enter the user's name 
		                          //  here to be used as the key 

	LONG resRegOpenKey;
	if(bForDb)
	{	
		resRegOpenKey = RegOpenKeyEx(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_DB_KEY , 0, KEY_QUERY_VALUE, &hRegKey);
	}
	else
	{
		resRegOpenKey = RegOpenKeyEx(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_KEY , 0, KEY_QUERY_VALUE, &hRegKey);
	}

	//if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, INMAGE_FAILVOER_KEY , 0, KEY_QUERY_VALUE, &hRegKey) == ERROR_SUCCESS)
	if(resRegOpenKey == ERROR_SUCCESS)
	{
		HCRYPTPROV hProv = NULL;
		HCRYPTKEY hKey = NULL;
		HCRYPTKEY hXchgKey = NULL;
		HCRYPTHASH hHash = NULL;
		DWORD dwLength = 0;

		if(CryptAcquireContext(
			&hProv,               // handle to the CSP
			UserName,                  // container name 
			NULL,                      // use the default provider
			PROV_RSA_FULL,             // provider type
			0))                        // flag values
		{
			//DebugPrintf("A crypto context with the %s key container has been acquired\n", UserName);
		}
		else
		{ 
		//-------------------------------------------------------------------
		// Some sort of error occurred in acquiring the context. 
		// Create a new default key container. 
				if(CryptAcquireContext(
					&hProv, 
					UserName, 
					NULL, 
					PROV_RSA_FULL, 
					CRYPT_NEWKEYSET)) 
				{
					DebugPrintf("A new key container has been created.\n");
				}
				else
				{
					DebugPrintf("\nCould not create a new key container.Error = %d\n",GetLastError());
					return FALSE;
				}
		} // end else
		// Create hash object.
		if (CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash))
		{
			// Hash password string.
			dwLength = (DWORD)sizeof(TCHAR)*_tcslen(InmszLocalPassword);
			if (CryptHashData(hHash, (BYTE *)InmszLocalPassword, dwLength, 0))
			{
				// Create block cipher session key based on hash of the password.
				if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
				{
					// the password is less than INM_MAX_PASSWD_LEN characters
					dwLength = INM_MAX_PASSWD_LEN*sizeof(TCHAR);
					DWORD dwType = REG_BINARY;
					if (RegQueryValueEx(hRegKey, _T("Password"), NULL, &dwType, (BYTE*)szPassword, &dwLength)==ERROR_SUCCESS)
					{
						if (!CryptDecrypt(hKey, 0, TRUE, 0, (BYTE *)szPassword, &dwLength))
						{
							bResult = FALSE;
							DebugPrintf("Error occured while opening CryptDecrypt ,[%0xd]", GetLastError());
						}
						else
						{
							dwPwdLength = dwLength;
							//only for debugging purpose
                              /* DebugPrintf("Decrypted Password length: %d\n",dwPwdLength);
							DebugPrintf("Decrypted Password: ");
							for(DWORD i = 0; i <dwLength;i++)
							{
								DebugPrintf("%c",szPassword[i]);
								
							}
							DebugPrintf("\n");*/
						}
					}
					else
					{
						bResult = FALSE;
						DebugPrintf("Error occured while opening CryptDeriveKey ,[%0xd]", GetLastError());
					}
					CryptDestroyKey(hKey);  // Release provider handle.
				}
				else
				{
					// Error during CryptDeriveKey!
					bResult = FALSE;
					DebugPrintf("Error occured while performing decryption ,[%0xd]", GetLastError());
				}
			}
			else
			{
				// Error during CryptHashData!
				bResult = FALSE;
				DebugPrintf("Error occured while performing hashing ,[%0xd]", GetLastError());
			}
			CryptDestroyHash(hHash); // Destroy session key.
		}
		else
		{
			// Error during CryptCreateHash!
			bResult = FALSE;
			DebugPrintf("Error occured while performing decryption ,[%0xd]", GetLastError());
		}
		CryptReleaseContext(hProv, 0);
		::RegCloseKey(hRegKey);
	}
	else
	{
		DebugPrintf("Failed to open Registry key, [%d]", GetLastError());
		bResult = FALSE;
	}
	return bResult;
}
BOOL InmReadCredentials(std::string &strDomainName,std::string &strUserName,std::string &strPassword, BOOL bForDb,DWORD *pdwPortNumber)
{
	BOOL bFlag = TRUE;
	DWORD dwLength = 0;
	TCHAR szDecryptPassword[INM_MAX_PASSWD_LEN] = {0};
	dwLength = INM_MAX_PASSWD_LEN *sizeof(TCHAR);
	DWORD dwDbPortNumber = 0;
	if(!bForDb)
	{
		pdwPortNumber = &dwDbPortNumber;		
	}
	if(FALSE == InmReadPasswordFromRegistry(szDecryptPassword,dwLength,bForDb))
	{
		DebugPrintf("\n Failed to read the Password from registry");
		return FALSE;
	}

	LONG lRet, lEnumRet;
    HKEY hKey;
	char lszDomainNme[MAX_PATH];
	char lszDomainUserNme[MAX_PATH];
	char lszDbUserNme[MAX_PATH];
	DWORD dwType=REG_SZ;
	DWORD dwTypepn = REG_DWORD;
	DWORD dwSize=MAX_PATH;
	DWORD dwLengthPn = sizeof(DWORD);
    int i=0;

	do
	{
		//Fisrt Lets try to open Fx Agent's keys for 32 bit machine
		if(bForDb)
		{
			lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE,InmINMAGE_FAILOVER_DB_KEY, 0L, KEY_READ , &hKey);
		}
		else
		{
			lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE,InmINMAGE_FAILOVER_KEY, 0L, KEY_READ , &hKey);
		}
		
		if(lRet != ERROR_SUCCESS)
		{
			DebugPrintf("\n Failed to open HKLM registry keys.Error = %d",GetLastError());
			bFlag = FALSE;
			break;
		}
		if(bForDb)
		{	
			lEnumRet = ERROR_SUCCESS;
		}
		else
		{
			lEnumRet = RegQueryValueEx(hKey, "DomainName", NULL, &dwType,(LPBYTE)&lszDomainNme, &dwSize);
		}
		
		if(lEnumRet != ERROR_SUCCESS)
		{
		    DebugPrintf("\n Failed to open DomainName registry keys.Error = %d",GetLastError());
			bFlag = FALSE;
			break;
	    }
		lEnumRet = 0;
		dwSize = MAX_PATH;
		if(bForDb)
		{
			lEnumRet = RegQueryValueEx(hKey, "UserName", NULL, &dwType,(LPBYTE)&lszDbUserNme, &dwSize);
			if(lEnumRet != ERROR_SUCCESS)
			{
				DebugPrintf("\n Failed to open User Name Key.");
				bFlag = FALSE;
				return bFlag;
			}
			lEnumRet = RegQueryValueEx(hKey, _T("PortNumber"), NULL, &dwTypepn/*(LPDWORD)&dwTypepn*/ ,(PBYTE)&dwDbPortNumber, &dwLengthPn);
			if(lEnumRet != ERROR_SUCCESS)
			{
				DebugPrintf("\n Failed to open portnumber registry keys using the portNumber.");
				bFlag = FALSE;
				return bFlag;
			}
		}
		else
		{
			lEnumRet = RegQueryValueEx(hKey, "DomainUserName", NULL, &dwType,(LPBYTE)&lszDomainUserNme, &dwSize);
		}
		
		if(lEnumRet != ERROR_SUCCESS)
		{
			DebugPrintf("\n Failed to open DomainUserName registry keys.Error = %d",GetLastError());
			//LPVOID lpMsgBuf;
			//FormatMessage( 
			//		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			//		FORMAT_MESSAGE_FROM_SYSTEM | 
			//		FORMAT_MESSAGE_IGNORE_INSERTS,
			//		NULL,
			//		GetLastError(),
			//		0, // Default language
			//		(LPTSTR) &lpMsgBuf,
			//		0,
			//		NULL);
			// // Process any inserts in lpMsgBuf.
			//// ...
			//// Display the string.
			//MessageBox( NULL, (LPCTSTR)lpMsgBuf, NULL, MB_OK | MB_ICONINFORMATION );
			//// Free the buffer.
			//LocalFree( lpMsgBuf);

		    //DebugPrintf("\n Failed to open DomainUserName registry keys.Error = %d",GetLastError());
			bFlag = FALSE;
			break;
	    }
	}while(0);
		RegCloseKey(hKey);
		if(TRUE == bFlag)
		{
			strPassword				= std::string(szDecryptPassword);
				if(!bForDb)
				{
					strDomainName		= std::string(lszDomainNme);
				}				
				if(!bForDb)
				{
					strUserName			= std::string(lszDomainUserNme);
				}
				else
				{
					strUserName			= std::string(lszDbUserNme);
					*pdwPortNumber        = dwDbPortNumber;
				}				
		}
		return bFlag;
}

BOOL InmSavePasswordToRegistry(TCHAR* szPassword, BOOL bForDb)
{
	BOOL bResult = TRUE;

	if(_tcslen(szPassword) >= INM_MAX_PASSWD_LEN)
	{
		SetLastError(ERROR_PASSWORD_RESTRICTION);
		DebugPrintf("Warning: Passwrod lenght should be less than %d characters\n",INM_MAX_PASSWD_LEN);
		return FALSE;
	}

	TCHAR szKey[256] = {0};
	HKEY hRegKey = NULL;

	LONG resRegCreateKey;
	if(bForDb)
	{
		resRegCreateKey = RegCreateKey(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_DB_KEY, &hRegKey);
	}
	else
	{
		resRegCreateKey = RegCreateKey(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_KEY , &hRegKey);
	}
	//if (RegCreateKey(HKEY_LOCAL_MACHINE, INMAGE_FAILVOER_KEY , &hRegKey) != ERROR_SUCCESS)
	if(resRegCreateKey != ERROR_SUCCESS)
	{
		DebugPrintf("Error occured while performing encryption, [%d]", GetLastError());
		return FALSE;
	}

	HCRYPTPROV hProv = NULL;
	HCRYPTKEY hKey = NULL;
	HCRYPTKEY hXchgKey = NULL;
	HCRYPTHASH hHash = NULL;
	DWORD dwLength;
	//CHAR szUserName[100];         // buffer to hold the name 
                              //  of the key container
	DWORD dwUserNameLen = 100;    // length of the buffer
	LPCSTR UserName= NULL;        // optionally enter the user's name 
		                          //  here to be used as the key 
			                      //  container name (limited to
				                  //  100 characters)

	TCHAR szEncryptedPassword[INM_MAX_PASSWD_LEN] = {0};
	inm_tcscpy_s(szEncryptedPassword,ARRAYSIZE(szEncryptedPassword),szPassword);

	// Get handle to user default provider.
	/********************************************************************************************/
	if(CryptAcquireContext(
			&hProv,               // handle to the CSP
			UserName,                  // container name 
			NULL,                      // use the default provider
			PROV_RSA_FULL,             // provider type
			0))                        // flag values
		{
			//DebugPrintf("\n Successfully persisted the Password in the Registry");
		}
		else
		{ 
		//-------------------------------------------------------------------
		// Some sort of error occurred in acquiring the context. 
		// Create a new default key container. 
				if(CryptAcquireContext(
					&hProv, 
					UserName, 
					NULL, 
					PROV_RSA_FULL, 
					CRYPT_NEWKEYSET)) 
				{
					/*DebugPrintf("A new key container has been created.\n");
					DebugPrintf("\n Successfully persisted the Password in the Registry");*/
				}
				else
				{
					DebugPrintf("\nCould not create a new key container.Error = %d\n",GetLastError());
					return FALSE;
				}
		} // end else
	    /********************************************************************************************/
		// Create hash object.
		if (CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash))
		{
			// Hash password string.
			dwLength = (DWORD)sizeof(TCHAR)*_tcslen(InmszLocalPassword);
			if (CryptHashData(hHash, (BYTE *)InmszLocalPassword, dwLength, 0))
			{
				// Create block cipher session key based on hash of the password.
				if (CryptDeriveKey(hProv, CALG_RC4, hHash, CRYPT_EXPORTABLE, &hKey))
				{
					// Determine number of bytes to encrypt at a time.
					dwLength = (DWORD)sizeof(TCHAR)*_tcslen(szEncryptedPassword) ;

					// Allocate memory.
					BYTE *pbBuffer = (BYTE *)malloc(dwLength);
					if (pbBuffer != NULL)
					{
						inm_memcpy_s(pbBuffer,dwLength, szPassword, dwLength);
						// Encrypt data
						if (CryptEncrypt(hKey, 0, TRUE, 0, pbBuffer, &dwLength, dwLength))
						{
							// Write data to registry.
							DWORD dwType = REG_BINARY;
							// Add the password.
							if (::RegSetValueEx(hRegKey, _T("Password"), 0, REG_BINARY, pbBuffer, dwLength)!=ERROR_SUCCESS)
							{
								bResult = FALSE;
							}
							RegFlushKey( hRegKey );
							::RegCloseKey(hRegKey);
						}
						else
						{
							bResult = FALSE;
							DebugPrintf("Error occured while performing encryption ,[%0xd]", GetLastError());
						}
						// Free memory.
					  free(pbBuffer);
					}
					else
					{
						bResult = FALSE;
						DebugPrintf("Error occured while performing encryption ,[%0xd]", GetLastError());
					}
					CryptDestroyKey(hKey);  // Release provider handle.
				}
				else
				{
					// Error during CryptDeriveKey!
					bResult = FALSE;
					DebugPrintf("Error occured while performing encryption ,[%0xd]", GetLastError());
				}
			}
			else
			{
				// Error during CryptHashData!
				bResult = FALSE;
				DebugPrintf("Error occured while performing hash oepration ,[%0xd]", GetLastError());
			}
			CryptDestroyHash(hHash); // Destroy session key.
		}
		else
		{
			// Error during CryptCreateHash!
			bResult = FALSE;
			DebugPrintf("Error occured while creating hash ,[%0xd]", GetLastError());
		}
		CryptReleaseContext(hProv, 0);
	/*}*/
	 
    /*else
	{
		DebugPrintf("Error occured while calling CRYPTO API,[%0xd]", GetLastError());
	}*/
	return bResult;
}




BOOL InmSaveUserNameToRegistry( CHAR *szUserName, BOOL bForDb)
{
	BOOL bResult = TRUE;

	TCHAR szKey[256] = {0};
	HKEY hRegKey = NULL;
	LONG resRegCreateKey;
	if(bForDb)
	{
		resRegCreateKey = RegCreateKey(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_DB_KEY , &hRegKey);
	}
	else
	{
		resRegCreateKey = RegCreateKey(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_KEY , &hRegKey);
	}
	
	//if(RegCreateKey(HKEY_LOCAL_MACHINE, INMAGE_FAILVOER_KEY , &hRegKey) != ERROR_SUCCESS)
	if(resRegCreateKey != ERROR_SUCCESS)
	{
		DebugPrintf("Error occured while performing encryption, [%d]", GetLastError());
		return FALSE;
	}
	// Write data to registry.
	DWORD dwType = REG_SZ;
	//DWORD dwLength = strlen(szDomainUserName)+1; 
	DWORD dwLength = (DWORD)strlen(szUserName)+1;
	if(bForDb)
	{
		// Add the Domain User.
		if (::RegSetValueEx(hRegKey, _T("UserName"), 0, dwType, (LPBYTE)szUserName, dwLength)!=ERROR_SUCCESS)
		{
			DebugPrintf("\nCould not set the DB UserName key in the registry."); 
			bResult = FALSE;
		}
		else
		{
			//DebugPrintf("\n Successfully persisted the Domain User Name in the Registry");
		}
	}
	else
	{
		// Add the Domain User.
		if (::RegSetValueEx(hRegKey, _T("DomainUserName"), 0, dwType, (LPBYTE)szUserName, dwLength)!=ERROR_SUCCESS)
		{
			DebugPrintf("\nCould not set the DomainUserName key in the registry."); 
			bResult = FALSE;
		}
		else
		{
			//DebugPrintf("\n Successfully persisted the Domain User Name in the Registry");
		}
	}
	RegFlushKey(hRegKey);
	::RegCloseKey(hRegKey);
	return bResult;
}

BOOL InmSaveDomainNameToRegistry( CHAR *szDomianName)
{
	
	BOOL bResult = TRUE;

	TCHAR szKey[256] = {0};
	HKEY hRegKey = NULL;
	
	if (RegCreateKey(HKEY_LOCAL_MACHINE, InmINMAGE_FAILOVER_KEY , &hRegKey) != ERROR_SUCCESS)
	{
		DebugPrintf("Error occured while performing encryption, [%d]", GetLastError());
		return FALSE;
	}
	// Write data to registry.
	DWORD dwType = REG_SZ;
	DWORD dwLength = (DWORD)strlen(szDomianName)+1;
	// Add the DomainName.
	if (::RegSetValueEx(hRegKey, _T("DomainName"), 0, dwType, (LPBYTE)szDomianName, dwLength)!=ERROR_SUCCESS)
	{
		DebugPrintf("\nCould not set the DomainName key in the registry."); 
		bResult = FALSE;
	}
	else
	{
		//DebugPrintf("\n Successfully persisted the Domain Name in the Registry");
	}
	RegFlushKey(hRegKey);
	::RegCloseKey(hRegKey);
	return bResult;
}



bool ChangeNewRepoDriveLetter(const std::string& existingRepo,
                              const std::string& newRepo)
{
    bool bRet = false ;
    WCHAR guid[36];
	WCHAR newRepoPathName[256] = {'\0'} ;
	WCHAR existingRepoPathName[256] = {'\0'} ;
    if( existingRepo.length() < 3 )
    {
        _snwprintf(existingRepoPathName, sizeof(existingRepoPathName), L"%c:\\", existingRepo[0]);
    }
    else
    {
		_snwprintf(existingRepoPathName, existingRepo.length() + 1 , L"%S\\", existingRepo.c_str());
    }
    if( newRepo.length() < 3 )
    {
        _snwprintf(newRepoPathName, sizeof(newRepoPathName), L"%c:\\", newRepo[0]);
    }
    else
    {
		_snwprintf(newRepoPathName, newRepo.length() + 1, L"%S\\", newRepo.c_str());
    }
	if( newRepo.length() <3 )
    {
        bRet = DriveLetterToGuid(newRepo[0], guid, sizeof(guid)) ;
    }
    else
    {
        bRet = MountPointToGuid(newRepo.c_str(), guid, sizeof(guid)) ; 
    }
	if( bRet ) 
	{
		wprintf (L"%s\n", newRepoPathName);  
		wprintf (L"%s\n", existingRepoPathName);  
		if( DeleteVolumeMountPointW(newRepoPathName) &&
            DeleteVolumeMountPointW(existingRepoPathName) )
		{
            std::wstring guidpath = L"\\\\?\\Volume{" ;
            guidpath.append( guid, 36 ) ;
            guidpath +=  L"}\\" ;
            if(SetVolumeMountPointW(existingRepoPathName, guidpath.c_str()))
			{
				bRet = true ;
			}
			else
			{
				std::cerr << "\rFailed to set the mount point " << existingRepo << " for guid " << guid << ". Error " << GetLastError() << std::endl ;
			}
		}
		else
		{
			std::cout<<"\tFailed to remove the mount point for " << existingRepo << ". Error " << GetLastError() << std::endl ;
		}
	}
	else
	{
		std::cerr<<"\tFailed to get the guid for " << newRepo << std::endl ;
	}
    return bRet ;
}
bool SetSparseAttribute(const std::string& file)
{
    bool bRet = false ;
    HANDLE hHandle = INVALID_HANDLE_VALUE ;
    hHandle = SVCreateFile(file.c_str(),GENERIC_WRITE ,0, NULL, 
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
    DWORD BytesReturned = 0;
    if( hHandle  != INVALID_HANDLE_VALUE )
    {
        bRet = true ;
        if( !DeviceIoControl(hHandle,FSCTL_SET_SPARSE,NULL,0,NULL,0,&BytesReturned,NULL ) )
        {
            bRet = false; 
            DebugPrintf(SV_LOG_ERROR,"Unable to set sparse attribute on %s ERROR: %d\n",file.c_str(), GetLastError());
        }
        SAFE_CLOSEHANDLE(hHandle);
    }
    return bRet ;
}
std::string ErrorMessageInReadableFormat (const std::string &repoPath , DWORD dword) 
{
	std::string generalMessage ; 
	generalMessage = "Reason : "; 
		generalMessage += GetErrorMessage(dword) ; 	
	return generalMessage ; 
}

std::string GetErrorMessage(DWORD dword)
{
	HLOCAL hlocVal;
	int n = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM,NULL,dword,0 ,(LPTSTR)&hlocVal,0,NULL);
	LocalLock(hlocVal);
	std::string strMsg = (LPTSTR)hlocVal;
	LocalFree(hlocVal);
	return strMsg;
}



int
RegisterSecurityHandleWithDriver()
{
    bool bResult = false ;
	DWORD   dwReturn;	
	VsnapErrorInfo errinfo ;
	HANDLE hFile = NULL;    
	VirVolume virvolume ;
	int retries = 0 ;
	do 
	{
		if( hFile = virvolume.createvirtualvolume() )
		{
			bResult = DeviceIoControl(
							 hFile,
							 (DWORD)IOCTL_IMPERSONATE_CIFS,
							 NULL,
							 0,
							 NULL,
							 0,
							 &dwReturn, 
							 NULL        // Overlapped
							 );
			if (bResult) {
				DebugPrintf(SV_LOG_DEBUG, "Returned Success from IOCTL_IMPERSONATE_CIFS DeviceIoControl call for volume \n");
			} else {
				DebugPrintf(SV_LOG_ERROR, "Returned Failed from IOCTL_IMPERSONATE_CIFS DeviceIoControl call for volume: %s\n", GetErrorMessage(GetLastError()));
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to open the vsnap control device. it will be retried..\n") ;
		}
		if( bResult == TRUE )
		{
			break ;
		}
		retries ++ ;
		ACE_OS::sleep(5) ;
		if( retries > 5 )
		{
			DebugPrintf(SV_LOG_ERROR, "unable to make IOCTL_IMPERSONATE_CIFS even after %d retries\n", retries) ;
			break ;
		}
	} while(true) ;
	 return bResult ;
}
bool CredentailsAvailable()
{
	std::string domain, user, password ;
	LocalConfigurator lc ;
	if( !lc.isGuestAccess() )
	{
		DebugPrintf(SV_LOG_DEBUG, "Reading the registry keys from new location\n") ;
		if( InmReadCredentials( domain, user, password ) )
		{
			return true ;
		}
		DebugPrintf(SV_LOG_DEBUG, "Reading the registry keys from old location\n") ;
		if( ReadCredentials( domain, user, password ) )
		{
			return true ;
		}
	}
	else
	{
		return true ;
	}
	return false ;
}

bool GetCredentials(std::string& domain, std::string& username, std::string& password)
{
	DebugPrintf(SV_LOG_DEBUG, "Reading the registry keys from new location\n") ;
	if( InmReadCredentials( domain, username, password ) )
{
		return true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Reading the registry keys from old location\n") ;
	if( ReadCredentials( domain, username, password ) )
	{
		return true ;
	}
	return false ;
}
bool isShareReadWriteable(const std::string& sharepath, INM_ERROR& errCode, std::string& errmsg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool process = false ;
	DWORD error = 0 ;
	HANDLE hFile; 
	
	char DataBuffer[] = "This is some test data to write to the file.";
	DWORD dwBytesToWrite = (DWORD)strlen(DataBuffer);
	DWORD dwBytesWritten = 0;
	SVMakeSureDirectoryPathExists(sharepath.c_str()) ;
	std::string accesscheck = sharepath ;
	if( accesscheck.length() == 1 )
	{
		accesscheck += ":\\" ;
	}
	if( accesscheck[accesscheck.length() - 1 ] != '\\' )
	{
		accesscheck += '\\' ;
	}
	//accesscheck += "\\" ;
	accesscheck += "accesscheck" ;
	accesscheck += boost::lexical_cast<std::string>(ACE_OS::getpid()) ;
	accesscheck += "_" ;
	accesscheck += boost::lexical_cast<std::string>(GetCurrentThreadId()) ;
	hFile = CreateFileW(getLongPathName(accesscheck.c_str()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);                 
	if( hFile != INVALID_HANDLE_VALUE && WriteFile( hFile, DataBuffer, dwBytesToWrite, 
							&dwBytesWritten, NULL) && dwBytesWritten >= dwBytesToWrite)
	{
		DebugPrintf(SV_LOG_DEBUG, "Able to write to files in the %S\n", getLongPathName(accesscheck.c_str()).c_str()) ;
		process = true ;
	}
	else
	{
		process = false ;
		error = GetLastError() ;
		errmsg = ErrorMessageInReadableFormat (sharepath ,error); // GetErrorMessage(error) ;
		DebugPrintf(SV_LOG_ERROR, "Failed to create/write files in %S with error %s\n", getLongPathName(accesscheck.c_str()).c_str(), errmsg.c_str()) ;
	}						
	if( hFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile) ; 
	}
	if( process )
	{
		ACE_OS::unlink( accesscheck.c_str()) ;
		accesscheck += "_dir" ;
		if( SVMakeSureDirectoryPathExists(accesscheck.c_str()) != SVS_OK)
		{
			process = false ;
			error = GetLastError() ;
			errmsg = ErrorMessageInReadableFormat (sharepath ,error); // GetErrorMessage(error) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to create/write directories in %s with error %s\n", sharepath.c_str(), errmsg.c_str()) ;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Able to create directories in the %s\n", sharepath.c_str()) ;
			ACE_OS::rmdir(accesscheck.c_str()) ;
		}
	}
	if( !process )
	{
		DebugPrintf(SV_LOG_ERROR, "No read/write access is available in the backup location\n") ;
		switch( error )
		{
			case ERROR_BAD_NETPATH :
			case ERROR_BAD_NET_NAME :
				errCode = E_REPO_NOT_AVAILABLE ;
				break ;
			case ERROR_ACCESS_DENIED :
				errCode = E_REPO_NORW_ACCESS ;
				break ;
			default :
				errCode = E_REPO_NOT_AVAILABLE ;
		}
	}
	else
	{
		errCode = E_SUCCESS ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return process ;
}


bool CloseConnection()
{
	LocalConfigurator lc ;
	bool bret = false ;
	try
	{
		std::string sharepath = lc.getRepositoryLocation() ;
		bret = CloseConnection(sharepath) ;
	}
	catch(ContextualException& ex)
	{
	}
	return bret ;
}
bool CloseConnection(const std::string& share)
{
	bool bret = true ;
	if( share.length() > 2 && share[0] == '\\' && share[1] == '\\' )
	{
		std::string sharepath = share ;
		if( sharepath[sharepath.length() - 1] == '\\' )
		{
			sharepath.erase(sharepath.length()-1) ;
		}
		DWORD dret ; 
		if( ( dret = WNetCancelConnection2( LPSTR(sharepath.c_str()), 0, TRUE ) ) != NO_ERROR )
		{
			bret = false ;
			DebugPrintf(SV_LOG_ERROR, "Failed to close the connection %ld\n", dret ) ;
		}
	}
	return bret ;
}
/*
bool logonuser(const std::string& user,
			   const std::string& domain,
			   const std::string& password,
			   INM_ERROR& error, 
			   std::string& errormsg,
			   HANDLE& hToken)
{
	return true ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool process = true ;
	if( !LogonUserEx(user.c_str(), domain.c_str(), password.c_str(), LOGON32_LOGON_NEW_CREDENTIALS, 
		LOGON32_PROVIDER_WINNT50, &hToken, NULL, NULL, NULL, NULL ) )
	{
		process = false ;
		error = E_INVALID_CREDENTIALS ;
		DebugPrintf(SV_LOG_ERROR, "user %s, domain %s\n", user.c_str(), domain.c_str()) ;
		errormsg = GetErrorMessage(GetLastError()) ;
		DebugPrintf(SV_LOG_ERROR, "Failed to login with given credentials. %s\n", errormsg.c_str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return process ;
}

*/

bool WNetAddConnectionWrapper(const std::string& sharepath,
							  const std::string& user,
							  const std::string& password,
							  const std::string& domain,
							  INM_ERROR& error, 
							  std::string& errormsg)
{

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool process = false ;
	DWORD dwordRet ;
	NETRESOURCE myNetResource ;
	memset(&myNetResource, 0, sizeof(myNetResource));
    myNetResource.dwType = RESOURCETYPE_DISK ; 
	myNetResource.lpLocalName = NULL ;
	myNetResource.lpRemoteName = LPSTR(sharepath.c_str()) ;
	std::string fquser = domain ;
	fquser += "\\" ;
	fquser += user ;
	DebugPrintf(SV_LOG_DEBUG, "Adding connection\n"); 
	DebugPrintf(SV_LOG_DEBUG, "user name %s\n", fquser.c_str()); 
	dwordRet = WNetAddConnection2( &myNetResource, password.c_str(), fquser.c_str(), CONNECT_TEMPORARY) ;
	if( dwordRet == ERROR_SESSION_CREDENTIAL_CONFLICT )
	{
		CloseConnection( LPSTR(sharepath.c_str()) ) ;
		Sleep(30000) ;
		dwordRet = WNetAddConnection2( &myNetResource, password.c_str(), fquser.c_str(), CONNECT_TEMPORARY) ;
	}
	if( dwordRet != NO_ERROR )
	{
		switch( dwordRet )
		{
			case ERROR_ACCESS_DENIED :
				error = E_REPO_NORW_ACCESS ;
				break ;
			case ERROR_BAD_USERNAME :
			case ERROR_INVALID_PASSWORD :
			case ERROR_LOGON_FAILURE :
			case ERROR_LOGON_TYPE_NOT_GRANTED :
				error = E_INVALID_CREDENTIALS ;
				break ;
			case ERROR_BAD_NETPATH :
			case ERROR_BAD_NET_NAME :
				error = E_REPO_NOT_AVAILABLE ;
				break ;
			default :
				error = E_REPO_NOT_AVAILABLE ;
		}
		errormsg = ErrorMessageInReadableFormat (sharepath ,dwordRet); //errormsg = GetErrorMessage(dwordRet) ;
		DebugPrintf(SV_LOG_ERROR, "Adding connection %s failed. error %s\n", sharepath.c_str(), GetErrorMessage(dwordRet).c_str()) ;
		process = false ;
	}
	else
	{
		error = E_SUCCESS ;
		DebugPrintf(SV_LOG_DEBUG, "No error\n") ; 
		process = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return process ;
}

bool AddConnection(const std::string& share,
				   const std::string& user,
				   const std::string& password,
				   const std::string& domain,
				   bool isGuestAccess,
				   INM_ERROR& error,
				   std::string& errormsg)
{
	bool process = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
	error = E_SUCCESS ;
	if( !isGuestAccess )
	{
		process = WNetAddConnectionWrapper(share, user, password, domain, error, errormsg) ;
	}
	else
	{
		process = true ;
	}
	return process ;
}
bool ImpersonateAccessToShare(INM_ERROR& error,
							  std::string& errormsg)
{
	bool bret = false ;
	LocalConfigurator lc ;
	std::string sharepath = lc.getRepositoryLocation() ;
	if( sharepath.length() > 2 && sharepath[0] == '\\' && sharepath[1] == '\\' )
	{
		if( sharepath[sharepath.length() - 1] == '\\' )
		{
			sharepath.erase(sharepath.length()-1) ;
		}
		do
		{
			std::string domain, user, password ;
			if( !GetCredentials( domain, user, password ) )
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to fetch the credentials\n") ;
				break ;
			}
			bret = ImpersonateAccessToShare(user, password, domain, error, errormsg); 
		} while(false) ;
	}
	return bret ;
}
void persistAccessCredentials(const std::string& repoPath, const std::string& username,const std::string& password,
							  const std::string& domain,INM_ERROR& errCode,
								std::string& errmsg, bool guestAccess)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	LocalConfigurator lc ;
	errCode = E_SUCCESS ;
	if ( isRepositoryConfiguredOnCIFS( repoPath ) )
	{
		if(!guestAccess)
		{
			SaveCredentialsInReg(username,password,domain,errCode,errmsg);
			lc.setGuestAccess(false);
		}
		else
		{
			RemCredentialsFromRegistry() ;
			lc.setGuestAccess(true);
		}
	}
	else 
	{
		RemCredentialsFromRegistry() ;
		lc.setGuestAccess(false);
	}
		
		
}

BOOL RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
{
    LPTSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;

    // First, see if we can delete the key without having
    // to recurse.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS)
	{
        return TRUE;
	}
    lResult = RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS) 
    {
        if (lResult == ERROR_FILE_NOT_FOUND) {
            DebugPrintf(SV_LOG_WARNING, "Key not found.\n");
            return TRUE;
        } 
        else {
            DebugPrintf(SV_LOG_ERROR, "Error opening key.\n");
            return FALSE;
        }
    }

    // Check for an ending slash and add one if it is missing.

    lpEnd = lpSubKey + lstrlen(lpSubKey);

    if (*(lpEnd - 1) != TEXT('\\')) 
    {
        *lpEnd =  TEXT('\\');
        lpEnd++;
        *lpEnd =  TEXT('\0');
    }

    // Enumerate the keys

    dwSize = MAX_PATH;
    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                           NULL, NULL, &ftWrite);

    if (lResult == ERROR_SUCCESS) 
    {
        do {

            StringCchCopy (lpEnd, MAX_PATH*2, szName);

            if (!RegDelnodeRecurse(hKeyRoot, lpSubKey)) {
                break;
            }

            dwSize = MAX_PATH;

            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                                   NULL, NULL, &ftWrite);

        } while (lResult == ERROR_SUCCESS);
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey (hKey);

    // Try again to delete the key.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS) 
        return TRUE;

    return FALSE;
}

void RemCredentialsFromRegistry()
{
    TCHAR szDelKey[MAX_PATH*2];
    StringCchCopy (szDelKey, MAX_PATH*2, TEXT(InmINMAGE_FAILOVER_KEY));
    RegDelnodeRecurse(HKEY_LOCAL_MACHINE, szDelKey);
}
bool SaveCredentialsInReg(const std::string& user, 
						  const std::string& password, 
						  const std::string& domain,
						  INM_ERROR& errCode,
						  std::string& errmsg)
{
	bool ret = false ;
	do
	{

		if(InmSavePasswordToRegistry(const_cast<char*>(password.c_str()), FALSE) == FALSE)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to persist password \n") ;
			break;
		}
		if(InmSaveDomainNameToRegistry(const_cast<char*>(domain.c_str())) == FALSE)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to persist domain \n") ;
			break ;
		}
		if(InmSaveUserNameToRegistry(const_cast<char*>(user.c_str()), FALSE) == FALSE)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to persist user \n") ;
			break ;
		}
		ret = true ;
	} while( false ) ;
	if( !ret )
	{
		errmsg = "Unable to persist the backup location access information" ;
		errCode = E_INTERNAL ;
	}
	return ret ;
}

bool ImpersonateAccessToShare( const std::string& user, 
							   const std::string& password,
						       const std::string& domain,
							   INM_ERROR& error,
							   std::string& errormsg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
	bool ret = false ;
	error = E_INTERNAL ;
	LocalConfigurator lc ;
	if( lc.isGuestAccess() )
	{
		return true ;
	}
	HANDLE hToken = NULL ;
	do
	{
		if( !LogonUserEx(user.c_str(), domain.c_str(), password.c_str(), LOGON32_LOGON_NEW_CREDENTIALS, 
			LOGON32_PROVIDER_WINNT50, &hToken, NULL, NULL, NULL, NULL ) )
		{
			DebugPrintf(SV_LOG_ERROR, "user %s, domain %s\n", user.c_str(), domain.c_str()) ;
			error = E_INVALID_CREDENTIALS ;
			errormsg = GetErrorMessage(GetLastError()) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to login with given credentials. %s\n", errormsg.c_str()) ;
			break ;
		}
		if( !ImpersonateLoggedOnUser( hToken ) )
		{
			errormsg = GetErrorMessage(GetLastError()) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to impersonate with logged on user %s\n", errormsg.c_str()) ;
			break ;
		}
		if( !RegisterSecurityHandleWithDriver() )
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to register the security context with driver\n") ;
			RevertToSelf() ;
			break ;
		}
		RevertToSelf() ;
		ret = true ;
		error = E_SUCCESS ;
	} while( false ) ;
	if( hToken != NULL )
	{
		CloseHandle( hToken ) ;
		hToken = NULL ;
	}
		
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	return ret ;
}
bool AccessRemoteShare(INM_ERROR& error,
					   std::string& errormsg)
{
	bool ret = false ;
	LocalConfigurator lc ;
	try
	{
		std::string repoPath ;
		repoPath = getRepositoryPath(lc.getRepositoryLocation()) ;
		ret = AccessRemoteShare(repoPath, error,errormsg) ;
	}
	catch(ContextualException& ex)
	{
	}
	return ret ;
}

bool AccessRemoteShare(const std::string& share, const std::string& user,
					   const std::string& password, const std::string& domain,
					   bool guestAccess, INM_ERROR& error,
					   std::string& errormsg, bool UnShareAndShare)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
	bool process = false ;
	std::string sharepath = share ;
	if( sharepath[sharepath.length() - 1] == '\\' )
	{
		sharepath.erase(sharepath.length()-1) ;
	}
	if( sharepath.length() > 2 && sharepath[0] == '\\' && sharepath[1] == '\\' )
	{
		do
		{
			if( UnShareAndShare || !isShareReadWriteable(sharepath, error, errormsg) )
			{
				if( !AddConnection(sharepath, user, password, domain, guestAccess, error, errormsg) )
				{
					break ;
				}
			}
			if( !isShareReadWriteable(sharepath, error, errormsg) )
			{
				break ;
			}
			process = true ;
			error = E_SUCCESS ;
		} while( false ) ;
	}
	else
	{
		if( isShareReadWriteable(sharepath, error, errormsg) )
		{
			process = true ;
		}
	}
	return process ;
}

bool AccessRemoteShare(const std::string& share,
					   INM_ERROR& error,
					   std::string& errormsg, bool UnShareAndShare)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
	std::string user, password, domain ;
	bool process = false ;
	bool guestAccess = true ;
	if( share.length() > 2 && share[0] == '\\' && share[1] == '\\' )
	{
		LocalConfigurator lc ;
		if( !lc.isGuestAccess() )
		{
			guestAccess = false ;
			if( !GetCredentials( domain, user, password ) )
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to fetch the credentials\n") ;
				error = E_UNKNOWN_CREDENTIALS ;
			}
		}
	}
	process = AccessRemoteShare(share, user, password, domain, guestAccess, error, errormsg, UnShareAndShare) ;
	return process ;
}

void CopyRange(HANDLE hFrom, HANDLE hTo, LARGE_INTEGER iPos, LARGE_INTEGER iSize) 
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
	BYTE buf[64*1024];
	SV_ULONG dwBytesRead, dwBytesWritten;
	SV_LONG iPosHigh = iPos.HighPart;
	SV_ULONG dr = ::SetFilePointer(hFrom, iPos.LowPart, &iPosHigh, FILE_BEGIN);
	if (dr == INVALID_SET_FILE_POINTER) {
		dr = ::GetLastError();
		if (dr != NO_ERROR) throw dr;
	}

	iPosHigh = iPos.HighPart;
	dr = ::SetFilePointer(hTo, iPos.LowPart, &iPosHigh, FILE_BEGIN);
	if (dr == INVALID_SET_FILE_POINTER) {
		dr = ::GetLastError();
		if (dr != NO_ERROR) throw dr;
	}

	while (iSize.QuadPart > 0) {
		SV_ULONG bytesRead; 
		if ( sizeof(buf) < iSize.QuadPart )
		{
			bytesRead = sizeof(buf); 
		}
		else 
		{
			bytesRead = iSize.QuadPart;
		}
		if (!::ReadFile(hFrom, buf,bytesRead, &dwBytesRead, NULL)) throw ::GetLastError();
		if (!::WriteFile(hTo, buf, dwBytesRead, &dwBytesWritten, NULL)) throw ::GetLastError();
		if (dwBytesWritten < dwBytesRead) throw (DWORD)ERROR_HANDLE_DISK_FULL;
		iSize.QuadPart -= dwBytesRead;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
}

void CopySparseFileRegularly(HANDLE hInFile, HANDLE hOutFile)
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
	BYTE buf[64*1024];
	SV_ULONG dwBytesRead, dwBytesWritten;

	do {
		if (!::ReadFile(hInFile, buf, sizeof(buf), &dwBytesRead, NULL)) 
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read the file\n") ;
			throw ::GetLastError();
		}
		if (dwBytesRead) {
			if (!::WriteFile(hOutFile, buf, dwBytesRead, &dwBytesWritten, NULL)) 
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to write the file\n") ;
				throw ::GetLastError();
			}
			if (dwBytesWritten < dwBytesRead) 
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to write entire bytes.. Disk may be full\n") ;
				throw (DWORD)ERROR_HANDLE_DISK_FULL;
			}
		}
	} while (dwBytesRead == sizeof(buf));
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
}
bool GetUsedSpace(const char * directory, const char * file_pattern, 
								 SV_ULONGLONG & filecount, SV_ULONGLONG & size_on_disk,SV_ULONGLONG& logicalsize)
{
	 bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
    do
    {
        if( ( NULL == directory ) || ( NULL == file_pattern ) )
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s invalid (null) input.\n",
                FUNCTION_NAME);
            rv = false;
            break;
        }

        // open directory
        // PR#10815: Long Path support
        ACE_DIR *dirp = sv_opendir(directory);

        if (dirp == NULL)
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "FUNCTION:%s opendir failed, path = %s, errno = %d\n", 
                FUNCTION_NAME, directory, ACE_OS::last_error()); 
            break;
        }

        struct ACE_DIRENT *dp = NULL;

        do
        {
            ACE_OS::last_error(0);

            // Get directory entry
            if ((dp = ACE_OS::readdir(dirp)) != NULL)
            {
                //Find a matching entry
                std::string dName ;
				dName = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
				if ( dName == "." || dName == ".." )
						continue;
				if (RegExpMatch(file_pattern, ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
                {
                    std::string filePath;
					filePath = directory;                    
                    filePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                    filePath += ACE_TEXT_ALWAYS_CHAR(dp->d_name);
					ACE_stat statbuf = {0};
					if(sv_stat(getLongPathName(filePath.c_str()).c_str(), &statbuf) != 0)
					{
						DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s Failed to get file :%s statistics. errno = %d.\n",
                            FUNCTION_NAME,filePath.c_str(), ACE_OS::last_error());
                        continue;
						//rv = false;
						//break;
					}
					else
					{
						if((statbuf.st_mode & S_IFDIR) == S_IFDIR )
						{
							DebugPrintf(SV_LOG_DEBUG, "entered inside dir %s\n",filePath.c_str());
							GetUsedSpace(filePath.c_str(), file_pattern ,filecount, size_on_disk, logicalsize) ;
							
						}
						else
						{
							size_on_disk += File::GetSizeOnDisk(filePath);
							logicalsize += statbuf.st_size;
							DebugPrintf(SV_LOG_DEBUG, "file path is %s and logicalsize is %llu size on disk %llu\n",filePath.c_str(),logicalsize, size_on_disk);
						}
						
					}
                    filecount += 1;
                }
            }
        } while(dp);

        //Close directory
        ACE_OS::closedir(dirp);

		DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s directory:%s pattern:%s, size_on_disk =" ULLSPEC " \n",
			FUNCTION_NAME, directory,file_pattern, size_on_disk);

    } while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
    return rv;

}

bool CopySparseFile(std::string &srcFile, std::string &tgtFile, unsigned long& err)
{
	bool bRet = false;
	HANDLE hInFile = INVALID_HANDLE_VALUE, hOutFile = INVALID_HANDLE_VALUE ;
	int retry = 0 ;
	do 
	{
		DWORD dr ;
		hInFile = hOutFile = INVALID_HANDLE_VALUE ;
		retry++ ;
		try 
		{
			BY_HANDLE_FILE_INFORMATION bhfi;

			hInFile = ::CreateFile(srcFile.c_str() , GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if (hInFile == INVALID_HANDLE_VALUE) 
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to create the file %s\n", srcFile.c_str());
				throw ::GetLastError();
			}
			hOutFile = ::CreateFile(tgtFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if (hOutFile == INVALID_HANDLE_VALUE) 
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to create the file %s\n", tgtFile.c_str()) ;
				throw ::GetLastError();
			}

			LARGE_INTEGER iFileSize;
			iFileSize.LowPart = ::GetFileSize(hInFile, (LPDWORD)&iFileSize.HighPart);
			if (iFileSize.LowPart == (DWORD)-1) {
				dr = ::GetLastError();
				if (dr != NO_ERROR) 
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get the file size\n") ;
					throw dr;
				}
			}
			
			FILE_ALLOCATED_RANGE_BUFFER queryrange;
			FILE_ALLOCATED_RANGE_BUFFER ranges[1024];
			SV_ULONG nbytes, n, i;
			BOOL br;

			queryrange.FileOffset.QuadPart = 0;
			queryrange.Length = iFileSize;
			
			do {	// This loop executes more than once only if the file has more than 1024 allocated areas
				br = ::DeviceIoControl(hInFile, FSCTL_QUERY_ALLOCATED_RANGES, &queryrange, sizeof(queryrange), ranges, sizeof(ranges), &nbytes, NULL);
				if (!br) {
					dr = ::GetLastError();
					if( dr == ERROR_BAD_NET_RESP )
					{
						DebugPrintf(SV_LOG_ERROR, "DeviceIoControl failed while querying the allocated ranges error :%d\n",dr) ;
						break ;
					}
					if (dr == ERROR_NOT_SUPPORTED )
					{
						DebugPrintf(SV_LOG_ERROR, "DeviceIoControl failed while querying the allocated ranges error :%d\n",dr) ;
						break ;
					}
					if (dr != ERROR_MORE_DATA ) 
					{
						DebugPrintf(SV_LOG_ERROR, "DeviceIoControl failed while querying the allocated ranges\n") ;
						throw dr;
					}
				}

				n = nbytes / sizeof(FILE_ALLOCATED_RANGE_BUFFER);		// Calculate the number of records returned
				for (i=0; i<n; i++)		// Main loop
				{
					CopyRange(hInFile, hOutFile, ranges[i].FileOffset, ranges[i].Length);
				}

				// Set starting address and size for the next query
				if (!br && n > 0) {
					queryrange.FileOffset.QuadPart = ranges[n-1].FileOffset.QuadPart + ranges[n-1].Length.QuadPart;
					queryrange.Length.QuadPart = iFileSize.QuadPart - queryrange.FileOffset.QuadPart;
				}
			} while (!br);	// Continue loop if ERROR_MORE_DATA
			if( dr == ERROR_BAD_NET_RESP || dr == ERROR_NOT_SUPPORTED)
			{
				DebugPrintf(SV_LOG_DEBUG, "Failure to query the sparse file ranges.. Using traditional copy\n") ;
				CopySparseFileRegularly(hInFile, hOutFile) ;
			}
			else
			{
				// Set end of file (required because there can be a sparse area in the end)
				dr = ::SetFilePointer(hOutFile, iFileSize.LowPart, &iFileSize.HighPart, FILE_BEGIN);
				if (dr == INVALID_SET_FILE_POINTER) {
					dr = ::GetLastError();
					if (dr != NO_ERROR) 
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to set the file pointer for out file\n") ;
						throw dr;
					}
				}
				if (!::SetEndOfFile(hOutFile)) 
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to set the EOF for out file\n") ;
					throw ::GetLastError();
				}
			}
			::CloseHandle(hInFile);
			hInFile = INVALID_HANDLE_VALUE ;
			bRet = true; 
		}
		catch (SV_ULONG dwErrCode) 
		{
			DebugPrintf (SV_LOG_DEBUG, "LastError is %ld ",dwErrCode); 
			err = dwErrCode ;
			bRet = false;
			if( hOutFile != INVALID_HANDLE_VALUE )
			{
				::CloseHandle(hOutFile);
				hOutFile = INVALID_HANDLE_VALUE ;
			}
			if( hInFile != INVALID_HANDLE_VALUE )
			{
				::CloseHandle(hInFile);
				hInFile = INVALID_HANDLE_VALUE ;
			}
			if( retry < 3 )
			{
				DebugPrintf(SV_LOG_ERROR, "File copy operation will be retried\n") ;
				Sleep(2000);//delaying for 2 sec for the next retry
			}
		}
	} while( bRet == false && retry < 3 ) ;
	return bRet;
}
bool SupportsSparseFiles(const std::string& path )
{
	std::string filepath = path ;
	if( filepath.length() == 1 )
	{
		filepath += ":\\" ;
	}
	if( filepath[filepath.length() - 1 ] != '\\' )
	{
		filepath += '\\' ;
	}
	filepath += "test.filepart0" ;

	bool bRet = false ;

#ifdef SV_WINDOWS
	SV_ULONGLONG maxsize = 214756 ;//maxsize can be any size, here it is taken for test purposes 
    HANDLE hHandle = INVALID_HANDLE_VALUE ;
	DWORD size ;
    hHandle = SVCreateFile(filepath.c_str(),GENERIC_WRITE ,0, NULL, 
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
    DWORD BytesReturned = 0;
    if( hHandle  != INVALID_HANDLE_VALUE )
    {
        bRet = true ;
        if( !DeviceIoControl(hHandle,FSCTL_SET_SPARSE,NULL,0,NULL,0,&BytesReturned,NULL ) )
        {
            bRet = false; 
            DebugPrintf(SV_LOG_ERROR,"Unable to set sparse attribute on %s ERROR: %d\n",filepath.c_str(), GetLastError());
        }
		else
		{
			LARGE_INTEGER li;
		 	li.QuadPart= maxsize;
			li.LowPart=SetFilePointer(hHandle,li.LowPart,&li.HighPart,FILE_END);
			if ( li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
			{
				DebugPrintf(SV_LOG_ERROR,"SetFilePointer call failed on %s. error: %d\n",filepath.c_str(), GetLastError());
				bRet = false;
			}
			else
			{
				if(0 == SetEndOfFile(hHandle))
				{
					DebugPrintf(SV_LOG_ERROR,"SetEndOfFile call failed on %s. error: %d\n",filepath.c_str(), GetLastError());
					bRet = false;
				}
				else
				{
					if ((size=GetCompressedFileSize(filepath.c_str(),NULL))== 214756 )
					{
							DebugPrintf(SV_LOG_ERROR,"FS_FILE_COMPRESSION not support and size is %lu\n",size);
							bRet= false;
					}
					else
					{
							DebugPrintf(SV_LOG_ERROR,"FS_FILE_COMPRESSION  support and size is %lu\n",size);
							bRet= true;
					}
				}
			}
		}
		SAFE_CLOSEHANDLE(hHandle);
		ACE_OS::unlink( filepath.c_str()) ;
    }
	else
	{
		DebugPrintf(SV_LOG_ERROR,"failed to open the file %s\n", filepath.c_str());
	}

  return bRet ;
#else
  return true ;
#endif	
}
//*************************************************************
//
//  ReadScoutMailRegistryKey()
//
//  Purpose:    Gets SMR InstallationDirectory value from Registry .
//
//  Parameters: value	-  (Get the Value of "InstallDirectory" Value from "SOFTWARE\\InMage Systems\\Installed Products\\3" from registry)
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

bool ReadScoutMailRegistryKey (std::string &value)
{
	BOOL bRet = false; 
	char lszValue[255];
	HKEY hKey;
	SV_LONG returnStatus;
	SV_ULONG dwType = REG_SZ;
	SV_ULONG dwSize=255;
	returnStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\InMage Systems\\Installed Products\\3", NULL,  KEY_ALL_ACCESS, &hKey);
	if (returnStatus == ERROR_SUCCESS)
	{
		returnStatus = RegQueryValueEx(hKey, TEXT("InstallDirectory"), NULL, &dwType,(LPBYTE)&lszValue, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
		{
			bRet = true; 
			DebugPrintf (SV_LOG_DEBUG,"Value Read is %s\n", lszValue);
			value = lszValue ; 
		}
		else 
		{
			DebugPrintf (SV_LOG_ERROR ,"Get Last Error is %s\n", GetErrorMessage (returnStatus).c_str() );	
		}
		RegCloseKey(hKey);
	}
	else 
	{
		DebugPrintf (SV_LOG_ERROR  ,"Get Last Error is %s\n", GetErrorMessage (returnStatus).c_str() );	
	}
	return bRet; 
}


bool GetInMageRebootPendingKey()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	BOOL bRet = false; 
	char lszValue[255];
	HKEY hKey;
	SV_LONG returnStatus;
	SV_ULONG dwType = REG_SZ;
	SV_ULONG dwSize=255;
	returnStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", NULL,  KEY_ALL_ACCESS, &hKey);
	if (returnStatus == ERROR_SUCCESS)
	{
		returnStatus = RegQueryValueEx(hKey, TEXT("InMageRebootPending"), NULL, &dwType,(LPBYTE)&lszValue, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
		{
			bRet = true; 
			DebugPrintf (SV_LOG_DEBUG,"InMageRebootPending key exist\n");
		}
		else 
		{
			DebugPrintf (SV_LOG_ERROR ,"Couldn't read the InMageRebootPending key  Error : %s\n", GetErrorMessage (returnStatus).c_str() );	
		}
		RegCloseKey(hKey);
	}
	else 
	{
		DebugPrintf (SV_LOG_ERROR  ,"Get Last Error is %s\n", GetErrorMessage (returnStatus).c_str() );	
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	return bRet; 

}
bool GetDiskNameForVolume(const DisksInfoMap_t& diskinfomap, const std::string& vol, std::string& diskname)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bret = false ;
	std::string volumename = vol ;
	if( volumename.length() == 1 )
	{
		volumename += ":\\" ;
	}
	if( volumename[volumename.length() - 1] != '\\' )
	{
		volumename += "\\" ;
	}
	DebugPrintf(SV_LOG_DEBUG, "No of disks on the system %d\n", diskinfomap.size()) ;
	DebugPrintf(SV_LOG_DEBUG, "Finding the disk name for %s\n", volumename.c_str()); 
	DisksInfoMap_t::const_iterator iter(diskinfomap.begin());
    DisksInfoMap_t::const_iterator iterEnd(diskinfomap.end());
	for (/* empty */; iter != iterEnd; ++iter) 
	{
		diskname = iter->second.DiskInfoSub.Name ;
		DebugPrintf(SV_LOG_DEBUG, "disk name %s\n", iter->second.DiskInfoSub.Name) ;
		VolumesInfoVec_t::const_iterator tgtVolIter((*iter).second.VolumesInfo.begin());
        VolumesInfoVec_t::const_iterator tgtVolIterEnd((*iter).second.VolumesInfo.end());
        for (/* empty */; tgtVolIter != tgtVolIterEnd; ++tgtVolIter) 
		{
			DebugPrintf(SV_LOG_DEBUG, "volume name %s\n", tgtVolIter->VolumeName) ;
			if( tgtVolIter->VolumeName == volumename )
			{
				DebugPrintf(SV_LOG_DEBUG, "Found the volume name %s in disk %s\n", volumename.c_str(), diskname.c_str()) ;
				bret = true ;
				break ;
			}
		}
		if( bret == true )
		{
			break ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	return bret ;
}
