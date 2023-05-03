#ifndef __EXCHANGE_COLLECTOR_H
#define __EXCHANGE_COLLECTOR_H
#include "Common.h"
using namespace DrDrillNS;

#include <map>
#include "ProductDataCollector.h"

class ExchangeCollector : public ProductDataCollector
{
    public:
		ExchangeCollector(void);
		~ExchangeCollector(void);
		ExchangeCollector(const _tstring &srcHost,
							const _tstring &tgtHost,
							const _tstring &evsName,
							const _tstring &appName,
							bool &bdrdrill);
		//Exchange Data collected from source in pre-script
		_tstring ex_srcExchVersion;
		_tstring ex_evsName;		
		std::list<ULONG> ex_evsIPList;
		std::list<_tstring> ex_tagVolList;
		std::map<_tstring,_tstring> ex_sgNamePath;
		std::map<_tstring,std::map<_tstring,_tstring>> ex_sgDbNamePath;
		
		//Exchange Data collected from target in post-script
		_tstring tgtExchVersion;

		bool PerformAppValidation();
		bool CollectAppData();
		bool GenerateAppReport();
 
		bool GetExchVersion(const _tstring hostName);
		bool GetExchConfiguration(const _tstring hostName);

		ProductDataCollector obj_pdtDataCollector;
		
				
		//MsExchangeCollector& GetExchangeData();

	protected:
			
    
    private:
   
    //Data Members
    
};
#endif