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

#ifndef LOCAL_HOST__H
#define LOCAL_HOST__H

#include "synchronize.h"
#include "generichost.h"
#include "volumegroupsettings.h"

class Host :    public GenericHost
{
public:
	static Host& GetInstance();
    static bool Destroy();
	int Init();
    virtual const std::string GetHostName () const;
    virtual const std::string GetFQDN() const;
    virtual const std::string GetIPAddress () const;
    void GetIPAddressInAddrInfo(strset_t &ipsInAddrInfo, std::string &errMsg) const;
    const CpuInfos_t &GetCpuInfos() const;
	const unsigned long long &GetAvailableMemory() const;
    bool GetFreeMemory(unsigned long long& freeMemory) const;
    bool GetSystemUptimeInSec(unsigned long long& uptime) const;
	
private:
	bool SetCpuInfos() ;
	bool SetMemory() ;
    static  Lockable m_SyncLock;
    static Host* theHost;

	static CpuInfos_t m_CpuInfos;
	static unsigned long long m_Memory;

private:
    static void Reset();
	Host();
	virtual ~Host();

};

#endif

