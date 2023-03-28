#ifndef FSDISCOVERYANDVALIDATOR_H
#define FSDISCOVERYANDVALIDATOR_H

#include "discoveryandvalidator.h"
#include "fileserverapplication.h"

class FSDiscoveryAndValidator : public DiscoveryAndValidator
{
    SVERROR Discover(bool shouldUpdateCx) ;
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;
	void PostDiscoveryInfoToCx( std::list<std::string>& networkNameList);
	SVERROR fillFileServerDiscoveryData( std::string& networkName, FileServerInfo& fsInfo);
	FileServerAplicationPtr m_FSApp ;
    bool IsItActiveNode() ;
public:
    FSDiscoveryAndValidator(const ApplicationPolicy& policies) ;
} ;

#endif