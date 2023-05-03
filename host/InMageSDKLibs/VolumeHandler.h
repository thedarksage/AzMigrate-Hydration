#ifndef VOLUME__HANDLER__H
#define VOLUME__HANDLER__H

#include "Handler.h"
#include "confschema.h"
#include "confschemamgr.h"
#include "volumesobject.h"
#include "protectionpairobject.h"
#include "planobj.h"
#include "scenariodetails.h"
#include "pairsobj.h"

struct volumeResizeDetails
{
    SV_LONGLONG m_newVolumeRawSize ;
    SV_LONGLONG m_oldVolumeRawSize ;
    SV_LONGLONG m_newVolumeCapacity ;
    SV_LONGLONG m_oldVolumeCapacity ;
	std::string m_IsSparseFilesCreated;
    time_t m_changeTimeStamp ;
} ;

struct volumeProperties
{
	std::string GUID;
	std::string name;
	std::string fileSystemType;
	std::string capacity;
	std::string isSystemVolume;
	std::string isBootVolume;
	std::string freeSpace;
	std::string attachMode;
	std::string volumeLabel;
	std::string mountPoint;
	std::string rawSize;
	std::string writeCacheState;
	std::string deviceInUse;
	std::string VolumeType;
	std::string InterfaceType;
    std::map<time_t, volumeResizeDetails> volumeResizeHistory ;
};

class VolumeHandler :
	public Handler
{
	
	ConfSchema::ProtectionPairObject m_ProPairObj;
	ConfSchema::PlanObject m_PlanObj;
	ConfSchema::ScenraionDetailsObject m_SceDetailsObj;
	ConfSchema::PairsObject m_PairObject;
	
public:
	VolumeHandler(void);
	~VolumeHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
	INM_ERROR ListVolumes(FunctionInfo& request);
	INM_ERROR ListProtectableVolumes(FunctionInfo& request);
	INM_ERROR ListProtectedVolumes(FunctionInfo& request);
	INM_ERROR ListAvailableDrives(FunctionInfo& request) ;
};




#endif /* VOLUME__HANDLER__H */
