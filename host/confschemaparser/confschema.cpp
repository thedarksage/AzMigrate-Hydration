#include "confschema.h"
#include "confschemaglobals.h"
#include "inmstrcmp.h"

ConfSchemaValueObject::ConfSchemaValueObject()
{

}
ConfSchemaValueObject::~ConfSchemaValueObject()
{

}
void
ConfSchemaValueObject::XmlConfileLoad(const std::string &filename)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(filename, pt);
		ParseConffileToLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}

void
ConfSchemaValueObject::XmlConfileLoad(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
    ptree pt;

	try
	{
		read_xml(XmlStream, pt);
		ParseConffileToLoad(pt);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}

void ConfSchemaValueObject::XmlConfileSave(const std::string &filename)
{
	using boost::property_tree::ptree;
	ptree ConfigTree;
	try
	{
		ParseConfigObjectForSave(ConfigTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(filename,ConfigTree,std::locale(),w);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}
}
void ConfSchemaValueObject::XmlConfileSave(std::stringstream &XmlStream)
{
	using boost::property_tree::ptree;
	ptree ConfigTree;
	try
	{
		ParseConfigObjectForSave(ConfigTree);
		boost::property_tree::xml_writer_settings<char> w(' ', 4);
		write_xml(XmlStream,ConfigTree,w);
	}
	catch(std::exception& ex)
	{
		std::cout<<"Error: "<<ex.what()<<std::endl;
		throw ex;
	}

}
void ConfSchemaValueObject::ParseConffileToLoad(boost::property_tree::ptree &pt)
{
	using boost::property_tree::ptree;
	
	BOOST_FOREACH(ptree::value_type &v, pt.get_child(CONFIGTAG".<"XMLATTR">"))
	{
		if(InmStrCmp<NoCaseCharCmp>(VERSION,v.first.data())== 0)
		{
			m_Config.m_version	= v.second.data();
		}
	}
	BOOST_FOREACH(ptree::value_type &v, pt.get_child(CONFIGTAG))
	{
		if(InmStrCmp<NoCaseCharCmp>(GROUP,v.first.data())== 0)
		{
			Group GroupObj;
			//std::string GroupType;
			
			BOOST_FOREACH(ptree::value_type &v1, v.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(TYPE,v2.first.data())== 0)
						{
							GroupObj.m_GroupType = v2.second.data();
						}
						if(InmStrCmp<NoCaseCharCmp>(VERSION,v2.first.data())== 0)
						{
							GroupObj.m_Groupversion = v2.second.data();
						}
					}
				}
				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),OBJECT) == 0)
				{
					Object ObjInstance;
					std::string ObjectId;
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(strcmp(v2.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(ID,v3.first.data())== 0)
								{
									ObjectId = v3.second.data();
								}
								if(InmStrCmp<NoCaseCharCmp>(TIMESTAMP,v3.first.data())== 0)
								{
									ObjInstance.m_TimeStamp = v3.second.data();
								}
							}
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE) == 0)
						{
							std::string AttrName,AttrValue;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(strcmp(v3.first.data(),"<xmlattr>") == 0)
								{
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),NAME) == 0)
											AttrName = v4.second.data();
										else if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),VALUE) == 0)
											AttrValue = v4.second.data();
									}
								}
							}
							ObjInstance.m_Attrs.insert(std::make_pair(AttrName,AttrValue));
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE_GROUP) == 0)
						{
							AttributeGroup m_ParentAttrgrpObj;
							std::string AttrgrpIdName;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(strcmp(v3.first.data(),"<xmlattr>") == 0)
								{	
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),ID) == 0)
										AttrgrpIdName = v4.second.data();
									}
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ATTRIBUTE) == 0)
								{
									std::string AttrName,AttrValue;
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(strcmp(v4.first.data(),"<xmlattr>") == 0)
										{
											BOOST_FOREACH(ptree::value_type &v5,v4.second)
											{
												if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),NAME) == 0)
													AttrName = v5.second.data();
												else if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),VALUE) == 0)
													AttrValue = v5.second.data();
											}
										}
									}
									m_ParentAttrgrpObj.m_Attrs.insert(std::make_pair(AttrName,AttrValue));
								}
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ATTRIBUTE_GROUP) == 0)
								{
									PopulateAttributegrpsInfo(v3,m_ParentAttrgrpObj);
								}
							}
							
							ObjInstance.m_ObjAttrbGrps.m_AttrbGroups.insert(std::make_pair(AttrgrpIdName,m_ParentAttrgrpObj));
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),GROUP) == 0)
						{
							//Group m_Parentgrpobj; //no need here
							PoplulateGroupsInfo(v2,ObjInstance);
						}
					}
					GroupObj.m_Objects.insert(std::make_pair(ObjectId,ObjInstance));
				}

				if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),REFERENCE) == 0)
				{
					Reference ReferenceObj;
					std::string ReferenceType;
					BOOST_FOREACH(ptree::value_type &v2,v1.second)
					{
						if(strcmp(v2.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(TYPE,v3.first.data())== 0)
								{
									ReferenceType = v3.second.data();
								}
								if(InmStrCmp<NoCaseCharCmp>(SOURCE,v3.first.data())== 0)
								{
									ReferenceObj.m_refSource = v3.second.data();
								}
							}
						}
						if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE) == 0)
						{
							std::string AttrName,AttrRefers;
							BOOST_FOREACH(ptree::value_type &v3,v2.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),"<xmlattr>") == 0)
								{
									BOOST_FOREACH(ptree::value_type &v4,v3.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),NAME) == 0)
											AttrName = v4.second.data();
										else if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),REFERS) == 0)
											AttrRefers = v4.second.data();
									}
								}
							}
							ReferenceObj.m_ReferenceAttrbs.insert(std::make_pair(AttrName,AttrRefers));
						}
					}
					
					GroupObj.m_References.insert(std::make_pair(ReferenceType,ReferenceObj));
				}
				
			}
			//pushing the group in config object
			m_Config.m_grplist.push_back(GroupObj);
		}
	}
}

void ConfSchemaValueObject::PopulateAttributegrpsInfo(boost::property_tree::ptree::value_type& v,AttributeGroup& m_ParentAttrgrpObj)
{
	using boost::property_tree::ptree;
	AttributeGroup m_attrgrpObj;
	std::string AttrgrpID;
	BOOST_FOREACH(ptree::value_type &v1,v.second)
	{
		if(std::strcmp(v1.first.data(),"<xmlattr>") == 0)
		{	
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ID) == 0)
				AttrgrpID = v2.second.data();
			}
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),ATTRIBUTE) == 0)
		{
			std::string AttrName,AttrValue;
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(strcmp(v2.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),NAME) == 0)
							AttrName = v3.second.data();
						else if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),VALUE) == 0)
							AttrValue = v3.second.data();
					}
				}
			}
			m_attrgrpObj.m_Attrs.insert(std::make_pair(AttrName,AttrValue));
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),ATTRIBUTE_GROUP) == 0)
		{
			PopulateAttributegrpsInfo(v1,m_attrgrpObj);
		}
	}
	m_ParentAttrgrpObj.m_AttrbGroups.insert(std::make_pair(AttrgrpID,m_attrgrpObj));
}

void
ConfSchemaValueObject::PoplulateGroupsInfo(boost::property_tree::ptree::value_type &v,Object& ParentObj)
{
	using boost::property_tree::ptree;
	Group m_GroupObj;
	std::string GroupType;

	BOOST_FOREACH(ptree::value_type &v1, v.second)
	{
		if(strcmp(v1.first.data(),"<xmlattr>") == 0)
		{
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(InmStrCmp<NoCaseCharCmp>(TYPE,v2.first.data())== 0)
				{
					GroupType = v2.second.data();
					m_GroupObj.m_GroupType = GroupType;//ideally no need of this
				}
				if(InmStrCmp<NoCaseCharCmp>(VERSION,v2.first.data())== 0)
				{
					m_GroupObj.m_Groupversion = v2.second.data();
				}
			}
		}
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),OBJECT) == 0)
		{
			Object ObjInstance;
			std::string ObjectId;
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(strcmp(v2.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(ID,v3.first.data())== 0)
						{
							ObjectId = v3.second.data();
						}
						if(InmStrCmp<NoCaseCharCmp>(TIMESTAMP,v3.first.data())== 0)
						{
							ObjInstance.m_TimeStamp = v3.second.data();
						}
					}
				}
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE) == 0)
				{
					std::string AttrName,AttrValue;
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(strcmp(v3.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v4,v3.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),NAME) == 0)
									AttrName = v4.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),VALUE) == 0)
									AttrValue = v4.second.data();
							}
						}
					}
					ObjInstance.m_Attrs.insert(std::make_pair(AttrName,AttrValue));
				}
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE_GROUP) == 0)
				{
					AttributeGroup m_ParentAttrgrpObj;
					std::string AttrgrpIdName;
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(strcmp(v3.first.data(),"<xmlattr>") == 0)
						{	
							BOOST_FOREACH(ptree::value_type &v4,v3.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),ID) == 0)
								AttrgrpIdName = v4.second.data();
							}
						}
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ATTRIBUTE) == 0)
						{
							std::string AttrName,AttrValue;
							BOOST_FOREACH(ptree::value_type &v4,v3.second)
							{
								if(strcmp(v4.first.data(),"<xmlattr>") == 0)
								{
									BOOST_FOREACH(ptree::value_type &v5,v4.second)
									{
										if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),NAME) == 0)
											AttrName = v5.second.data();
										else if(InmStrCmp<NoCaseCharCmp>(v5.first.data(),VALUE) == 0)
											AttrValue = v5.second.data();
									}
								}
							}
							m_ParentAttrgrpObj.m_Attrs.insert(std::make_pair(AttrName,AttrValue));
						}
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),ATTRIBUTE_GROUP) == 0)
						{
							PopulateAttributegrpsInfo(v3,m_ParentAttrgrpObj);
						}
					}
					
					ObjInstance.m_ObjAttrbGrps.m_AttrbGroups.insert(std::make_pair(AttrgrpIdName,m_ParentAttrgrpObj));
				}
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),GROUP) == 0)
				{
					PoplulateGroupsInfo(v2,ObjInstance);
				}
			}
			m_GroupObj.m_Objects.insert(std::make_pair(ObjectId,ObjInstance));
		}
		
		//need to check if child groups will have the references are not
		if(InmStrCmp<NoCaseCharCmp>(v1.first.data(),REFERENCE) == 0)
		{
			Reference ReferenceObj;
			std::string ReferenceType;
			BOOST_FOREACH(ptree::value_type &v2,v1.second)
			{
				if(strcmp(v2.first.data(),"<xmlattr>") == 0)
				{
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(TYPE,v3.first.data())== 0)
						{
							ReferenceType = v3.second.data();
						}
						if(InmStrCmp<NoCaseCharCmp>(SOURCE,v3.first.data())== 0)
						{
							ReferenceObj.m_refSource = v3.second.data();
						}
					}
				}
				if(InmStrCmp<NoCaseCharCmp>(v2.first.data(),ATTRIBUTE) == 0)
				{
					std::string AttrName,AttrRefers;
					BOOST_FOREACH(ptree::value_type &v3,v2.second)
					{
						if(InmStrCmp<NoCaseCharCmp>(v3.first.data(),"<xmlattr>") == 0)
						{
							BOOST_FOREACH(ptree::value_type &v4,v3.second)
							{
								if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),NAME) == 0)
									AttrName = v4.second.data();
								else if(InmStrCmp<NoCaseCharCmp>(v4.first.data(),REFERS) == 0)
									AttrRefers = v4.second.data();
							}
						}
					}
					ReferenceObj.m_ReferenceAttrbs.insert(std::make_pair(AttrName,AttrRefers));
				}
			}
			
			m_GroupObj.m_References.insert(std::make_pair(ReferenceType,ReferenceObj));
		}
		
	}

	ParentObj.m_Groups.insert(std::make_pair(GroupType,m_GroupObj));
}

void ConfSchemaValueObject::ParseConfigObjectForSave(boost::property_tree::ptree& ConfigTree)
{
	using boost::property_tree::ptree;
	ConfigTree.put(CONFIGTAG".<xmlattr>."VERSION,m_Config.m_version);

	std::list<Group>::const_iterator GrouplistIter = m_Config.m_grplist.begin();
	while(GrouplistIter != m_Config.m_grplist.end())
	{
		ptree GroupTree;
		GroupTree.put("<xmlattr>."TYPE,GrouplistIter->m_GroupType);
		GroupTree.put("<xmlattr>."VERSION,GrouplistIter->m_Groupversion);
		
		if(!GrouplistIter->m_Objects.empty())
		{
			object_t::const_iterator ObjectIter = GrouplistIter->m_Objects.begin();
			while(ObjectIter!=GrouplistIter->m_Objects.end())
			{
				ptree ObjectTree;

				ObjectTree.put("<xmlattr>."ID,ObjectIter->first);
				ObjectTree.put("<xmlattr>."TIMESTAMP,ObjectIter->second.m_TimeStamp);

				if(!ObjectIter->second.m_Attrs.empty())
				{
					std::map<std::string,std::string>::const_iterator ObjectAttrIter =  ObjectIter->second.m_Attrs.begin();
					while(ObjectAttrIter != ObjectIter->second.m_Attrs.end())
					{
						ptree childtree;
						childtree.put("<xmlattr>."NAME,ObjectAttrIter->first);
						childtree.put("<xmlattr>."VALUE,ObjectAttrIter->second);
						ObjectTree.add_child(ATTRIBUTE,childtree);
						ObjectAttrIter++;
					}

				}
				if(!ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.empty())
				{
					std::map<std::string,AttributeGroup>::const_iterator AttrGrpIter = ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.begin();
					while(AttrGrpIter != ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.end())
					{
						ptree AttrGrptree;
						std::string AttrGrpID = AttrGrpIter->first;
						AttrGrptree.put("<xmlattr>."ID,AttrGrpID);
						std::map<std::string,std::string>::const_iterator childAttrIter = AttrGrpIter->second.m_Attrs.begin();
						while(childAttrIter != AttrGrpIter->second.m_Attrs.end())
						{
							ptree childtree;
							childtree.put("<xmlattr>."NAME,childAttrIter->first);
							childtree.put("<xmlattr>."VALUE,childAttrIter->second);
							AttrGrptree.add_child(ATTRIBUTE,childtree);
							childAttrIter++;
						}
						if(!AttrGrpIter->second.m_AttrbGroups.empty())
						{
							std::string AttrGrpPath = ATTRIBUTE_GROUP;
							PopulateAttrGrpsTree(AttrGrpIter->second.m_AttrbGroups,AttrGrpPath,AttrGrptree);
						}
						ObjectTree.add_child(ATTRIBUTE_GROUP,AttrGrptree);
						AttrGrpIter++;
					}
				}
				if(!ObjectIter->second.m_Groups.empty())
				{
					PopulateGroupstree(ObjectIter->second.m_Groups,ObjectTree);
				}
				GroupTree.add_child(OBJECT,ObjectTree);
				//GroupTree.add_child(OBJECT_PATH,ObjectTree);
				ObjectIter++;
			}
		}
		if(!GrouplistIter->m_References.empty())
		{
			//ptree ReferenceTree;
			std::map<ReferenceType_t,Reference>::const_iterator ReferenceIter = GrouplistIter->m_References.begin();
			while(ReferenceIter!=GrouplistIter->m_References.end())
			{
				ptree ReferenceTree;
				ReferenceTree.put("<xmlattr>."TYPE,ReferenceIter->first);
				ReferenceTree.put("<xmlattr>."SOURCE,ReferenceIter->second.m_refSource);

				std::map<std::string,std::string>::const_iterator ReferenceAttrIter = ReferenceIter->second.m_ReferenceAttrbs.begin();
				while(ReferenceAttrIter!= ReferenceIter->second.m_ReferenceAttrbs.end())
				{
					ptree childtree;
					childtree.put("<xmlattr>."NAME,ReferenceAttrIter->first);
					childtree.put("<xmlattr>."REFERS,ReferenceAttrIter->second);
					
					ReferenceTree.add_child(ATTRIBUTE,childtree);
					ReferenceAttrIter++;
				}
				
				GroupTree.add_child(REFERENCE,ReferenceTree);
				ReferenceIter++;
			}
			//GroupTree.add_child(REFERENCE_PATH,ReferenceTree);
			//GroupTree.add_child(REFERENCE,ReferenceTree);
		}
	
		ConfigTree.add_child(GROUP_PATH,GroupTree);
		//ConfigTree.add_child(GROUP,GroupTree);

		GrouplistIter++;
	}
}

void
ConfSchemaValueObject::PopulateAttrGrpsTree(const std::map<std::string,AttributeGroup>& AttrGrps, std::string& AttrGrpTreePath, boost::property_tree::ptree& AttrGrptree)
{
	using boost::property_tree::ptree;
	if(!AttrGrps.empty())
	{
		std::map<std::string,AttributeGroup>::const_iterator AttrgrpIter = AttrGrps.begin();
		while(AttrgrpIter!=AttrGrps.end())
		{
			ptree childAtgrptree;
			std::string ChildAttrGrpID = AttrgrpIter->first;
			childAtgrptree.put("<xmlattr>."ID,ChildAttrGrpID);
			std::map<std::string,std::string>::const_iterator AtgrpchildparamIter = AttrgrpIter->second.m_Attrs.begin();
			while(AtgrpchildparamIter != AttrgrpIter->second.m_Attrs.end())
			{
				ptree childtree;
				childtree.put("<xmlattr>."NAME,AtgrpchildparamIter->first);
				childtree.put("<xmlattr>."VALUE,AtgrpchildparamIter->second);
				childAtgrptree.add_child(ATTRIBUTE,childtree);
				AtgrpchildparamIter++;
			}
			if(!AttrgrpIter->second.m_AttrbGroups.empty())
			{
				PopulateAttrGrpsTree(AttrgrpIter->second.m_AttrbGroups,AttrGrpTreePath,childAtgrptree);
			}
			std::string childAtgrptreepath = AttrGrpTreePath;
			AttrGrptree.add_child(childAtgrptreepath,childAtgrptree);
			AttrgrpIter++;
		}
	}
}

void
ConfSchemaValueObject::PopulateGroupstree(const std::map<GroupType_t,Group>& ObjGroupsMap, boost::property_tree::ptree &ObjectTree)
{
	using boost::property_tree::ptree;
	
	std::map<GroupType_t,Group>::const_iterator ObjGroupsMapIter = ObjGroupsMap.begin();
	while(ObjGroupsMapIter != ObjGroupsMap.end())
	{
		ptree GroupTree;
		GroupTree.put("<xmlattr>."TYPE,ObjGroupsMapIter->first);
		GroupTree.put("<xmlattr>."VERSION,ObjGroupsMapIter->second.m_Groupversion);
		
		if(!ObjGroupsMapIter->second.m_Objects.empty())
		{
			object_t::const_iterator ObjectIter = ObjGroupsMapIter->second.m_Objects.begin();
			while(ObjectIter!=ObjGroupsMapIter->second.m_Objects.end())
			{
				ptree ObjectTree;

				ObjectTree.put("<xmlattr>."ID,ObjectIter->first);
				ObjectTree.put("<xmlattr>."TIMESTAMP,ObjectIter->second.m_TimeStamp);

				if(!ObjectIter->second.m_Attrs.empty())
				{
					std::map<std::string,std::string>::const_iterator ObjectAttrIter =  ObjectIter->second.m_Attrs.begin();
					while(ObjectAttrIter != ObjectIter->second.m_Attrs.end())
					{
						ptree childtree;
						childtree.put("<xmlattr>."NAME,ObjectAttrIter->first);
						childtree.put("<xmlattr>."VALUE,ObjectAttrIter->second);
						ObjectTree.add_child(ATTRIBUTE,childtree);
						ObjectAttrIter++;
					}

				}
				if(!ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.empty())
				{
					std::map<std::string,AttributeGroup>::const_iterator AttrGrpIter = ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.begin();
					while(AttrGrpIter != ObjectIter->second.m_ObjAttrbGrps.m_AttrbGroups.end())
					{
						ptree AttrGrptree;
						std::string AttrGrpID = AttrGrpIter->first;
						AttrGrptree.put("<xmlattr>."ID,AttrGrpID);
						std::map<std::string,std::string>::const_iterator childAttrIter = AttrGrpIter->second.m_Attrs.begin();
						while(childAttrIter != AttrGrpIter->second.m_Attrs.end())
						{
							ptree childtree;
							childtree.put("<xmlattr>."NAME,childAttrIter->first);
							childtree.put("<xmlattr>."VALUE,childAttrIter->second);
							AttrGrptree.add_child(ATTRIBUTE,childtree);
							childAttrIter++;
						}
						if(!AttrGrpIter->second.m_AttrbGroups.empty())
						{
							std::string AttrGrpPath = ATTRIBUTE_GROUP;
							PopulateAttrGrpsTree(AttrGrpIter->second.m_AttrbGroups,AttrGrpPath,AttrGrptree);
						}
						ObjectTree.add_child(ATTRIBUTE_GROUP,AttrGrptree);
						AttrGrpIter++;
					}
				}
				if(!ObjectIter->second.m_Groups.empty())
				{
					PopulateGroupstree(ObjectIter->second.m_Groups,ObjectTree);
				}
				GroupTree.add_child(OBJECT,ObjectTree);
				//GroupTree.add_child(OBJECT_PATH,ObjectTree);
				ObjectIter++;
			}
		}
		if(!ObjGroupsMapIter->second.m_References.empty())
		{
			ptree ReferenceTree;
			std::map<ReferenceType_t,Reference>::const_iterator ReferenceIter = ObjGroupsMapIter->second.m_References.begin();
			while(ReferenceIter != ObjGroupsMapIter->second.m_References.end())
			{
				ptree ReferenceTree;
				ReferenceTree.put("<xmlattr>."TYPE,ReferenceIter->first);
				ReferenceTree.put("<xmlattr>."SOURCE,ReferenceIter->second.m_refSource);

				std::map<std::string,std::string>::const_iterator ReferenceAttrIter = ReferenceIter->second.m_ReferenceAttrbs.begin();
				while(ReferenceAttrIter!= ReferenceIter->second.m_ReferenceAttrbs.end())
				{
					ptree childtree;
					childtree.put("<xmlattr>."NAME,ReferenceAttrIter->first);
					childtree.put("<xmlattr>."REFERS,ReferenceAttrIter->second);
					
					ReferenceTree.add_child(ATTRIBUTE,childtree);
					ReferenceAttrIter++;
				}
				
				ReferenceIter++;
			}
			GroupTree.add_child(REFERENCE_PATH,ReferenceTree);
		}
		
		//pushing groups in to Object
		ObjectTree.add_child(GROUP,GroupTree);
		ObjGroupsMapIter++;
	}
}