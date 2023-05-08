#include "stdafx.h"


#include "Common.h"
using namespace DrDrillNS;

#include <string>
#include "inaddr.h"
#include "winsock2.h"
#include "svtypes.h"
#include "svstatus.h"
#include "ExchangeCollector.h"


ExchangeCollector::ExchangeCollector(void)
{
}
ExchangeCollector::~ExchangeCollector(void)
{
}

ExchangeCollector::ExchangeCollector(const _tstring &srcHost,
											const _tstring &tgtHost,
											const _tstring &evsName,
											const _tstring &appName,
											bool &bdrdrill)
:ProductDataCollector(srcHost,tgtHost,
					evsName,appName,bdrdrill)											
{
	
}
bool ExchangeCollector::PerformAppValidation()
{
	bool result = false;
	return result;
}
bool ExchangeCollector::CollectAppData()
{
	bool result = false;
	std::list<ULONG>::const_iterator evsIPIter;

	//get evsName
	ex_evsName = m_evsName;
	std::cout<<"\nevsName = "<<ex_evsName;

	//get evsIP
	if(!ex_evsName.empty())
	{
		ex_evsIPList = GetIpAddress(ex_evsName);
		evsIPIter = ex_evsIPList.begin();
		while(evsIPIter != ex_evsIPList.end())
		{
			struct in_addr addr;
			addr.s_addr = (*evsIPIter);
			std::cout<<"\nEVS IP = "<<inet_ntoa(addr);
			evsIPIter++;
		}
	}

	//get Exchange Version
	if(!ex_evsName.empty())
	{
		std::cout<<"\nGetExchVersion(hostName) = "<<GetExchVersion(ex_evsName);		
	}
	else
	{
		std::cout<<"\nGetExchVersion(hostName) = "<<GetExchVersion(m_srcHostName);		
	}

	std::cout<<"\nExchange version = "<<ex_srcExchVersion;
	return result;
}
bool ExchangeCollector::GetExchVersion(const _tstring hostName)
{
	bool result = true;
	std::list<_tstring> attrValues;	
	std::list<_tstring>::const_iterator iter;
	
	if(	obj_adIntf.getAttrValueList(hostName, attrValues, "serialNumber", "(objectclass=msExchExchangeServer)", "") == SVS_OK )
	{
        iter = attrValues.begin();
		if(!iter->empty())
		{
			if (obj_adIntf.findPosIgnoreCase((*iter), "Version 6.5") == 0)
				ex_srcExchVersion = "Exchange2003";
			else if (obj_adIntf.findPosIgnoreCase((*iter), "Version 8") == 0)
				ex_srcExchVersion = "Exchange2007";
			else if(obj_adIntf.findPosIgnoreCase((*iter), "Version 14") == 0)
				ex_srcExchVersion = "Exchange2010";
		}		
	}
	else
		result = false;
	return result;
}

bool ExchangeCollector::GenerateAppReport()
{
	bool result = false;
	return result;
}


