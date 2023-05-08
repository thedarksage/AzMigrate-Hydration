/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : basicio.cpp
 *
 * Description: 
 */
#include <cassert>
#include <string>

#include "ace/OS_NS_netdb.h"
#include "ace/OS_NS_unistd.h"
#include "entity.h"
#include "portable.h"

#include "synchronize.h"
#include "error.h"

#include "generichost.h"
#include "host.h"
#include "inmsafecapis.h"

extern bool GetIPAddressSetFromNicInfo(strset_t &ips, std::string &errMsg);


Host* Host::theHost = NULL;
Lockable Host::m_SyncLock;
CpuInfos_t Host::m_CpuInfos;
unsigned long long Host::m_Memory;

/*
 * FUNCTION NAME : Host
 *
 * DESCRIPTION : constructor
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
Host::Host()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : ~Host
 *
 * DESCRIPTION : Destructor
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
Host::~Host()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME : Init
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
int Host::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_bInitialized = true;
    int iStatus = SV_SUCCESS;

    if (!SetCpuInfos())
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR, "could not set cpu info from host\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "cpuinfos:\n");
    for (CpuInfosIter_t it = m_CpuInfos.begin(); it != m_CpuInfos.end(); it++)
    {
        DebugPrintf(SV_LOG_DEBUG, "cpu id: %s\n", it->first.c_str());
        Object &cpuinfo = it->second;
        cpuinfo.Print();
    }
 
	if (!SetMemory())
	{
		iStatus = SV_FAILURE;
		DebugPrintf(SV_LOG_ERROR, "could not set memory available host\n");
	}
    DebugPrintf(SV_LOG_DEBUG, "host has " ULLSPEC " bytes of memory\n", m_Memory);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return iStatus;
}

/*
 * FUNCTION NAME : Reset
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
void Host::Reset()
{
    m_CpuInfos.clear();
    m_Memory = 0;
}

/*
 * FUNCTION NAME : Destroy
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
bool Host::Destroy()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    AutoLock CreateGuard(m_SyncLock);
    if ( NULL != theHost)
    {
        delete theHost;
        theHost = NULL;
        Reset();
    }

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return true;
}

/*
 * FUNCTION NAME : GetInstance
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
Host& Host::GetInstance()
{
    if (NULL == theHost)
    {
        AutoLock CreateGuard(m_SyncLock);
        if (NULL == theHost)
        {
            theHost = new Host;
            theHost->Init();
        }
    }
	return *theHost;
}


const CpuInfos_t & Host::GetCpuInfos() const
{
    return m_CpuInfos;
}

const unsigned long long & Host::GetAvailableMemory() const
{
	return m_Memory;
}

const std::string Host::GetIPAddress() const
{
    std::string errMsg; 
    strset_t ipsFromGetAddrInfo;
    GetIPAddressInAddrInfo(ipsFromGetAddrInfo, errMsg);

    AutoLock CreateGuard(m_SyncLock); // Add the lock after GetIPAddressInAddrInfo since there is a lock inside GetIPAddressInAddrInfo too.
    std::string sIPAddress;

    strset_t ipsInNicinfo;
    if (GetIPAddressSetFromNicInfo(ipsInNicinfo, errMsg))
    {
        // Select first in ipsFromGetAddrInfo present in ipsInNicinfo
        // If no match found select frist in ipsInNicinfo
        strset_t::const_iterator ipsItr = ipsFromGetAddrInfo.begin();
        if (ipsItr != ipsFromGetAddrInfo.end())
        {
            for (ipsItr; ipsItr != ipsFromGetAddrInfo.end(); ipsItr++)
            {
                strset_t::const_iterator ipsInNicinfoItr = ipsInNicinfo.find(*ipsItr);
                if (ipsInNicinfoItr != ipsInNicinfo.end())
                {
                    sIPAddress = *ipsInNicinfoItr;
                    break;
                }
                else
                {
                    continue;
                }
            }
            if (sIPAddress.empty())
            {
                sIPAddress = *(ipsInNicinfo.begin());
            }
        }
        else
        {
            sIPAddress = *(ipsInNicinfo.begin());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
        return std::string();
    }

    return sIPAddress;
}

