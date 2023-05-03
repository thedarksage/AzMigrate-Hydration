/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	DiskGroupEntity.h

Description	:   DiskGroupEntity class is defind as per the RCM DiskGroupInput class.
The member names should match that defined in the RCM DiskGroupInput class. 
Uses the ESJ JSON formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/
#ifndef _DISK_GROUP_ENTITY_H
#define _DISK_GROUP_ENTITY_H

#include "json_writer.h"
#include "json_reader.h"
#include <string>
#include <boost/shared_ptr.hpp>

namespace RcmClientLib
{
    /// \brief represents a disk group
    class DiskGroupEntity 
    {
    public:
        /// \brief an id to uniquely identify disk group in a system
        std::string  Id;

        DiskGroupEntity() { }

        DiskGroupEntity(const std::string& id)
            :Id(id)
        {

        }

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "DiskGroupEntity", false);

            JSON_T(adapter,Id);
        }
    };

    typedef boost::shared_ptr<DiskGroupEntity>   DiskGroupEntityPtr_t;
    typedef std::vector<DiskGroupEntity> DiskGroupEntities_t;

}
#endif
