#ifndef CONF_SCHEMA_XMLPARSER_H
#define CONF_SCHEMA_XMLPARSER_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>


typedef std::map<std::string, std::string> Attributes_t;
typedef std::string ObjectId_t ;
struct Group ;
typedef std::string GroupType_t; 
typedef std::map<GroupType_t,Group> Group_t;

typedef std::map<std::string,std::string> ReferenceAttrbs_t;

struct AttributeGroup;
typedef std::map<std::string,AttributeGroup> AttributeGroups_t;
struct AttributeGroup
{
  Attributes_t m_Attrs;
  AttributeGroups_t m_AttrbGroups;
};

struct Object
{
	std::string m_TimeStamp;
    Attributes_t m_Attrs;
	AttributeGroup m_ObjAttrbGrps;
    Group_t m_Groups;
};

typedef std::map<ObjectId_t,Object> object_t;

struct Reference
{
   std::string m_refSource;
   ReferenceAttrbs_t m_ReferenceAttrbs;
};
typedef std::string ReferenceType_t; 
typedef std::map<ReferenceType_t,Reference> References_t;

struct Group
{ 
 std::string m_GroupType;
 std::string m_Groupversion;
 object_t m_Objects;
 References_t m_References;
};

struct Config 
{
 std::string m_version;
 std::list<Group> m_grplist;
};

class ConfSchemaValueObject
{
	Config m_Config;
	void ParseConffileToLoad(boost::property_tree::ptree& pt);
	void ParseConfigObjectForSave(boost::property_tree::ptree& ConfigTree);
	
	void PopulateAttributegrpsInfo(boost::property_tree::ptree::value_type& v,AttributeGroup& m_ParentAttrgrpObj);
	//PoplulateGroupsInfo(boost::property_tree::ptree::value_type& v,Group& ParentGrpObj);
	void PoplulateGroupsInfo(boost::property_tree::ptree::value_type& v,Object& ParentObj);

	void PopulateAttrGrpsTree(const std::map<std::string,AttributeGroup>& AttrGrps,std::string& AttrGrpTreePath,boost::property_tree::ptree& AttrGrptree);
	void PopulateGroupstree(const std::map<GroupType_t,Group>& ObjGroupsMap,boost::property_tree::ptree& ObjectTree);

public:
	ConfSchemaValueObject();
	~ConfSchemaValueObject();
	void XmlConfileLoad(const std::string &filename);
	void XmlConfileLoad(std::stringstream &XmlStream);

	void XmlConfileSave(const std::string &filename);
	void XmlConfileSave(std::stringstream &XmlStream);
};


#endif /* confschema.h */