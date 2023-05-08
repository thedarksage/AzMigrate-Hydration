
#include "portablehelpers.h"
#include "InmXmlParser.h"
#include "InmXmlGlobals.h"
#include "inmstrcmp.h"

XmlRequestValueObject::XmlRequestValueObject()
{

}
XmlRequestValueObject::~XmlRequestValueObject()
{

}

XmlResponseValueObject::XmlResponseValueObject()
{

}
XmlResponseValueObject::~XmlResponseValueObject()
{

}

void XmlRequestValueObject::InitializeXmlRequestObject(const std::string &filename)
{
	try
	{
		XmlRequestLoad(filename);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error while reading in xml file :"<<ex.what()<<std::endl;
		throw ex;
	}
}
void XmlRequestValueObject::InitializeXmlRequestObject(std::stringstream &XmlStream)
{
	try
	{
		XmlRequestLoad(XmlStream);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error while reading in xml file :"<<ex.what()<<std::endl;
		throw ex;
	}
	
}
void XmlRequestValueObject::XmlRequestLoad(const std::string &filename)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(filename, pt);
		ParseXmlRequestForLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
	/*catch(boost::property_tree::xml_parser::xml_parser_error ex)
	{
		std::cout<<"Error:"<<ex.what()<<std::endl;
	}*/
}
void XmlRequestValueObject::XmlRequestLoad(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(XmlStream, pt);
		ParseXmlRequestForLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}

void XmlRequestValueObject::ParseXmlRequestForLoad(boost::property_tree::ptree &pt)
{
	using boost::property_tree::ptree;

	BOOST_FOREACH(ptree::value_type &v, pt.get_child(REQUESTTAG".<"XMLATTR">"))
	{
		if(!std::string(ID).compare(v.first.data()))
		{
			m_xmlRequestinfo.m_RequestID = v.second.data();
		}
		if(!std::string(VERSION).compare(v.first.data()))
		{
			m_xmlRequestinfo.m_Version = v.second.data();
		}
	}

	BOOST_FOREACH(ptree::value_type &v, pt.get_child(HEADER_PATH))
    {
  		if(InmStrCmp<NoCaseCharCmp>(v.first.data(),AUTHENTICATETAG) == 0)
		{
			BOOST_FOREACH(ptree::value_type &v1, v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),ACCESSKEYID) == 0)
				{
					m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessKeyID = v1.second.data();
				}
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),ACCESSSIGNATURE) == 0)
				{
					m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessSignature = v1.second.data();
				}
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETERTAG)==0)
				{
					ValueType vtype;
					std::string ParamName,ParamValue;
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
									ParamName = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),VALUE) == 0)
									ParamValue = v3.second.data();
							}
						}
					}
				 
				 vtype.value = ParamValue;
				 m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AuthenticationParamsPair.insert(std::make_pair(ParamName,vtype));
				}
			}
		}
		if(InmStrCmp<NoCaseCharCmp>(v.first.data(),PARAMETER_GROUPTAG) == 0)
		{	
			ParameterGroup m_ParentParmgrpObj;
			std::string ParmgrpIdName;
			BOOST_FOREACH(ptree::value_type &v1,v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),"<xmlattr>") == 0)
				{	
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ID) == 0)
						ParmgrpIdName = v2.second.data();
					}
				}
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETERTAG) == 0)
				{
					ValueType vtype;
					std::string ParamName,ParamValue;
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
									ParamName = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),VALUE) == 0)
									ParamValue = v3.second.data();
							}
						}
					}
					vtype.value = ParamValue;
					//m_ParentParmgrpObj.m_ChildParameterPair.insert(std::make_pair(ParamName,vtype));
					m_ParentParmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));
				}
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETER_GROUPTAG) == 0)
				{
					PopulateParamgrpsInfo(v1,m_ParentParmgrpObj);
				}
			}
			//m_xmlRequestinfo.m_HeaderObj.m_HeaderPararmGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
			m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
		}
		if(!std::string(PARAMETERTAG).compare(v.first.data()))
		{
			ValueType vtype;
			std::string ParamName,ParamValue;
			BOOST_FOREACH(ptree::value_type &v1,v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),NAME) == 0)
							ParamName = v2.second.data();
						else if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),VALUE) == 0)
							ParamValue = v2.second.data();
					}
				}
			}
			vtype.value = ParamValue;
			//m_xmlRequestinfo.m_HeaderObj.m_HeaderParameterPair.insert(std::make_pair(ParamName,ParamValue));
			m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params.insert(std::make_pair(ParamName,vtype));
		}
	}
	
	BOOST_FOREACH(ptree::value_type &v, pt.get_child(REQUESTTAG))
	{
		if(InmStrCmp<NoCaseCharCmp>(v.first.data(),BODY) == 0)
		{
			BOOST_FOREACH(ptree::value_type &v1,v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),FUNCTION_REQUEST_TAG) == 0)
				{
					//FunctionRequestInfo m_FunRequestInfoObj;
					FunctionInfo m_FunRequestInfoObj;
					
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
						{
							std::string ReqFunName,ReFunID,ReqInclude;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
									ReqFunName = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ID) == 0)
									ReFunID = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),INCLUDE) == 0)
									ReqInclude = v3.second.data();
							}
							m_FunRequestInfoObj.m_FuntionId = ReFunID;
							m_FunRequestInfoObj.m_ReqstInclude  = ReqInclude;
							m_FunRequestInfoObj.m_RequestFunctionName = ReqFunName;
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),PARAMETERTAG) == 0)
						{
							ValueType vtype;
							std::string ParamName,ParamValue;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),"<xmlattr>") == 0)
								{
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),NAME) == 0)
											ParamName = v4.second.data();
										else if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),VALUE) == 0)
											ParamValue = v4.second.data();
									}
								}
							}
							vtype.value = ParamValue;
							m_FunRequestInfoObj.m_RequestPgs.m_Params.insert(std::make_pair(ParamName,vtype));
							//m_FunRequestInfoObj.m_FunctionParameterpair.insert(std::make_pair(ParamName,ParamValue));
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),PARAMETER_GROUPTAG) == 0)
						{

							ParameterGroup m_ParentParmgrpObj;
							std::string ParmgrpIdName;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),"<xmlattr>") == 0)
								{	
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),ID) == 0)
										ParmgrpIdName = v4.second.data();
									}
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETERTAG) == 0)
								{
									ValueType vtype;
									std::string ParamName,ParamValue;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),"<xmlattr>") == 0)
										{
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),NAME) == 0)
													ParamName = v5.second.data();
												else if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),VALUE) == 0)
													ParamValue = v5.second.data();
											}
										}
									}
									vtype.value = ParamValue;
									//m_ParentParmgrpObj.m_ChildParameterPair.insert(std::make_pair(ParamName,ParamValue));
									m_ParentParmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));

								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETER_GROUPTAG) == 0)
								{
									PopulateParamgrpsInfo(v3,m_ParentParmgrpObj);
								}
							}
							//m_FunRequestInfoObj.m_FunReqPararmGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
							m_FunRequestInfoObj.m_RequestPgs.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
						}
					}
					//Pushing the RefunctionInfo in to the list in the body
					//m_xmlRequestinfo.m_bodyObj.m_RequestedFunctionsList.push_back(m_FunRequestInfoObj);
					m_xmlRequestinfo.m_ReqstbodyObj.m_FunctionsList.push_back(m_FunRequestInfoObj);
				}
			}
		}
	}
}

void XmlRequestValueObject::PopulateParamgrpsInfo(boost::property_tree::ptree::value_type &v,ParameterGroup &m_ParentParmgrpObj)
{
	using boost::property_tree::ptree;
	ParameterGroup m_parmgrpObj;
	std::string ParmgrpIdName;
	BOOST_FOREACH(ptree::value_type &v1,v.second)
	{
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),"<xmlattr>") == 0)
		{	
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ID) == 0)
				ParmgrpIdName = v2.second.data();
			}
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETERTAG) == 0)
		{
			ValueType vtype;
			std::string ParamName,ParamValue;
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
							ParamName = v3.second.data();
						else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),VALUE) == 0)
							ParamValue = v3.second.data();
					}
				}
			}
			vtype.value = ParamValue;
			//m_parmgrpObj.m_ChildParameterPair.insert(std::make_pair(ParamName,ParamValue));
			m_parmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETER_GROUPTAG) == 0)
		{
			PopulateParamgrpsInfo(v1,m_parmgrpObj);
		}
	}
	//m_ParentParmgrpObj.m_ChildParamGroups.insert(std::make_pair(ParmgrpIdName,m_parmgrpObj));
	m_ParentParmgrpObj.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_parmgrpObj));
}

void XmlRequestValueObject::XmlRequestSave(const std::string &filename)
{
	using boost::property_tree::ptree;
	ptree RequestTree;
	try
	{
		ParseXmlRequestObjectForSave(RequestTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(filename,RequestTree,std::locale(),w);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}

}
void XmlRequestValueObject::XmlRequestSave(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
	ptree RequestTree;
	try
	{
		ParseXmlRequestObjectForSave(RequestTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(XmlStream,RequestTree,w);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}

void XmlRequestValueObject::ParseXmlRequestObjectForSave(boost::property_tree::ptree &RequestTree)
{
	using boost::property_tree::ptree;
	RequestTree.put(REQUESTTAG".<xmlattr>."ID,m_xmlRequestinfo.m_RequestID);
	RequestTree.put(REQUESTTAG".<xmlattr>."VERSION,m_xmlRequestinfo.m_Version);
	RequestTree.put(AUTHENTICATE_ACCESSKEYID_PATH,m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessKeyID);
	RequestTree.put(AUTHENTICATE_ACCESSSIGNATURE_PATH,m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessSignature);

	std::map<std::string,ValueType>::const_iterator ReqAuthParamIter =  m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AuthenticationParamsPair.begin();
	while(ReqAuthParamIter!=m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AuthenticationParamsPair.end())
	{
		ptree childtree;
		childtree.put("<xmlattr>."NAME,ReqAuthParamIter->first);
		childtree.put("<xmlattr>."VALUE,Escapexml(ReqAuthParamIter->second.value));

		RequestTree.add_child(AUTHENTICATE_PARAMETER_PATH,childtree);
		ReqAuthParamIter++;
	}
	
	//if(!m_xmlRequestinfo.m_HeaderObj.m_HeaderParameterPair.empty())
	if(!m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params.empty())
	{
		std::map<std::string,ValueType>::const_iterator ReqHeaderParamIter =  m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params.begin();
		while(ReqHeaderParamIter != m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params.end())
		{
			ptree childtree;
			childtree.put("<xmlattr>."NAME,ReqHeaderParamIter->first);
			childtree.put("<xmlattr>."VALUE,Escapexml(ReqHeaderParamIter->second.value));
		
			RequestTree.add_child(HEADER_PARAMETER_PATH,childtree);
			ReqHeaderParamIter++;
		}
	}

	if(!m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups.empty())
	{
		ParameterGroupsIter_t ReqHdrParmGrpIter = m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups.begin();
		while(ReqHdrParmGrpIter!=m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups.end())
		{
			ptree ParmGrptree;
			std::string ParamGrpID = ReqHdrParmGrpIter->first;
			ParmGrptree.put("<xmlattr>."ID,ParamGrpID);
			std::map<std::string,ValueType>::const_iterator PgchildparamIter = ReqHdrParmGrpIter->second.m_Params.begin();
			while(PgchildparamIter != ReqHdrParmGrpIter->second.m_Params.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,PgchildparamIter->first);
				childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
				ParmGrptree.add_child(PARAMETERTAG,childtree);
				PgchildparamIter++;
			}
			if(!ReqHdrParmGrpIter->second.m_ParamGroups.empty())
			{
				std::string ParamGrpPath = PARAMETER_GROUPTAG;
				PopulateParamGrpsTree(ReqHdrParmGrpIter->second.m_ParamGroups,ParamGrpPath,ParmGrptree);
			}
			RequestTree.add_child(HEADER_PARAMETERGROUP_PATH,ParmGrptree);
			ReqHdrParmGrpIter++;
		}
	}

	std::list<FunctionInfo>::const_iterator ReqFunctionInfoIter = m_xmlRequestinfo.m_ReqstbodyObj.m_FunctionsList.begin();
	while(ReqFunctionInfoIter!=  m_xmlRequestinfo.m_ReqstbodyObj.m_FunctionsList.end())
	{
		ptree pt;
	
		pt.put("<xmlattr>."NAME,ReqFunctionInfoIter->m_RequestFunctionName);
		pt.put("<xmlattr>."ID,ReqFunctionInfoIter->m_FuntionId);
		pt.put("<xmlattr>."INCLUDE,ReqFunctionInfoIter->m_ReqstInclude);

		if(!ReqFunctionInfoIter->m_RequestPgs.m_Params.empty())
		{
			std::map<std::string,ValueType>::const_iterator ReqFunParamIter =  ReqFunctionInfoIter->m_RequestPgs.m_Params.begin();
			while(ReqFunParamIter != ReqFunctionInfoIter->m_RequestPgs.m_Params.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,ReqFunParamIter->first);
				childtree.put("<xmlattr>."VALUE,Escapexml(ReqFunParamIter->second.value));
				pt.add_child(PARAMETERTAG,childtree);
				ReqFunParamIter++;
			}
		}
		if(!ReqFunctionInfoIter->m_RequestPgs.m_ParamGroups.empty())
		{
			ConstParameterGroupsIter_t ReqFunParmGrpIter = ReqFunctionInfoIter->m_RequestPgs.m_ParamGroups.begin();
			while(ReqFunParmGrpIter != ReqFunctionInfoIter->m_RequestPgs.m_ParamGroups.end())
			{
				ptree ParmGrptree;
				std::string ParamGrpID = ReqFunParmGrpIter->first;
				ParmGrptree.put("<xmlattr>."ID,ParamGrpID);
				std::map<std::string,ValueType>::const_iterator PgchildparamIter = ReqFunParmGrpIter->second.m_Params.begin();
				while(PgchildparamIter != ReqFunParmGrpIter->second.m_Params.end())
				{
					ptree childtree;
					childtree.put("<xmlattr>."NAME,PgchildparamIter->first);
					childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
					ParmGrptree.add_child(PARAMETERTAG,childtree);
					PgchildparamIter++;
				}
				if(!ReqFunParmGrpIter->second.m_ParamGroups.empty())
				{
					std::string ParamGrpPath = PARAMETER_GROUPTAG;
					PopulateParamGrpsTree(ReqFunParmGrpIter->second.m_ParamGroups,ParamGrpPath,ParmGrptree);
				}
				pt.add_child(PARAMETER_GROUPTAG,ParmGrptree);
				ReqFunParmGrpIter++;
			}
		}
		RequestTree.add_child(FUNCTION_REQUEST_PATH,pt);
		ReqFunctionInfoIter++;
	}
}

void XmlRequestValueObject::PopulateParamGrpsTree(const ParameterGroups_t &ParagmGrps,std::string &ParmGrpTreePath,boost::property_tree::ptree &ParmGrptree)
{
	using boost::property_tree::ptree;
	if(!ParagmGrps.empty())
	{
		ConstParameterGroupsIter_t ParmgrpIter = ParagmGrps.begin();
		while(ParmgrpIter!=ParagmGrps.end())
		{
			ptree childPrgTree;
			std::string ChildPrmGrpID = ParmgrpIter->first;
			childPrgTree.put("<xmlattr>."ID,ChildPrmGrpID);
			std::map<std::string,ValueType>::const_iterator PgchildparamIter = ParmgrpIter->second.m_Params.begin();
			while(PgchildparamIter != ParmgrpIter->second.m_Params.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,PgchildparamIter->first);
				childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
				childPrgTree.add_child(PARAMETERTAG,childtree);
				PgchildparamIter++;
			}
			if(!ParmgrpIter->second.m_ParamGroups.empty())
			{
				PopulateParamGrpsTree(ParmgrpIter->second.m_ParamGroups,ParmGrpTreePath,childPrgTree);
			}
			std::string childPrgtreepath = ParmGrpTreePath;
			ParmGrptree.add_child(childPrgtreepath,childPrgTree);
			ParmgrpIter++;
		}
	}
}

//response load
void XmlResponseValueObject::InitializeXmlResponseObject(const std::string &filename)
{
	try
	{
		XmlResponseLoad(filename);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error while reading in xml file :"<<ex.what()<<std::endl;
		throw ex;
	}
}
void XmlResponseValueObject::InitializeXmlResponseObject(std::stringstream &XmlStream)
{
	try
	{
		XmlResponseLoad(XmlStream);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error while reading in xml file :"<<ex.what()<<std::endl;
		throw ex;
	}
}
void XmlResponseValueObject::XmlResponseLoad(const std::string &filename)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(filename, pt);
		ParseXmlResponseForLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}
void XmlResponseValueObject::XmlResponseLoad(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(XmlStream, pt);
		ParseXmlResponseForLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}
void XmlResponseValueObject::ParseXmlResponseForLoad(boost::property_tree::ptree &pt)
{
	using boost::property_tree::ptree;
	BOOST_FOREACH(ptree::value_type &v, pt.get_child(RESPONSE_TAG".<"XMLATTR">"))
	{
		if(!std::string(ID).compare(v.first.data()))
		{
			m_xmlResponseInfo.m_Id = v.second.data();
		}
		if(!std::string(VERSION).compare(v.first.data()))
		{
			m_xmlResponseInfo.m_version = v.second.data();
		}
		if(!std::string(RETURNCODE).compare(v.first.data()))
		{
			m_xmlResponseInfo.m_ReturnCode = v.second.data();
		}
		if(!std::string(MESSAGE).compare(v.first.data()))
		{
			m_xmlResponseInfo.m_Message = v.second.data();
		}
	}
	
	BOOST_FOREACH(ptree::value_type &v, pt.get_child(RESPONSE_TAG))
	{
		if(InmStrCmp<NoCaseCharCmp>(v.first.data(),BODY) == 0)
		{
			BOOST_FOREACH(ptree::value_type &v1,v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),FUNCTIONTAG) == 0)
				{
					FunctionInfo m_FunctionInfoObj;
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
						{
							std::string FunName,FunID,Returncode,Message;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
									FunName = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ID) == 0)
									FunID = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),RETURNCODE) == 0)
									Returncode = v3.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),MESSAGE) == 0)
									Message = v3.second.data();
							}
							m_FunctionInfoObj.m_RequestFunctionName = FunName;
							m_FunctionInfoObj.m_FuntionId	 = FunID;
							m_FunctionInfoObj.m_ReturnCode	 = Returncode;
							m_FunctionInfoObj.m_Message		 = Message;

						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),FUNCTION_REQUEST_TAG) == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),"<xmlattr>") == 0)
								{
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(v4.first.data(),NAME)
										m_FunctionInfoObj.m_RequestFunctionName = v4.second.data();
									}
									
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETERTAG) == 0)
								{
									ValueType vtype;
									std::string ParamName,ParamValue;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),"<xmlattr>") == 0)
										{
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),NAME) == 0)
													ParamName = v5.second.data();
												else if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),VALUE) == 0)
													ParamValue = v5.second.data();
											}
										}
									}
									vtype.value = ParamValue;
									m_FunctionInfoObj.m_RequestPgs.m_Params.insert(std::make_pair(ParamName,vtype));
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETER_GROUPTAG) == 0)
								{
									ParameterGroup m_ParentParmgrpObj;
									std::string ParmgrpIdName;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),"<xmlattr>") == 0)
										{	
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),ID) == 0)
												ParmgrpIdName = v5.second.data();
											}
										}
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),PARAMETERTAG) == 0)
										{
											ValueType vtype;
											std::string ParamName,ParamValue;
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),"<xmlattr>") == 0)
												{
													BOOST_FOREACH(ptree::value_type &v6,v5.second)
													{
														if(InmStrCmp<NoCaseCharCmp>(v6.first.data(),NAME) == 0)
															ParamName = v6.second.data();
														else if(InmStrCmp<NoCaseCharCmp>(v6.first.data(),VALUE) == 0)
															ParamValue = v6.second.data();
													}
												}
											}
											vtype.value = ParamValue;
											m_ParentParmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));
										}
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),PARAMETER_GROUPTAG) == 0)
										{
											PopulateParamgrpsInfo(v4,m_ParentParmgrpObj);
										}
									}
				
									m_FunctionInfoObj.m_RequestPgs.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
								}
							}
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),FUNCTION_RESPONSE_TAG) == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETERTAG) == 0)
								{
									ValueType vtype;
									std::string ParamName,ParamValue;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),"<xmlattr>") == 0)
										{
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),NAME) == 0)
													ParamName = v5.second.data();
												else if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),VALUE) == 0)
													ParamValue = v5.second.data();
											}
										}
									}
									vtype.value = ParamValue;
									m_FunctionInfoObj.m_ResponsePgs.m_Params.insert(std::make_pair(ParamName,vtype));
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),PARAMETER_GROUPTAG) == 0)
								{
									ParameterGroup m_ParentParmgrpObj;
									std::string ParmgrpIdName;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),"<xmlattr>") == 0)
										{	
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),ID) == 0)
												ParmgrpIdName = v5.second.data();
											}
										}
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),PARAMETERTAG) == 0)
										{
											ValueType vtype;
											std::string ParamName,ParamValue;
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),"<xmlattr>") == 0)
												{
													BOOST_FOREACH(ptree::value_type &v6,v5.second)
													{
														if(InmStrCmp<NoCaseCharCmp>(v6.first.data(),NAME) == 0)
															ParamName = v6.second.data();
														else if(InmStrCmp<NoCaseCharCmp>(v6.first.data(),VALUE) == 0)
															ParamValue = v6.second.data();
													}
												}
											}
											vtype.value = ParamValue;
											m_ParentParmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));
										}
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),PARAMETER_GROUPTAG) == 0)
										{
											PopulateParamgrpsInfo(v4,m_ParentParmgrpObj);
										}
									}
									m_FunctionInfoObj.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_ParentParmgrpObj));
								}
							}
						}
					}
					m_xmlResponseInfo.m_ResponsebodyObj.m_FunctionsList.push_back(m_FunctionInfoObj);
				}
			}
		}
	}
}

void XmlResponseValueObject::PopulateParamgrpsInfo(boost::property_tree::ptree::value_type &v,ParameterGroup &m_ParentParmgrpObj)
{
	using boost::property_tree::ptree;
	ParameterGroup m_parmgrpObj;
	std::string ParmgrpIdName;
	BOOST_FOREACH(ptree::value_type &v1,v.second)
	{
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),"<xmlattr>") == 0)
		{	
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ID) == 0)
				ParmgrpIdName = v2.second.data();
			}
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETERTAG) == 0)
		{
			ValueType vtype;
			std::string ParamName,ParamValue;
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
							ParamName = v3.second.data();
						else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),VALUE) == 0)
							ParamValue = v3.second.data();
					}
				}
			}
			vtype.value = ParamValue;
			m_parmgrpObj.m_Params.insert(std::make_pair(ParamName,vtype));
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),PARAMETER_GROUPTAG) == 0)
		{
			PopulateParamgrpsInfo(v1,m_parmgrpObj);
		}
	}
	m_ParentParmgrpObj.m_ParamGroups.insert(std::make_pair(ParmgrpIdName,m_parmgrpObj));
}
void XmlResponseValueObject::XmlResponseSave(const std::string &filename)
{
	using boost::property_tree::ptree;
	ptree ResponseTree;
	try
	{
		ParseXmlResponseObjectForSave(ResponseTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(filename,ResponseTree,std::locale(),w);	
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}

}

void XmlResponseValueObject::XmlResponseSave(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
	ptree ResponseTree;
	try
	{
		ParseXmlResponseObjectForSave(ResponseTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(XmlStream,ResponseTree,w);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}

}
void XmlResponseValueObject::ParseXmlResponseObjectForSave(boost::property_tree::ptree &ResponseTree)
{
	using boost::property_tree::ptree;
	ResponseTree.put(RESPONSE_TAG".<xmlattr>."ID,Escapexml(m_xmlResponseInfo.m_Id));
	ResponseTree.put(RESPONSE_TAG".<xmlattr>."VERSION,m_xmlResponseInfo.m_version);
	ResponseTree.put(RESPONSE_TAG".<xmlattr>."RETURNCODE,m_xmlResponseInfo.m_ReturnCode);
	ResponseTree.put(RESPONSE_TAG".<xmlattr>."MESSAGE,Escapexml(m_xmlResponseInfo.m_Message));
	
	std::list<FunctionInfo>::const_iterator FunctionIter  = m_xmlResponseInfo.m_ResponsebodyObj.m_FunctionsList.begin();
	while(FunctionIter != m_xmlResponseInfo.m_ResponsebodyObj.m_FunctionsList.end())
	{
		ptree FunctionTree;
		FunctionTree.put("<xmlattr>."NAME,FunctionIter->m_RequestFunctionName);
		FunctionTree.put("<xmlattr>."ID,FunctionIter->m_FuntionId);
		FunctionTree.put("<xmlattr>."RETURNCODE,FunctionIter->m_ReturnCode);
		FunctionTree.put("<xmlattr>."MESSAGE,Escapexml(FunctionIter->m_Message));
		
		//need to check this 
		//if(FunctionIter->m_FunReqInResponseObj.m_Include.compare("Yes"))
		if(InmStrCmp<NoCaseCharCmp>(FunctionIter->m_ReqstInclude.c_str(),"Yes") == 0)
		{
			ptree pt;
			pt.put("<xmlattr>."NAME,FunctionIter->m_RequestFunctionName);
			
			if(!FunctionIter->m_RequestPgs.m_Params.empty())
			{
				std::map<std::string,ValueType>::const_iterator ReqFunParamIter =  FunctionIter->m_RequestPgs.m_Params.begin();
				while(ReqFunParamIter != FunctionIter->m_RequestPgs.m_Params.end())
				{
					ptree childtree;
					childtree.put("<xmlattr>."NAME,ReqFunParamIter->first);
					childtree.put("<xmlattr>."VALUE,Escapexml(ReqFunParamIter->second.value));
	
					pt.add_child(PARAMETERTAG,childtree);
					ReqFunParamIter++;
				}
			}

			if(!FunctionIter->m_RequestPgs.m_ParamGroups.empty())
			{
				ConstParameterGroupsIter_t ReqFunParmGrpIter = FunctionIter->m_RequestPgs.m_ParamGroups.begin();
				while(ReqFunParmGrpIter != FunctionIter->m_RequestPgs.m_ParamGroups.end())
				{
					ptree ParmGrptree;
					std::string ParamGrpID = ReqFunParmGrpIter->first;
					ParmGrptree.put("<xmlattr>."ID,Escapexml(ParamGrpID));
	
					std::map<std::string,ValueType>::const_iterator PgchildparamIter = ReqFunParmGrpIter->second.m_Params.begin();
					while(PgchildparamIter != ReqFunParmGrpIter->second.m_Params.end())
					{
						ptree childtree;
						childtree.put("<xmlattr>."NAME,Escapexml(PgchildparamIter->first));
						childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
	
						ParmGrptree.add_child(PARAMETERTAG,childtree);
						PgchildparamIter++;
					}
					if(!ReqFunParmGrpIter->second.m_ParamGroups.empty())
					{
						std::string ParamGrpPath = PARAMETER_GROUPTAG;
						PopulateParamGrpsTree(ReqFunParmGrpIter->second.m_ParamGroups,ParamGrpPath,ParmGrptree);
					}
	
					pt.add_child(PARAMETER_GROUPTAG,ParmGrptree);
					ReqFunParmGrpIter++;
				}
			}

			FunctionTree.add_child(FUNCTION_REQUEST_TAG,pt);
		}
		
		ptree childRespnseTree;
		if(!FunctionIter->m_ResponsePgs.m_Params.empty())
		{
			std::map<std::string,ValueType>::const_iterator ReqFunParamIter =  FunctionIter->m_ResponsePgs.m_Params.begin();
			while(ReqFunParamIter != FunctionIter->m_ResponsePgs.m_Params.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,ReqFunParamIter->first);
				childtree.put("<xmlattr>."VALUE,Escapexml(ReqFunParamIter->second.value));
		
				childRespnseTree.add_child(PARAMETERTAG,childtree);
				ReqFunParamIter++;
			}
		}
		if(!FunctionIter->m_ResponsePgs.m_ParamGroups.empty())
		{
	
			ConstParameterGroupsIter_t ReqFunParmGrpIter = FunctionIter->m_ResponsePgs.m_ParamGroups.begin();
			while(ReqFunParmGrpIter != FunctionIter->m_ResponsePgs.m_ParamGroups.end())
			{
				ptree ParmGrptree;
				std::string ParamGrpID = ReqFunParmGrpIter->first;
				ParmGrptree.put("<xmlattr>."ID,Escapexml(ParamGrpID));
		
				std::map<std::string,ValueType>::const_iterator PgchildparamIter = ReqFunParmGrpIter->second.m_Params.begin();
				while(PgchildparamIter != ReqFunParmGrpIter->second.m_Params.end())
				{
					ptree childtree;
					childtree.put("<xmlattr>."NAME,PgchildparamIter->first);
					childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
			
					ParmGrptree.add_child(PARAMETERTAG,childtree);
					PgchildparamIter++;
				}
				if(!ReqFunParmGrpIter->second.m_ParamGroups.empty())
				{
					std::string ParamGrpPath = PARAMETER_GROUPTAG;
					PopulateParamGrpsTree(ReqFunParmGrpIter->second.m_ParamGroups,ParamGrpPath,ParmGrptree);
				}
			
				childRespnseTree.add_child(PARAMETER_GROUPTAG,ParmGrptree);
				ReqFunParmGrpIter++;
			}
		}
	
		FunctionTree.add_child(FUNCTION_RESPONSE_TAG,childRespnseTree);
		FunctionIter++;
		ResponseTree.add_child(MAIN_FUNCTION_IN_RESPONSE_PATH,FunctionTree);
	}
}

void XmlResponseValueObject::PopulateParamGrpsTree(const ParameterGroups_t& ParagmGrps,std::string &ParmGrpTreePath,boost::property_tree::ptree &ParmGrptree)
{
	using boost::property_tree::ptree;
	if(!ParagmGrps.empty())
	{
		ConstParameterGroupsIter_t ParmgrpIter = ParagmGrps.begin();
		while(ParmgrpIter!=ParagmGrps.end())
		{
			ptree childPrgTree;
			std::string ChildPrmGrpID = ParmgrpIter->first;
			childPrgTree.put("<xmlattr>."ID,Escapexml(ChildPrmGrpID));
			std::map<std::string,ValueType>::const_iterator PgchildparamIter = ParmgrpIter->second.m_Params.begin();
			while(PgchildparamIter != ParmgrpIter->second.m_Params.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,PgchildparamIter->first);
				childtree.put("<xmlattr>."VALUE,Escapexml(PgchildparamIter->second.value));
				childPrgTree.add_child(PARAMETERTAG,childtree);
				PgchildparamIter++;
			}
			if(!ParmgrpIter->second.m_ParamGroups.empty())
			{
				PopulateParamGrpsTree(ParmgrpIter->second.m_ParamGroups,ParmGrpTreePath,childPrgTree);
			}
			std::string childPrgtreepath = ParmGrpTreePath;
			ParmGrptree.add_child(childPrgtreepath,childPrgTree);
			ParmgrpIter++;
		}
	}
}


//Xml request valueObject get and set methods
std::string XmlRequestValueObject::getRequestId() const
{
	return m_xmlRequestinfo.m_RequestID;
}
std::string XmlRequestValueObject::getVersion() const
{
	return m_xmlRequestinfo.m_Version;
}
Header XmlRequestValueObject::getHeaderInfo() const
{
	return m_xmlRequestinfo.m_HeaderObj;
}

std::string XmlRequestValueObject::getAuthenticationAccessKeyID() const
{
	return m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessKeyID;
	
}
std::string XmlRequestValueObject::getAuthenticationAccessSignature() const
{
	return m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessSignature;
	
}
std::map<std::string,ValueType> XmlRequestValueObject::getAuthenticationParameterPair() const
{
	return m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AuthenticationParamsPair;
}
std::map<std::string,ValueType> XmlRequestValueObject::getHeaderParameterPair() const
{
	return m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params;
}
ParameterGroups_t XmlRequestValueObject::getHeaderParameterGroups() const
{
	return m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups;
}

std::list<FunctionInfo> XmlRequestValueObject::getRequestFunctionList() const
{
	return m_xmlRequestinfo.m_ReqstbodyObj.m_FunctionsList;
}


void XmlRequestValueObject::setRequestId( std::string const& RequestId)
{
	m_xmlRequestinfo.m_RequestID = RequestId;
}
void XmlRequestValueObject::setVersion(std::string const& Version)
{
	m_xmlRequestinfo.m_Version = Version;
}
void XmlRequestValueObject::setAuthenticationAccessKeyID(std::string const& AccessKeyID)
{
	m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessKeyID = AccessKeyID;
}
void XmlRequestValueObject::setAuthenticationAccessSignature(std::string const& AccessSignature)
{
	m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AccessSignature = AccessSignature;
}
void XmlRequestValueObject::setAuthenticationParameterPair(std::string const& AuthParamName,ValueType& AuthParamValue)
{
	m_xmlRequestinfo.m_HeaderObj.m_AuthenticationObj.m_AuthenticationParamsPair.insert(std::make_pair(AuthParamName,AuthParamValue));
}
void XmlRequestValueObject::setHeaderParameterPair(std::string const& HeaderParamName,ValueType &HeaderParamValue)
{
	m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_Params.insert(std::make_pair(HeaderParamName,HeaderParamValue));
}
void XmlRequestValueObject::setHeaderParameterGroups(std::string const&parmGrpID,ParameterGroup &m_paramgrpobj)
{
	m_xmlRequestinfo.m_HeaderObj.m_HeaderParmGrp.m_ParamGroups.insert(std::make_pair(parmGrpID,m_paramgrpobj));
}

void XmlRequestValueObject::setRequestFunctionList(FunctionInfo &m_Funreqinfo)
{
	m_xmlRequestinfo.m_ReqstbodyObj.m_FunctionsList.push_back(m_Funreqinfo);
}


//xml reponse get set functions
std::string XmlResponseValueObject::getResponseId() const
{
	return m_xmlResponseInfo.m_Id;
}
std::string XmlResponseValueObject::getResponseVersion() const 
{
	return m_xmlResponseInfo.m_version;
}
std::string XmlResponseValueObject::getResponseReturncode() const
{
	return m_xmlResponseInfo.m_ReturnCode;
}
std::string XmlResponseValueObject::getResponseReturnMeaasge() const
{
	return m_xmlResponseInfo.m_Message;
}
std::list<FunctionInfo> XmlResponseValueObject::getFunctionList() const
{
	return m_xmlResponseInfo.m_ResponsebodyObj.m_FunctionsList;
}

void XmlResponseValueObject::setResponseID(std::string const& ResponseID)
{
	m_xmlResponseInfo.m_Id = ResponseID;
}
void XmlResponseValueObject::setResponseVersion(std::string const& ResponseVersion)
{
	m_xmlResponseInfo.m_version = ResponseVersion;
}
void XmlResponseValueObject::setResponseReturncode(std::string const& ResponseReturncode)
{
	m_xmlResponseInfo.m_ReturnCode = ResponseReturncode;
}
void XmlResponseValueObject::setResponseReturnMeaasge(std::string const& ResponseReturnMeaasge)
{
	m_xmlResponseInfo.m_Message = Escapexml(ResponseReturnMeaasge);
}
void XmlResponseValueObject::setFunctionList(FunctionInfo &m_FunctionInfoobj)
{
	m_xmlResponseInfo.m_ResponsebodyObj.m_FunctionsList.push_back(m_FunctionInfoobj);
}
