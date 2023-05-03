#include "VmAssociationHandler.h"

VmAssociationHandler::VmAssociationHandler(void)
{
}

VmAssociationHandler::~VmAssociationHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR VmAssociationHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_SUCCESS;
	//TODO: Write logic hare
	//Through unsupportted function handler exception.
	return Handler::updateErrorCode(errCode);
}
INM_ERROR VmAssociationHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}