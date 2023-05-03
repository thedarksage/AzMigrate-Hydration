// TODO: 2735203 remove unwanted includes
#include <string>
#include "error.h"
#include "host.h"
#include "logger.h"
#include "inmsafecapis.h"

extern std::string ValidateAndGetIPAddress(const std::string &ipAddress, std::string &errMsg);

void Host::GetIPAddressInAddrInfo(strset_t &ipsInAddrInfo, std::string &errMsg) const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    
    std::string sHostName = GetHostName();
    
    AutoLock CreateGuard(m_SyncLock); // Add the lock after GetHostName since there is a lock inside GetHostName too.
    
	if ( !sHostName.empty() )
	{
        char szTemp[MAXIPV4ADDRESSLEN + 1];
        memset((void*)szTemp, 0, sizeof(szTemp));
        
		struct addrinfo *paddInfo = NULL;
		int retv = getaddrinfo(sHostName.c_str(), NULL, NULL, &paddInfo);
		if( 0 == retv )
		{
			struct sockaddr_in *psockaddr_in = NULL;
            for (addrinfo *ptmp = paddInfo; ptmp != NULL; ptmp = ptmp->ai_next)
			{
                if (ptmp->ai_family == AF_INET)
                {
                    psockaddr_in = (struct sockaddr_in *) ptmp->ai_addr; 
                    
                    memset((void*)szTemp, 0, sizeof(szTemp));
                    inm_strcpy_s(szTemp, ARRAYSIZE(szTemp), inet_ntoa(psockaddr_in->sin_addr));
                    
                    std::string err;
                    std::string ipAddr = ValidateAndGetIPAddress(szTemp, err);
                    if (!ipAddr.empty())
                    {
                        ipsInAddrInfo.insert(ipAddr);
                    }
                    else
                    {
                        errMsg += err;
                        errMsg += ", ";
                    }
                }
            }
            if ( NULL != paddInfo )
            {
                freeaddrinfo(paddInfo);
            }
		}
		else
        {
			const char* err = gai_strerror(retv);
            std::stringstream sserr;
            sserr << "getaddrinfo failed. Error: ";
            sserr << retv;
            sserr << " ";
            sserr << err;
            errMsg += sserr.str();
            errMsg += ", ";
        }
	}
    else
    {
        errMsg += "HostName is blank.";
    }
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return;
}


/*
 * FUNCTION NAME : GetHostName
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const std::string Host::GetHostName() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	AutoLock CreateGuard(m_SyncLock);
	std::string sHostName = "";
	char szTemp[MAXHOSTNAMELEN];
	memset((void*)szTemp, 0, MAXHOSTNAMELEN);
    if ( 0 == ACE_OS::hostname(szTemp,sizeof(szTemp) ) )
	{
		sHostName = szTemp;
	}
	else
	{
		DebugPrintf( SV_LOG_ERROR,
					 "FAILED: Couldn't determine local host name. %s. @LINE %d in FILE %s \n",
					 Error::Msg().c_str(),
					 LINE_NO,
					 FILE_NAME);
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return sHostName;
}

const std::string Host::GetFQDN() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AutoLock CreateGuard(m_SyncLock);
    std::string sHostName = GetHostName();
    if (sHostName.empty())
    {
        return sHostName;
    }

    std::string sFqdn;
    struct hostent* pHostEnt = NULL;
    if (NULL != (pHostEnt = ACE_OS::gethostbyname(sHostName.c_str())))
    {
        sFqdn = pHostEnt->h_name;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,
            "FAILED: Couldn't determine local host FQDN. %s. \n",
            Error::Msg().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with FQDN %s\n", FUNCTION_NAME, sFqdn.c_str());
    return sFqdn;
}

