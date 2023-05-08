#ifndef ARRAYDISCOVERYANDVALIDATOR_H
#define ARRAYDISCOVERYANDVALIDATOR_H
#include "discovery/unix/discoveryandvalidator.h"
#include "applicationsettings.h"

class ArrayDiscoveryAndValidator : public DiscoveryAndValidator
{
    int Run(const std::string &, std::string &);
    void ReportLuns(bool shouldrUpdateCx);
    void RegisterCxWithArray();
    void DeregisterCxWithArray();
public:
    SVERROR PostDiscoveryInfoToCx();
    virtual SVERROR Discover(bool shouldUpdateCx) ;
    void DiscoverForArrayInfo(const std::string &) ;
    void ReportLunsForArrayInfo(const std::string &) ;
    SVERROR PerformReadinessCheckAtSource();
    SVERROR PerformReadinessCheckAtTarget();
    virtual SVERROR Process();
    
    int pcliStatus;
    std::string pcliFailReason;
    ArrayDiscoveryAndValidator(const ApplicationPolicy& policy);
} ;

#endif

