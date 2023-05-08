#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>        // close(), write()
#include "svtypes.h"
#include "svtransport.h"
#include "hostagenthelpers_ported.h"
#include "portable.h"
#include "../fr_log/logger.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "curlwrapper.h"

size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
    register int realsize;
    INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
    MemoryStruct *mem = (MemoryStruct *)data;
  
    size_t memlen;
    INM_SAFE_ARITHMETIC(memlen = InmSafeInt<size_t>::Type(mem->size) + realsize + 1, INMAGE_EX(mem->size)(realsize))
    mem->memory = (char *)realloc(mem->memory, memlen);
    if (mem->memory) 
    {
        inm_memcpy_s(&(mem->memory[mem->size]),(realsize + 1),ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }
    return realsize;
}


#if 0
SVERROR 
PostToSVServer( const char *pszHost,
                const char *pszUrl,
                char *pszPostBuffer,
                SV_ULONG dwPostBufferLength )
{
    /* place holder function */
	return SVS_OK;
}
#endif

SVERROR 
PostToSVServer( const char *pszSVServerName,
	        SV_INT HttpPort,
                const char *pszPostURL,
                char *pchPostBuffer,
                SV_ULONG dwPostBufferLength )
{

    CURL *curl;
    CURLcode res;
    char* pszCompleteURL = NULL;

    DebugPrintf(SV_LOG_DEBUG,"[SVTansport::PostToServer()] Server = %s\n",pszSVServerName);
    DebugPrintf(SV_LOG_DEBUG,"[SVTansport::PostToServer()] Post URL = %s\n",pszPostURL);
    curl = curl_easy_init();
    if(curl)
    {
	   char sPort[10];
       memset(sPort,0,10);
	   inm_sprintf_s(sPort, ARRAYSIZE(sPort), "%d", HttpPort);
	   int totalSize;
       INM_SAFE_ARITHMETIC(totalSize = InmSafeInt<size_t>::Type(strlen(pszSVServerName))+strlen(pszPostURL)+ MAX_PORT_DIGITS + 2 /* +1 for : */, INMAGE_EX(strlen(pszSVServerName))(strlen(pszPostURL)))
       DebugPrintf(SV_LOG_DEBUG,"[SVTansport::PostToServer()] Total Size = %d\n",totalSize);
       pszCompleteURL = new (std::nothrow) char[totalSize];
       memset(pszCompleteURL,0,totalSize);
       inm_strncpy_s(pszCompleteURL,totalSize,pszSVServerName,strlen(pszSVServerName));
	   inm_strcat_s(pszCompleteURL,totalSize,":");
	   inm_strncat_s(pszCompleteURL,totalSize,sPort,strlen(sPort));
       inm_strncat_s(pszCompleteURL,totalSize,pszPostURL,strlen(pszPostURL));
       DebugPrintf(SV_LOG_DEBUG,"[SVTansport::PostToServer()] URL = %s\n",pszCompleteURL);

       /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */
        curl_easy_setopt(curl, CURLOPT_URL, pszCompleteURL);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pchPostBuffer);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* always cleanup */
        curl_easy_cleanup(curl);
        delete[] pszCompleteURL;
        return SVS_OK;
  }
  return SVE_FAIL;
}
size_t WriteMemoryCallbackFileReplication(void *ptr, size_t size, size_t nmemb, void *data)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	size_t realsize;
    INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
	MemoryStruct *mem = (MemoryStruct *)data;

    size_t memlen;
    INM_SAFE_ARITHMETIC(memlen = InmSafeInt<size_t>::Type(mem->size) + realsize + 1, INMAGE_EX(mem->size)(realsize))
	mem->memory = (char *)realloc(mem->memory, memlen);

	if (mem->memory) {
		inm_memcpy_s(&(mem->memory[mem->size]),(realsize + 1) ,ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return realsize;
}

SVERROR postToCx(const char* pszHost, 
     SV_INT Port, 
     const char* pszUrl, 
     const char* pszBuffer,
     char** ppszBuffer,bool useSecure, 
     SV_ULONG size, 
     SV_ULONG retryCount
     ) 
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);


    SVERROR rc = SVE_FAIL ;
   
	int retries = 0;
    MemoryStruct chunk = {0};

    if ( Port > 65535 || Port < 1 )
    {
        DebugPrintf(SV_LOG_ERROR, "The Port is out of range.. Please choose port number between range 1-65535 on which the CX server is listening\n");  
    }
    else
    {

		DebugPrintf(SV_LOG_DEBUG,"Server = %s\n",pszHost);
		DebugPrintf(SV_LOG_DEBUG,"Port = %d\n",Port);
		DebugPrintf(SV_LOG_DEBUG,"Secure = %d\n",useSecure);
		DebugPrintf(SV_LOG_DEBUG,"PHP URL = %s\n",pszUrl);

		std::string result;
		CurlOptions options(pszHost,Port, pszUrl ,useSecure);
		options.writeCallback( static_cast<void *>( &chunk ),WriteMemoryCallbackFileReplication);
		options.lowSpeedLimit(10);
		options.lowSpeedTime(120);
		options.transferTimeout(CX_TIMEOUT);

		while ( retries < retryCount && rc == SVE_FAIL )
		{

			DebugPrintf(SV_LOG_DEBUG, "Attemt %d: PostToServer ...\n", retries );

			chunk.memory = NULL; 
			chunk.size = 0; /* no data at this point */

			try {
				CurlWrapper cw;
				cw.post(options, pszBuffer);
				if( chunk.size > 0)
				{

					if(ppszBuffer != NULL) 
					{
						*ppszBuffer = chunk.memory;
					}
					else
					{
						free( chunk.memory ) ;
					}
				}
				rc = SVS_OK;
			} catch(ErrorException& exception )
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED : %s with error %s\n",__FUNCTION__, exception.what());
				rc = SVE_FAIL;
			} 

            retries++;

#ifdef SV_WINDOWS
            Sleep(retries * 5000 );
#else
            sleep(retries * 5 );
#endif 
        }
 
        if( rc == SVE_FAIL )
        {
            DebugPrintf(SV_LOG_ERROR, "PostToServer server failed even after %d attempts, This may cause file replication jobs to fail...\n", retries);  
        }

    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( rc );
}
