#ifndef __DISCOVERY__FACTORY__H
#define __DISCOVERY__FACTORY__H

#include <boost/shared_ptr.hpp>
#include "app/application.h"
#include "appglobals.h"
#include "discoveryandvalidator.h"
class DiscoveryFactory
{
public:
    static ApplicationPtr getApplication(InmProtectedAppType) ;
    static std::list<DiscoveryAndValidatorPtr> getDiscoveryAndValidator(SV_ULONG policyId, const std::set<std::string>& selectedApplicationNameSet) ;
} ;

#endif