#ifndef REQUEST__VALIDATOR__H
#define REQUEST__VALIDATOR__H

#include "HandlerFactory.h"
#include "InmXmlParser.h"
#include "InmsdkGlobals.h"

typedef enum _PROCESSDATA_MODE
{
	SIMULATION,
	PRODUCTION
}PROCESSDATA_MODE;

class RequestValidator
{
	XmlRequestValueObject m_request;
	XmlResponseValueObject m_response;

	PROCESSDATA_MODE m_mode;
	bool m_acl_enbled;

	std::map<std::string,std::string> m_identities;
	void fillIdentities();

	std::map<std::string,std::string> m_acl_types;
	void fillAcltypes();
	
	std::map<std::string,std::string> m_resources;
	void fillResources();
	
	std::map<std::string,std::string> m_acl_resource_permissions;
	void fillAclPermisions();

	INM_ERROR processError(INM_ERROR errCode);
	INM_ERROR processFuncReqError(INM_ERROR errCode,Handler *pHandler,FunctionInfo &funcRequest);

	INM_ERROR authenticate();
	INM_ERROR authorize(Handler* pHandler);
	INM_ERROR getResponse();
	void updateResponse(INM_ERROR errCode);
	PROCESSDATA_MODE getMode();
	//If the id is valied then it will returns the corresponding passphrase, other wise returns an empty string.
	std::string getPP(const std::string& id);
	bool isAclEnabled()
	{
		return m_acl_enbled;
	}
	std::string getAclType(const std::string& id);
	std::string getResourcePermissions(std::string id);
public:
	RequestValidator(void);
	~RequestValidator(void);
	RequestValidator(const char* XMLRequest,char* XMLResponse="");

	INM_ERROR initializeRequstInfo(const std::string & xmlRequstFile);
	INM_ERROR initializeRequstInfo(std::stringstream & xmlRequestStream);
	INM_ERROR generateResponse(const std::string & xmlResponseFile);
	INM_ERROR generateResponse(std::stringstream & xmlResponseStream);
	INM_ERROR ValidateFunctionRequest();
};

#endif /*Requestvalidator.h*/