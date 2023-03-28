#ifdef SV_WINDOWS
#include "Objbase.h"
#include "Rpc.h"
#endif
#include "InMageSDK.h"
#include "APIController.h"
#include "RequestValidator.h"
#include "fileconfigurator.h"
#include "logger.h"
#include "inmsafecapis.h"
bool bNoComInit = false ;
void NoComInit()
{
	bNoComInit = true ;
}
#ifdef SV_WINDOWS
bool enabledLogging = false ;
void SetupLocalLogging()
{
	FileConfigurator lc ;
	if( lc.getHostId() == "" )
	{
		UUID a ;
		UuidCreate( &a ) ;
		RPC_CSTR  uuidstr ;
		UuidToString(&a, &uuidstr) ;
		std::stringstream stream ;
		stream << uuidstr ;
		std::string hostid ;
		hostid = stream.str() ;
		hostid = ToUpper(hostid) ;
		lc.setHostId(hostid) ;
		RpcStringFree(&uuidstr) ;
	}
	std::string logpath = lc.getInstallPath() ;
	logpath += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	logpath += "inmsdk.log" ;
	SetDaemonMode();
	SetLogFileName(logpath.c_str()) ;
	SetLogLevel( 7 ) ;
	SetLogInitSize(10 * 1024 * 1024) ;
}

void init()
{
    HRESULT hr;
	if( !enabledLogging )
	{
		SetupLocalLogging() ;
		enabledLogging = true ;
	}
	if( !bNoComInit )
	{
		DebugPrintf(SV_LOG_DEBUG, "Initializing the com\n") ;
		do
		{
			hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

			if (hr != S_OK)
			{
				DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
				DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
				break;
			}

			hr = CoInitializeSecurity
				(
				NULL,                                //  IN PSECURITY_DESCRIPTOR         pSecDesc,
				-1,                                  //  IN LONG                         cAuthSvc,
				NULL,                                //  IN SOLE_AUTHENTICATION_SERVICE *asAuthSvc,
				NULL,                                //  IN void                        *pReserved1,
				RPC_C_AUTHN_LEVEL_CONNECT,           //  IN DWORD                        dwAuthnLevel,
				RPC_C_IMP_LEVEL_IMPERSONATE,         //  IN DWORD                        dwImpLevel,
				NULL,                                //  IN void                        *pAuthList,
				EOAC_NONE,                           //  IN DWORD                        dwCapabilities,
				NULL                                 //  IN void                        *pReserved3
				);
		

			// Check if the securiy has already been initialized by someone in the same process.
			if (hr == RPC_E_TOO_LATE)
			{
				hr = S_OK;
			}

			if (hr != S_OK)
			{
				DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
				DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
				CoUninitialize();
				break;
			}
		} while(false);
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Not Initializing the com\n") ;
	}
}
#endif


INMAGE_ERROR_CODE processRequestWithFile(const char* xmlRequest,const char* xmlResponse)
{
	INMAGE_ERROR_CODE errCode = E_SUCCESS;
	RequestValidator validator;
#ifdef SV_WINDOWS
    init() ;
#endif
	errCode = validator.initializeRequstInfo(xmlRequest);
    //Exception handling pending
    if(E_SUCCESS == errCode)
    {
        errCode = validator.ValidateFunctionRequest();
    }

    
	//Process error code for additional information.
	//Hanle exception for generating response
	validator.generateResponse(xmlResponse);
	return errCode;
}
INMAGE_ERROR_CODE processRequestWithStream(std::stringstream &xmlRequeststream,std::stringstream &xmlResponseStream)
{
	INMAGE_ERROR_CODE errCode = E_INTERNAL;
#ifdef SV_WINDOWS
    init() ;
#endif
	//APIController controller;
	RequestValidator validator;
    errCode = validator.initializeRequstInfo(xmlRequeststream);
    //Exception handling pending
    if(E_SUCCESS == errCode)
    {
        errCode = validator.ValidateFunctionRequest();
    }
    //Process error code for additional information.
    validator.generateResponse(xmlResponseStream);
	//Hanle exception for generating response
	return errCode;
}

INMAGE_ERROR_CODE processRequestWithSimpleCStream(const char* xmlRequest,char** xmlResponse)
{
#ifdef SV_WINDOWS
    init() ;
#endif
 INMAGE_ERROR_CODE errCode = E_SUCCESS;
 std::stringstream xmlRequeststream;
 xmlRequeststream << std::string(xmlRequest);
 std::stringstream xmlResponseStream;
 std::cout<<xmlRequeststream.str();
 errCode = processRequestWithStream(xmlRequeststream,xmlResponseStream);
 unsigned long bufferSize=xmlResponseStream.str().length(); 
 *xmlResponse = new char[bufferSize+1];
 inm_strncpy_s(*xmlResponse,(bufferSize+1),xmlResponseStream.str().c_str(),(bufferSize+1));
 return errCode; 
}

size_t ErrorMessage(INMAGE_ERROR_CODE errCode,char **pMessageBuf)
{
	size_t bifSize = 0;
	*pMessageBuf = NULL;
	std::string errMsg = getErrorMessage(errCode);
	size_t strLength = errMsg.length();
	*pMessageBuf = new char[strLength+1];
    memset(*pMessageBuf, '\0', strLength+1) ;
	inm_strncpy_s(*pMessageBuf,(strLength+1),errMsg.c_str(),strLength);
	return strLength;
}
void Inm_cleanUp(char **pBuff)
{
	if(*pBuff)
		delete []*pBuff;
	*pBuff = NULL;
}
void Inm_Exit()
{
	SignalApplicationQuit(true) ;
}
