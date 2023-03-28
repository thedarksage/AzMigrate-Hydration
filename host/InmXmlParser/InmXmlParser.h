#ifndef INM__XMLPARSER__H_
#define INM__XMLPARSER__H_

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "InmFunctionInfo.h"

struct Body
{
	std::list<FunctionInfo>m_FunctionsList;
};

struct XmlRequestGroups
{
   std::string m_RequestID;
   std::string m_Version;
   Header m_HeaderObj;
   Body m_ReqstbodyObj;
};

class XmlRequestValueObject
{
private:
	void ParseXmlRequestForLoad(boost::property_tree::ptree &pt);
	void ParseXmlRequestObjectForSave(boost::property_tree::ptree &RequestTree);
	
	void PopulateParamgrpsInfo(boost::property_tree::ptree::value_type &v,ParameterGroup &m_ParentParmgrpObj);
	void PopulateParamGrpsTree(const ParameterGroups_t& ParagmGrps,std::string &ParmGrpTreePath,boost::property_tree::ptree &ParmGrptree);

public:
	XmlRequestValueObject();
	~XmlRequestValueObject();

	XmlRequestGroups m_xmlRequestinfo;
	void InitializeXmlRequestObject(const std::string &filename);
	void InitializeXmlRequestObject(std::stringstream &XmlStream);
	void XmlRequestLoad(const std::string &filename);
	void XmlRequestLoad(std::stringstream &XmlStream);

	void XmlRequestSave(const std::string &filename);
	void XmlRequestSave(std::stringstream &XmlStream);
	
	Header getHeaderInfo() const;
	std::list<FunctionInfo> getRequestFunctionList() const;
	void setRequestFunctionList(FunctionInfo &m_Funreqinfo);
	//void setHeaderInfo();

	std::string getRequestId() const;
	std::string getVersion() const ;
	std::string getAuthenticationAccessKeyID() const;
	std::string getAuthenticationAccessSignature() const;
	std::map<std::string,ValueType> getAuthenticationParameterPair() const;
	std::map<std::string,ValueType> getHeaderParameterPair() const;
	ParameterGroups_t getHeaderParameterGroups() const;

	void setRequestId( std::string const& RequestId);
	void setVersion(std::string const& Version);
	void setAuthenticationAccessKeyID(std::string const& AccessKeyID);
	void setAuthenticationAccessSignature(std::string const& AccessSignature);
	void setAuthenticationParameterPair(std::string const& AuthParamName,ValueType& AuthParamValue);
	
	void setHeaderParameterPair(std::string const& HeaderParamName,ValueType& HeaderParamValue);
	void setHeaderParameterGroups(std::string const& parmGrpID,ParameterGroup& m_paramgrpobj);
};

// Response structures
struct XmlResponseGroups
{
  std::string m_Id;
  std::string m_ReturnCode;
  std::string m_Message;
  std::string m_version;
  Body m_ResponsebodyObj;
};

//Response Class
class XmlResponseValueObject
{
private:
	void ParseXmlResponseForLoad(boost::property_tree::ptree &pt);
	void ParseXmlResponseObjectForSave(boost::property_tree::ptree &ResponseTree);
	void PopulateParamgrpsInfo(boost::property_tree::ptree::value_type &v,ParameterGroup &m_ParentParmgrpObj);
	void PopulateParamGrpsTree(const ParameterGroups_t &ParagmGrps,std::string &ParmGrpTreePath,boost::property_tree::ptree &ParmGrptree);

public:
	XmlResponseValueObject();
	~XmlResponseValueObject();
	XmlResponseGroups m_xmlResponseInfo;

	void InitializeXmlResponseObject(const std::string &filename);
	void InitializeXmlResponseObject(std::stringstream &XmlStream);
	void XmlResponseLoad(const std::string &filename);
	void XmlResponseLoad(std::stringstream &XmlStream);

	void XmlResponseSave(const std::string &filename);
	void XmlResponseSave(std::stringstream &XmlStream);

	std::list<FunctionInfo> getFunctionList() const;
	void setFunctionList(FunctionInfo &m_FunctionInfoobj);

	std::string getResponseId() const;
	std::string getResponseVersion() const ;
	std::string getResponseReturncode() const;
	std::string getResponseReturnMeaasge() const;
 
	void setResponseID(std::string const& ResponseID);
	void setResponseVersion(std::string const& ResponseVersion);
	void setResponseReturncode(std::string const& ResponseReturncode);
	void setResponseReturnMeaasge(std::string const& ResponseReturnMeaasge);
};


#endif /* INM__XMLPARSER__H_ */

