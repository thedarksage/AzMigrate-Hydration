#ifndef HANDLER__FACTORY__H
#define HANDLER__FACTORY__H

#include "Handler.h"
#include <boost/shared_ptr.hpp>

class HandlerFactory
{
	// TODO: open this once unix compilation is done
	static bool isHostDiscoveryHandler(HandlerInfo& hInfo);
    static bool isSettingsHandler(HandlerInfo& hInfo);
    static bool isResyncHandler(HandlerInfo& hInfo);	
    static bool isSourceHandler(HandlerInfo& hInfo);	
    static bool isTargetHandler(HandlerInfo& hInfo);
    static bool isAppSettingsHandler(HandlerInfo& hInfo);
    static bool isPolicyHandler(HandlerInfo& hInfo);
    static bool isPairCreationHandler(HandlerInfo& hInfo);
    static bool isSnapshothandler(HandlerInfo& hInfo);
	static bool isHostHandler(HandlerInfo& hInfo);
	static bool isVolumeHandler(HandlerInfo& hInfo);
	static bool isVmAssociationHandler(HandlerInfo& hInfo);
	static bool isRepositoryHandler(HandlerInfo& hInfo);
	static bool isMonitorHandler(HandlerInfo& hInfo);
	static bool isProtectionHandler(HandlerInfo& hInfo);
	static bool isLicencingHandler(HandlerInfo& hInfo);
	static bool isRecoveryHandler(HandlerInfo& hInfo);
	//Based on id and function name the Handler will will constuct the m_handler map
public:
	static INM_ERROR findHandler(HandlerInfo& hInfo);
	HandlerFactory(void);
	~HandlerFactory(void);
	//Returns the Handler corresponding to the requested function.
	static boost::shared_ptr<Handler> getHandler(const std::string& id, //Authenticatin ID
		const std::string& functionName,		//Name of the requested API
		const std::string& funcReqId,			//Function requst ID
		const std::string& funcIncludes,		//Includes
		INM_ERROR& );					//Pointer to the Handler
	
	static boost::shared_ptr<Handler> getHandler(const std::string& functionName,		//Name of the requested API
		const std::string& funcReqId,			//Function requst ID
		INM_ERROR& error);					//Pointer to the Handler
	
	//If the handler is already created the return value is existingHandler, otherwise a pointer to StubHandler.
	static boost::shared_ptr<Handler> getStubHandler(INM_ERROR& errCode );
};



#endif /* HANDLER__FACTORY__H */

