/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2019
+------------------------------------------------------------------------------------+
File    :   ProtectionState.h

Description :   ProtectionState class implementation.

+------------------------------------------------------------------------------------+
*/

#ifndef RCM_PROTECTION_STATE_H
#define RCM_PROTECTION_STATE_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <map>

#define RCM_PROTECTION_STATE_INDICATOR_DISABLED_HOSTID  "ProtectionDisabledHostId"

namespace RcmClientLib
{
    class ProtectionState
    {
    public:
        std::map<std::string, std::string>  m_stateIndicators;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ProtectionState", false);

            JSON_T(adapter, m_stateIndicators);
        }

        void serialize(ptree& node)
        {
            JSON_KV_P(node, m_stateIndicators);
        }
    };
}

#endif