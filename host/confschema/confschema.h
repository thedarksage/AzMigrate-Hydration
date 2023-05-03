#ifndef CONF__SCHEMA__H__
#define CONF__SCHEMA__H__

#include <set>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <sstream>

#include "inmstrcmp.h"

/* should have a name space; 
* These names can easily clash and 
* are more logically confstore module */
namespace ConfSchema
{
    /* map from attr name to value */
    typedef std::map<std::string, std::string> Attrs_t;
    typedef Attrs_t::const_iterator ConstAttrsIter_t;
    typedef Attrs_t::iterator AttrsIter_t;

    struct Group;
    /* map from relation type to group */
    typedef std::string GroupType_t;
    typedef std::map<GroupType_t, Group> Groups_t;
    typedef Groups_t::iterator GroupsIter_t;
    typedef Groups_t::const_iterator ConstGroupsIter_t;

    struct AttributeGroup;
    /* map from attributegrp Id to AttributeGroup*/
    typedef std::map<std::string,AttributeGroup> AttributeGroups_t;
    struct AttributeGroup
    {
        Attrs_t m_Attrs;
        AttributeGroups_t m_AttrbGroups;
        bool operator==( AttributeGroup const&) const;
    };
    typedef AttributeGroups_t::const_iterator ConstAttrGrpIter_t;
    typedef AttributeGroups_t::iterator AttrGrpIter_t;

    /* if we dont have any key for object map we have to insert numerical 
    starts with 1 Ex:like object id =1,2,3...*/
    struct Object
    {
        std::string m_TimeStamp;
        Attrs_t m_Attrs;
        AttributeGroup m_ObjAttrbGrps;
        Groups_t m_Groups;
        bool operator==( Object const&) const;
        void Print(void);
    };
    typedef std::string ObjectId_t ;
    typedef std::map<ObjectId_t,Object> Object_t;
    typedef std::pair<const ObjectId_t,Object> ObjectPair_t;
    // typedef std::vector<Object> Objects_t;
    typedef Object_t::const_iterator ConstObjectsIter_t;
    typedef Object_t::iterator ObjectsIter_t;
    typedef Object_t::reverse_iterator ObjectsReverseIter_t;

    /* map from referer to reference */
    typedef std::map<std::string, std::string> ReferenceAttrs_t;
    typedef ReferenceAttrs_t::const_iterator ConstReferenceAttrsIter_t;
    typedef ReferenceAttrs_t::iterator ReferenceAttrsIter_t;
    struct Reference
    {
        /* upper layer should not use this */
        std::string m_Source;
        ReferenceAttrs_t m_ReferenceAttrs;
        bool operator==( Reference const&) const;
        void Print(void);
    };
    /* map from relation type to reference */
    typedef std::string ReferenceType_t; 
    typedef std::map<ReferenceType_t, Reference> References_t;
    typedef References_t::const_iterator ConstReferencesIter_t;
    typedef References_t::iterator ReferencesIter_t;

    struct Group
    {
        std::string m_GroupType;
        std::string m_Groupversion;
        Object_t m_Objects;
        References_t m_References;
        bool operator==( Group const&) const;
        void Print(void);
    };

    typedef std::set<std::string> GroupNames_t;

    struct Config 
    {
        std::string m_version;
        Groups_t m_groupsMap;
    };


    class EqAttr
    {
        const char *m_pName;
        const char *m_pValue;

    public:
        EqAttr(const char *pname, const char *pvalue)
        {
            m_pName = pname;
            m_pValue = pvalue;
        }
        bool operator()(const ObjectPair_t & objPair)
        {
            const Object & o = objPair.second;           
            ConstAttrsIter_t attriter = o.m_Attrs.find(m_pName);
            return ( attriter != o.m_Attrs.end()) && ( InmStrCmp<NoCaseCharCmp>(m_pValue,attriter->second.c_str()) == 0 ) ;
        }
    };
}

#endif /* CONF__SCHEMA__H__ */
