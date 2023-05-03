#ifndef __SETUPREPO_GENERAL__H__
#define __SETUPREPO_GENERAL__H__
#include "appglobals.h"
#include <boost/shared_ptr.hpp>
#include "appexception.h"
#include "applicationsettings.h"


class StorageRepository;
typedef boost::shared_ptr<StorageRepository> StorageRepositoryObjPtr ;

class StorageRepository
{
public:
	std::string m_deviceNamePath;
	std::string m_outputFile;
	std::string m_statusMessage;
	std::string m_type;
	
	StorageRepository(const std::string& outputFile, std::string type)
	{
		m_outputFile = outputFile;
		m_statusMessage = "";
		m_type = type;
	}
	virtual SVERROR setupRepository() = 0;
	virtual SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) = 0 ;
	std::string getDeviceNamePath()
	{
		return m_deviceNamePath;
	}
	std::string getStatusMessage()
	{
		return m_statusMessage;
	}
	static StorageRepositoryObjPtr getSetupRepoObj(std::map<std::string, std::string>& properties, const std::string& outputFile );
	static StorageRepositoryObjPtr getSetupRepoObjV2(const std::string& outputFile ) ;
};



#endif



