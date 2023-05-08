#ifndef HOST__DISCOVERY__HANDLER__H_
#define HOST__DISCOVERY__HANDLER__H_

#include "Handler.h"
#include "confschema.h"
#include "volumesobject.h"
#include "inmapihandlerdefs.h"
#include "repositoriesobject.h"
#include "protectionpairobject.h"
typedef enum eHostRegisterType
{
    StaticHostRegister,
    DynamicHostRegister
} HostRegisterType_t;

class HostDiscoveryHandler :
	public Handler
{
public:

	HostDiscoveryHandler(void);
	~HostDiscoveryHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    /* TODO: why are these publics ? */
    /*       compare ignore case through 
     *       c++. not stricmp */
    /*
	INM_ERROR RegisterHostDynamicInfo(FunctionInfo &f);
	INM_ERROR RegisterHostStaticInfo(FunctionInfo &f);
    */
    INM_ERROR RegisterHost(FunctionInfo &f, const HostRegisterType_t regtype);
    static int count;
private:
	bool registrationAllowed() ;
    void FillAppToSchemaAttrs(ConfSchema::VolumesObject &vobj, AppToSchemaAttrs_t &apptoschemaattrs, 
                              const HostRegisterType_t regtype);
    void SendAlertIfNewVolume(ConfSchema::Object *pobject, const ParameterGroupsIter_t iter) ;
    void UpdateVolumesRelation(ParameterGroupsIter_t &voliter, ConfSchema::Group &volumesgroup, 
                               AppToSchemaAttrs_t &apptoschemaattrs,std::vector<std::string> &obtainedVolList, const HostRegisterType_t regtype);
    void UpdateVolumeHistory(ConfSchema::Object *pobject, const ParameterGroupsIter_t iter) ;

    void UpdateVolumeGroupWithNewVolumeList(ConfSchema::Group &volumesGroup,std::vector<std::string> &obtainedVolumeList);  
    void ReadVolumesSchemaToGetVolumeSummary(FunctionInfo& f);
    void GetVolumesSummary(ConfSchema::Group & volumesGroup,FunctionInfo &f);
    void trimSlashesForNames(ConfSchema::Object &o);
	void PopulateRepositoies(ConfSchema::Object& Pobj,std::string& replocation,ConfSchema::Reference& Ref);
    void UpdateAgentConfiguration(FunctionInfo &f) ;
    void UpdateRepositoryDetails(std::string& hostname) ;
};


#endif /* HOST__DISCOVERY__HANDLER__H_ */
