#ifndef __INMAGESDK__H
#define __INMAGESDK__H

#include <sstream>
#include "InmsdkGlobals.h"

#ifdef INMAGESDK_EXPORTS
#define INMAGESDK_API extern "C" __declspec(dllexport)
#else
#define INMAGESDK_API extern "C" __declspec(dllimport)
#endif

typedef enum INM_ERROR INMAGE_ERROR_CODE;

#ifdef SV_WINDOWS
INMAGESDK_API INMAGE_ERROR_CODE processRequestWithFile(const char* xmlRequest,const char* xmlResponse);
INMAGESDK_API INMAGE_ERROR_CODE processRequestWithStream(std::stringstream &xmlRequeststream,std::stringstream &xmlResponseStream);
INMAGESDK_API INMAGE_ERROR_CODE processRequestWithSimpleCStream(const char* xmlRequest,char** xmlResponse);
INMAGESDK_API size_t ErrorMessage(INMAGE_ERROR_CODE errCode,char **pMessageBuf);
INMAGESDK_API void Inm_cleanUp(char **pBuf);
INMAGESDK_API void NoComInit() ;
INMAGESDK_API void Inm_Exit() ;
#else
INMAGE_ERROR_CODE processRequestWithFile(const char* xmlRequest,const char* xmlResponse);
INMAGE_ERROR_CODE processRequestWithStream(std::stringstream &xmlRequeststream,std::stringstream &xmlResponseStream);
INMAGE_ERROR_CODE processRequestWithSimpleCStream(const char* xmlRequest,char* xmlResponse,unsigned long* bufferSize);
size_t ErrorMessage(INMAGE_ERROR_CODE errCode,char **pMessageBuf);
void Inm_cleanUp(char **pBuf);
void Inm_Exit() ;
#endif

//
//Return the Error message. User should clean message buffer, using Inm_cleanUP(), explicitly.
//

#endif /* __INMAGESDK__H */
