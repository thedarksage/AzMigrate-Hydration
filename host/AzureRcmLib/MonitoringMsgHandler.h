/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File        :   MonitoringMessageHandler.h

Description :  Contains the monitoring chain of handlers.
+------------------------------------------------------------------------------------+
*/
#ifndef _MONITORING_MESSAGE_HANDLER_H
#define _MONITORING_MESSAGE_HANDLER_H


#include "Monitoring.h"
#include "MonitoringMsgSettings.h"
#include "RcmConfigurator.h"

#include "devicestream.h"

#include <boost/thread/mutex.hpp>

namespace RcmClientLib
{
    const int32_t BYTES_IN_MEGA_BYTE = 1024 * 1024;

    enum MonitoringMessageHandlers {
        SOURCE_AGENT,
        VIRTUAL_NODE,
        CLUSTER
    };

    class AbstractMonMsgPostToRcmHandler
    {

        /// \brief driver dev stream 
        int GetDriverDevStream(boost::shared_ptr<DeviceStream>& devStream);

        /*Note: Note that pending changes with driver are accurate while WOS is data or metadata.
             If bitmap changes are not completely read, this would skew. Drvutil used to have an explicit call for
             finding bitmap changes, it was removed due to some other issue. This is a limitation we need to remember for future reference.*/
        double GetPendingChangesAtSourceInMB(std::string& strDiskId, boost::shared_ptr<DeviceStream> pDeviceStream);

        /// \brief serialize the Azure to Azure health issues input
        std::string GetSerailizedHealthIssueInputForAzureToAzure(SourceAgentProtectionPairHealthIssues& issues);

        /// \brief serialize the onprem to azure health issues input
        std::string GetSerailizedHealthIssueInputForOnPremToAzure(SourceAgentProtectionPairHealthIssues& issues);

    protected:

        boost::shared_ptr<AbstractMonMsgPostToRcmHandler> nextInChain;

        /// \brief common code that would reduce duplications.
        bool ValidateSettingsAndGetPostUri(MonitoringMsgType msgType, 
            std::string& uri, 
            MonitoringMessageHandlers handler = SOURCE_AGENT);

        /// \brief serialize the health issues 
        std::string GetSerailizedHealthIssueInput(SourceAgentProtectionPairHealthIssues& issues);

        /// \brief resend the same messages in case hour has passed 
        bool IsMinResendTimePassed(const long long& lastMessageSendTime);

        /// \brief handlers filter out health issues they want to process
        SourceAgentProtectionPairHealthIssues FilterHealthIssues(
            SourceAgentProtectionPairHealthIssues& healthIssues,
            MonitoringMessageHandlers handler);

        bool FilterVolumeStats(const VolumesStats_t& stats,
            const DiskIds_t& diskIds,
            std::string& statsMsg,
            boost::function<bool(std::string)> predicate);

        SVSTATUS GetEventMessage(SV_ALERT_TYPE alertType,
            const InmAlert& alert,
            std::string& eventMesg);

    public:

        AbstractMonMsgPostToRcmHandler() {}

        virtual ~AbstractMonMsgPostToRcmHandler()
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\n", FUNCTION_NAME);
        }

        boost::shared_ptr<AbstractMonMsgPostToRcmHandler>& SetNextHandler(
            const boost::shared_ptr<AbstractMonMsgPostToRcmHandler>&);

        static bool IsPostMonitoringToRcmEnabled();

        /// \brief handles the health issues to rcm post requests 
        virtual bool HandleHealthIssues(
            SourceAgentProtectionPairHealthIssues& healthIssues) = 0;

        /// \brief handles the stats posts to rcm
        virtual bool HandleStats(
            const VolumesStats_t& stats,
            const DiskIds_t& diskIds) = 0;

        /// \brief handles alert post to rcm
        virtual bool HandleAlerts(
            SV_ALERT_TYPE alertType,
            SV_ALERT_MODULE alertingModule,
            const InmAlert& alert,
            const std::string& diskId = std::string()) = 0;

        static boost::shared_ptr<AbstractMonMsgPostToRcmHandler> GetHandlerChain();
    };
}

#endif