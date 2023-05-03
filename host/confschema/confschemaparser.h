#ifndef CONF_SCHEMA_PARSER_H
#define CONF_SCHEMA_PARSER_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

#include "confschema.h"
//Example of one schema file
//<Config version="1.0">
//	<Group Type="Policy" version="1.0">
//		<Object Id = "1" Timestamp = "" >
//			<Attribute Name = "id"   Value = ""/> 
//			<Attribute Name = "type" Value = ""/> 
//			<Attribute Name = "policyTimestamp"   Value = ""/>
//		</Object>
//	</Group>
//	<Group Type="PolicyInstance" version="1.0">
//		<Object Id = "1" Timestamp = "" >
//			<Attribute Name = "uniqueId"   Value = ""/>
//			<Attribute Name = "dependentInstanceId"   Value = ""/>
//			<Attribute Name = "scenarioId" Value = "" />
//		</Object>
//		<Reference Type="Policy" source="."> 
//			<Attribute Name="policyId" Refers="Id" />   
//		</Reference>
//	</Group>
//</Config>
//
namespace ConfSchema
{
	#define XMLATTR		"xmlattr"
	#define CONFIGTAG	"Config"
	#define VERSION		"Version"
	#define GROUP_TAG	"Group"
	#define OBJECT		"Object"
	#define TYPE		"Type"
	#define ID			"Id"
	#define TIMESTAMP	"Timestamp"
	#define NAME		"Name"
	#define VALUE		"Value"
	#define	ATTRIBUTE	"Attribute"
	#define ATTRIBUTE_GROUP	"AttributeGroup"
	#define REFERENCE	"Reference"
	#define SOURCE		"Source"
	#define REFERS		"Refers"

	#define GROUP_PATH		"Config.Group"
	#define OBJECT_PATH		"Config.Group.Object"
	#define REFERENCE_PATH	"Config.Group.Reference"

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
		void XmlConfileLoad(const std::string &filename);
		void XmlConfileLoad(std::stringstream &XmlStream);

		void XmlConfileSave(const std::string &filename);
		void XmlConfileSave(std::stringstream &XmlStream);

		std::string getSchemaVersion() const ;
		Groups_t getGroupMap() const;
		void setSchemaVersion(std::string const& Version);
		void setGroupMap(const Groups_t& GrpMapObj);

	};
}
#endif /* CONFSCHEMAPARSER.H */
