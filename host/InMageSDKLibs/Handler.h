#ifndef HANDLER__H
#define HANDLER__H

#include "configurator2.h"
#include "InmFunctionInfo.h"
#include "InmsdkGlobals.h"
#include "serializationtype.h"
#include <boost/lexical_cast.hpp>
#include "../confschema/confschema.h"
#include "inmsdkexception.h"
#include "volumegroupsettings.h"
#include "localconfigurator.h"

enum eVolumeResize
{
    VOLUME_SIZE_NORMAL,
    VOLUME_RESIZE_PENDING,
    VOLUME_RESIZE_SHRINK,
    VOLUME_RESIZE_NOFREESPACE,
    VOLUME_RESIZE_FAILED,
    VOLUME_RESIZE_COMPLETED
} ;

struct HandlerInfo{
    bool access;
    bool funcSupported;
    std::string authId;
    std::string funcReqId;
    std::string functionName;
    std::string funcIncludes;
    FUNCTION_HANDLER funcHandler;
    HandlerInfo()
    {
        access = false;
        funcSupported = false;
        authId = "";
        funcReqId = "";
        functionName = "";
        funcIncludes = "";
        funcHandler = HANDLER_STUB;
    }
};

class Handler
{
protected:
    HandlerInfo m_handler;
    FunctionInfo m_ResponseData;
    //XmlResponseValueObject m_stubResponse;//Used only for processFromStub
    std::string m_errMessage;

    Configurator* m_vxConfigurator;
    bool m_bconfig;
public:
    Handler()
    {
        m_errMessage = "";

        m_vxConfigurator = NULL;
        m_bconfig = false;
    }
	virtual ~Handler() {};
    void setHandlerInfo(HandlerInfo& hInfo);

    //For Configurator initilization
    INM_ERROR initConfig();

    //Returns true is the requested function is supported, otherwise false.
    bool hasSupport()
    {
        return m_handler.funcSupported;
    }
    //Returns true if the requested function has accese permission, otherwise false.
    bool hasAccess()
    {
        return m_handler.access;
    }
    //Calls the acual API requested
    virtual INM_ERROR process(FunctionInfo& request) = 0 ;

    virtual INM_ERROR updateErrorCode(INM_ERROR errCode);
    //Validates the arguments/paramerts of the function
    virtual INM_ERROR validate(FunctionInfo& request);
    //Return the response data
    virtual FunctionInfo getResponse()
    {
        return m_ResponseData;
    }
    FUNCTION_HANDLER getHandlerType()
    {
        return m_handler.funcHandler;
    }
    virtual INM_ERROR processFromStub(const std::string& stubFile,FunctionInfo& request);

    //to get a value of attribute from FunctionInfo ParameterGroup( parent grps only not child)

};

bool GetHostDetailsToRepositoryConf(std::list<std::map<std::string, std::string> >& hostInfoList) ;
bool AddHostDetailsToRepositoryConf(const std::string& repoPath="") ;
bool IsRepositoryInIExists();
bool isHostNameChanged (std::string &currentName) ;
bool isRepoPathChanged() ;
void GetSparseFileVolPackMap(const LocalConfigurator& lc, std::map<std::string, std::string>& sparseFileDeviceMap) ;
#endif /* __HANDLER__H */

