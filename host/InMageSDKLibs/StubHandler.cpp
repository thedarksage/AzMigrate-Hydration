#include "StubHandler.h"

StubHandler::StubHandler(void)
{
	m_errCode = E_UNKNOWN_FUNCTION;
}

StubHandler::~StubHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR StubHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	//TODO: Write logic hare
	return Handler::updateErrorCode(m_errCode);
}
INM_ERROR StubHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}