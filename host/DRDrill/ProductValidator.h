#ifndef __PRODUCT_VALIDATOR_H
#define __PRODUCT_VALIDATOR_H
#include "Common.h"
using namespace DrDrillNS;

#include "ProductDataCollector.h"

class ProtectionValidator : public ProductDataCollector
{
	public:
		ProtectionValidator(void);
		~ProtectionValidator(void);

		
		void GetMaxValidatorThreads();//Read from DRScout.conf file
    
    private:
    //Add common Validation Results here
};
#endif