#ifndef __BULKPROTECTION_AND_VALIDATOR__
#define __BULKPROTECTION_AND_VALIDATOR__

#include "discoveryandvalidator.h"

class BulkProtectionAndValidator : public DiscoveryAndValidator
{
	//virtual SVERROR Discover(bool shouldUpdateCx) = 0;
	SVERROR Discover(bool shouldUpdateCx){return SVS_OK;}
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;

public:
    BulkProtectionAndValidator(const ApplicationPolicy& policies) ;	

};

#endif
