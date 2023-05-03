/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. All rights reserved.
+------------------------------------------------------------------------------------+
File        :    CommonRcmContracts.h

Description    :   Contains definitions of corresponding Agent Upgrade

+------------------------------------------------------------------------------------+
*/

#ifndef _COMMON_RCM_CONTRACTS_H
#define _COMMON_RCM_CONTRACTS_H 

#include "json_writer.h"
#include "json_reader.h"

#include <boost/property_tree/ptree.hpp>


using boost::property_tree::ptree;

namespace RcmClientLib {

    /// \brief daily schedule for agent upgrade
    class TimeWindow {
    public:

        /// \brief Scheduled start hour during the day
        int StartHour;

        /// \brief Scheduled start minute in the specified start hour
        int StartMinute;

        /// \brief Scheduled end hour during the day
        int EndHour;

        /// \brief Scheduled end minute in the specified end hour
        int EndMinute;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "TimeWindow", false);

            JSON_E(adapter, StartHour);
            JSON_E(adapter, StartMinute);
            JSON_E(adapter, EndHour);
            JSON_T(adapter, EndMinute);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, StartHour);
            JSON_P(node, StartMinute);
            JSON_P(node, EndHour);
            JSON_P(node, EndMinute);
        }
    };
}
#endif // _COMMON_RCM_CONTRACTS_H
