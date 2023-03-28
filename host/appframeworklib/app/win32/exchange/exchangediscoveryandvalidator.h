#ifndef EXCHANGEDISCOVERYANDVALIDATOR_H
#define EXCHANGEDISCOVERYANDVALIDATOR_H
#include "discoveryandvalidator.h"
#include "exchangeapplication.h"

class ExchangeDiscoveryAndValidator : public DiscoveryAndValidator
{
    SVERROR ConvertToCxFormat(const std::list<std::string>& instancesList) ;
    SVERROR Discover(bool shouldUpdateCx) ;
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;
	ExchangeApplicationPtr m_ExchangeAppPtr ;
    void convertToExchangeDiscoveryInfo(const ExchAppVersionDiscInfo& exchDiscoveryInfo, 
                                                                    const ExchangeMetaData& exchMetadata, 
                                                                    ExchangeDiscoveryInfo exchdiscoveryInfo
										) ;
    void convertToExchange2010DiscoveryInfo(const ExchAppVersionDiscInfo& exchDiscoveryInfo, 
                                                                    const Exchange2k10MetaData& exch2010Metadata, 
                                                                    ExchangeDiscoveryInfo exchdiscoveryInfo
										) ;
    bool IsItActiveNode() ;

public:
    ExchangeDiscoveryAndValidator(const ApplicationPolicy& policies) ;
} ;

#endif
