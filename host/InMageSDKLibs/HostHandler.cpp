#include "HostHandler.h"

HostHandler::HostHandler(void)
{
}

HostHandler::~HostHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
INM_ERROR HostHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_SUCCESS;
	//TODO: Write your logic here
	//Through unsupportted function handler exception.
	return Handler::updateErrorCode(errCode);
}
INM_ERROR HostHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}