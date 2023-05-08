#ifndef __EXCHANGE_VALIDATOR_H
#define __EXCHANGE_VALIDATOR_H

#include <windef.h>
#include "ExchangeCollector.h"

class ExchangeValidator : public ExchangeCollector
{
	public:
		ExchangeValidator(void);
		~ExchangeValidator(void);

		//Exchange data validation done on source in pre-script
		bool  v_nestedMntRule;
		bool  v_verifyTagIssue;

		//Exchange data validation done on target in post-script
		bool  v_hostName;
		bool  v_dataonSysDrive;
		bool  v_lcrEnabled;
		bool  v_ccrEnabled;
		bool  v_scrEnabled;
		bool  v_volCapacityCheck;
		bool  v_exVersionMatch;
		bool  v_adminGpMatch;
		bool  v_domainMatch;
		bool  v_dnsMatch;
		bool  v_adMatch;
		
		//Offline Validations
		//bool PerformOfflineValidations(MsExchangeCollector exchangeCollector);

		//Online Validations
		//bool PerformOnlineValidations(MsExchangeCollector exchangeCollector);

	private:
		ULONG CalculateNoOfLogs();
		bool SpawnThreads();//Required number is equal to number of logs
		bool AggregateResults();
		int InvokeValidationScript();

		//Add Exchange Specific Validations here
		bool ValidateMountPointRule();
		bool ValidateVolumeConsistency();
		bool ValidateExchangeDataOnSysDrive();


  };
#endif