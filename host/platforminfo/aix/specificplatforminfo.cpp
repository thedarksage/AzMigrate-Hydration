#include <string>
#include <sstream>
#include <odmi.h>
#include <sys/cfgodm.h>
#include <sys/cfgdb.h>
#include <sys/sysmacros.h>
#include "specificplatforminfo.h"

SpecificPlatformInfo::SpecificPlatformInfo()
{
    /* TODO: better way altogther upto return value */
    odm_initialize();
}


SpecificPlatformInfo::~SpecificPlatformInfo()
{
    odm_terminate();
}


std::string SpecificPlatformInfo::GetCustomizedAttributeValue(const std::string &name, const std::string &attribute, std::string &errmsg)
{
    std::string value;

    struct CuAt stCuAt;
    std::string criteria;
    criteria = "name = \'";
    criteria += name;
    criteria += '\'';
    criteria += " and attribute = \'";
    criteria += attribute;
    criteria += '\'';
    struct CuAt *ret = (struct CuAt *)odm_get_obj(CuAt_CLASS, (char *)criteria.c_str(), &stCuAt, ODM_FIRST);
    if (ret == ( struct CuAt *)-1)
    {
        std::stringstream ss;
        ss << odmerrno;
        errmsg = "querying value from odm for name ";
        errmsg += name;
        errmsg += " and attribute ";
        errmsg += attribute;
        errmsg += " failed with odm error ";
        errmsg += ss.str();
    }
    else if (ret == 0)
    {
        errmsg = std::string("value does not exist in odm for name ") + name + " and attribute " + attribute;
    }
    else
    {
        value = stCuAt.value;
    }

    return value;
}


std::string SpecificPlatformInfo::GetCustomizedDeviceParent(const std::string &device, std::string &errmsg)
{
    std::string parent;

    struct CuDv stCuDv;
    std::string criteria;
    criteria = "name = \'";
    criteria += device;
    criteria += '\'';
    struct CuDv *ret = (struct CuDv *)odm_get_obj(CuDv_CLASS, (char *)criteria.c_str(), &stCuDv, ODM_FIRST);
    if (ret == ( struct CuDv *)-1)
    {
        std::stringstream ss;
        ss << odmerrno;
        errmsg = "querying parent from odm for ";
        errmsg += device;
        errmsg += " failed with odm error ";
        errmsg += ss.str();
    }
    else if (ret == 0)
    {
        errmsg = std::string("parent does not exist in odm for ") + device;
    }
    else
    {
        parent = stCuDv.parent;
    }

    return parent;
}
