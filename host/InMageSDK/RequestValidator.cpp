#include "RequestValidator.h"
#include "inmsdkutil.h"
#include <ace/File_Lock.h>
#include <ace/Guard_T.h>
#include "confengine/confschemareaderwriter.h"
#include "inmageex.h"
#include "cdputil.h"
#include "service.h"
#include "RepositoryHandler.h"
#include "apinames.h"
#include "APIController.h"

RequestValidator::RequestValidator(void)
{
	m_acl_enbled = true;
	m_mode = SIMULATION;
	fillAcltypes();
	fillAclPermisions();
	fillIdentities();
}
RequestValidator::RequestValidator(const char *XMLRequest, char *XMLResponse)
{
	m_acl_enbled = true;
	m_mode = SIMULATION;
	fillAcltypes();
	fillAclPermisions();
	fillIdentities();
}
RequestValidator::~RequestValidator(void)
{
}

INM_ERROR RequestValidator::initializeRequstInfo(const std::string & xmlRequstFile)
{
	INM_ERROR errCode = E_SUCCESS;
	//2. Build Request object 
	try {
		m_request.InitializeXmlRequestObject(xmlRequstFile);
	}catch (...)
	{
		//Exception while parsing xml file.
		errCode = E_FORMAT;
	}
	return errCode;
}

INM_ERROR RequestValidator::initializeRequstInfo(std::stringstream & xmlRequstStream)
{
	INM_ERROR errCode = E_SUCCESS;
	//2. Build Request object 
	try {
		m_request.InitializeXmlRequestObject(xmlRequstStream);
	}catch (...)
	{
		//Exception while parsing xml file.
		errCode = E_FORMAT;
	}
	return errCode;
}
INM_ERROR RequestValidator::generateResponse(const std::string & xmlResponseFile)
{
	INM_ERROR errCode = E_SUCCESS;
	//Construct/Update the response in XMLfile
	try{
		m_response.XmlResponseSave(xmlResponseFile);
	}catch(...)
	{
		//Exception while saving/writing response to xml file. No response XML will be generated.
		errCode = E_FORMAT;
	}
	return errCode;
}
INM_ERROR RequestValidator::generateResponse(std::stringstream & xmlResponseStream)
{
	INM_ERROR errCode = E_SUCCESS;
	//Construct/Update the response in XMLfile
	try{
		m_response.XmlResponseSave(xmlResponseStream);
	}catch(...)
	{
		//Exception while saving/writing response to xml file. No response XML will be generated.
		errCode = E_FORMAT;
	}
	return errCode;
}
void RequestValidator::updateResponse(INM_ERROR errCode)
{
	m_response.setResponseID(m_request.getRequestId());
	m_response.setResponseVersion(m_request.getVersion());
	m_response.setResponseReturnMeaasge(getErrorMessage(errCode));
	m_response.setResponseReturncode(getStrErrCode(errCode));
}

INM_ERROR RequestValidator::ValidateFunctionRequest()
{
	INMAGE_ERROR_CODE errCode = E_SUCCESS;
    
	errCode = authenticate();
	if(E_SUCCESS != errCode)
	{
		return processError(errCode);
	}
    CDPUtil::InitQuitEvent(true, true);
	std::list<FunctionInfo> listFuncReq = m_request.getRequestFunctionList();
	std::list<FunctionInfo>::const_iterator iterFunc = listFuncReq.begin();
	
	HandlerFactory hFactoryobj;
	while(iterFunc != listFuncReq.end())
	{
        FunctionInfo FunData = *iterFunc;
        INMAGE_ERROR_CODE funcErrCode = E_SUCCESS;
		ProcessAPI(FunData) ;
		funcErrCode = static_cast<INMAGE_ERROR_CODE>(boost::lexical_cast<int>(FunData.m_ReturnCode)) ;
		m_response.setFunctionList(FunData);
		iterFunc++;
	}
	updateResponse(errCode);
    CDPUtil::RequestQuit();
    CDPUtil::ClearQuitRequest();

	//8.Create Request/Error Log
	return errCode;
}

INM_ERROR RequestValidator::authenticate()
{
	INM_ERROR errCode = E_SUCCESS;

	//Temporary logic: 
	return errCode;//
	//eliminating the authentication as the authentication-logic is not decided.

	std::string id = m_request.getRequestId();
	std::string passPhrase = m_request.getHeaderInfo().m_AuthenticationObj.m_AccessSignature;
	std::string ppStored = getPP(id);
	if(!ppStored.empty())
	{
		//Compute the PP wiht id & passphrase
		std::string ppComputed = strToUpper(md5("TKL"+id+passPhrase+"BLL"));

		//Compar ppStored and ppComputed
		if(ppStored.compare(ppComputed) != 0)
		{
			//Authenticatin failure
			errCode = E_AUTHENTICATION;
		}
	}
	else
	{
		//ppStored empty means the ID is invalied.
		errCode = E_AUTHENTICATION;
	}

	return errCode;
}

INM_ERROR RequestValidator::authorize(Handler* pHandler)
{
	INM_ERROR errCode = E_SUCCESS;

	//Temporary logic: 
	return errCode;//
	//eliminating the authorization as the authorization-logic is not decided.

	std::string refID = strToUpper(md5(m_request.getRequestId()));
	std::string acl_type = getAclType(refID);
	if(isAclEnabled())
	{
		if(!acl_type.empty())
		{
			//Check for Universal authentication
			if(acl_type.compare("A") != 0)
			{
				if(!pHandler->hasAccess())
				{
					errCode = E_AUTHORIZATION;
				}
			}
		}
		else
		{
			//Invalied Id
			errCode = E_AUTHORIZATION;
		}
	}
	return errCode;
}

INM_ERROR RequestValidator::processFuncReqError(INM_ERROR errCode,Handler *pHandler,FunctionInfo &funcRequest)
{
	//FunctionInfo funcResponse;
	/*if(NULL != pHandler)
	{
		funcResponse = pHandler->getResponse();
	}*/
	funcRequest.m_ReturnCode = getStrErrCode(errCode);
	if(funcRequest.m_Message.empty())
		funcRequest.m_Message = getErrorMessage(errCode);
	//funcResponse.m_RequestFunctionName = funcRequest.m_RequestFunctionName;
	//funcResponse.m_FunReqInResponseObj.m_RequestFunctionName = funcRequest.m_RequestFunctionName;
	//funcResponse.m_FuntionId = funcRequest.m_FuntionId;
	//m_response.setFunctionList(funcResponse);
	
	//m_response.setFunctionList(funcRequest);
	return errCode;
}
INM_ERROR RequestValidator::processError(INM_ERROR errCode)
{
	updateResponse(errCode);
	return errCode;
}

void RequestValidator::fillAcltypes()
{
	m_acl_types.insert(std::make_pair("F82A8D0F02B0BAC25DCA1C4116A54DCE","A"));
	m_acl_types.insert(std::make_pair("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5","P"));
}

void RequestValidator::fillAclPermisions()
{
	m_acl_resource_permissions.insert(std::make_pair("F82A8D0F02B0BAC25DCA1C4116A54DCE","HVPLM"));
	m_acl_resource_permissions.insert(std::make_pair("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5","HVPLM"));
}

void RequestValidator::fillIdentities()
{
	m_identities.insert(std::make_pair("F82A8D0F02B0BAC25DCA1C4116A54DCE","D35055ED31AD5318A1F8C5202C2DF7AE"));
	m_identities.insert(std::make_pair("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5","9973E6A3CC13FB39F366A2984E89B706"));
}
std::string RequestValidator::getPP(const std::string& id)
{
	std::string idUpper = strToUpper(md5(id));
	std::map<std::string,std::string>::iterator iterPP = m_identities.find(idUpper);
	if(m_identities.end() == iterPP)
	{
		return "";
	}
	return iterPP->second;
}
std::string RequestValidator::getAclType(const std::string& id)
{
	std::map<std::string,std::string>::iterator iterAclType = m_acl_types.find(id);
	if(m_acl_types.end() == iterAclType)
	{
		return "";
	}
	return iterAclType->second;
}