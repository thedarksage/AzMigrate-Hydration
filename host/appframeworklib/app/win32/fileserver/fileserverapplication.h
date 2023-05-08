#ifndef _FILESERVER_APPLICATION_
#define _FILESERVER_APPLICATION_

#include "app/application.h"
#include "fileserverdiscovery.h"
#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include "ruleengine/validator.h"
class FileServerAplication;

typedef boost::shared_ptr<FileServerAplication> FileServerAplicationPtr ;


class FileServerAplication : public Application
{
public:
	boost::shared_ptr<FileServerDiscoveryImpl> m_fileServerDiscoveryImplPtr ;
	std::list<InmService> m_services ;
	std::string m_registryVersion;
    std::map<std::string, FileServerInstanceDiscoveryData> m_fileShareInstanceDiscoveryMap ;
    std::map<std::string, FileServerInstanceMetaData> m_fileShareInstanceMetaDataMap	;
	FileServerAplication();
	SVERROR discoverServices() ;
	SVERROR getNetworkNameList( std::list<std::string>& );
	SVERROR getActiveInstanceNameList( std::list<std::string>& );
	SVERROR getDiscoveredDataByInstance( const std::string& instanceName, FileServerInstanceDiscoveryData& );
	SVERROR getDiscoveredMetaDataByInstance( const std::string& instanceName, FileServerInstanceMetaData& );
	void dumpSharedResourceInfo();
	std::string getSummary( std::list<ValidatorPtr> list) ;
	virtual SVERROR discoverApplication(); 
    virtual bool isInstalled() ;
	virtual SVERROR discoverMetaData(){ return SVS_OK; }
    void clean();
};

#endif