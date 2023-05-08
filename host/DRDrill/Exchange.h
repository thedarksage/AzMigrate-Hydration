#ifndef __EXCHANGE_H
#define __EXCHANGE_H

#include "Common.h"
using namespace DrDrillNS;

#include "ExchangeCollector.h"

class Exchange : public ExchangeCollector
{
	public:	  
		Exchange(void);
		~Exchange(void);
		Exchange(const _tstring &srcHost,
					const _tstring &tgtHost,
					const _tstring &evsName,
					const _tstring &appName,
					bool &bdrdrill);

		bool CollectExchData();
		
};
#endif
