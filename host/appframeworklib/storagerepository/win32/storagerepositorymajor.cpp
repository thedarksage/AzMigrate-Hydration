#include "storagerepositorymajor.h"
#include "storagerepository/storagerepository.h"

StorageRepositoryObjPtr StorageRepository::getSetupRepoObj(std::map<std::string, std::string>& properties, const std::string& outputFileName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	StorageRepositoryObjPtr SetupRepoObjPtr;
	if(properties.find("TYPE") != properties.end())
	{
		std::string setuRepType = properties.find("TYPE")->second;
		throw AppException("InValid Type");
	}
	else
	{
		throw AppException("InValid Type");
		DebugPrintf(SV_LOG_ERROR,"TYPE key is not available in the map.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return SetupRepoObjPtr;
}

StorageRepositoryObjPtr StorageRepository::getSetupRepoObjV2(const std::string& outputFile )
{
    StorageRepositoryObjPtr storageRepoPtr ;
    throw AppException("SetupRepositoryV2 Policy is valid for windows\n") ;
    return storageRepoPtr ;
}