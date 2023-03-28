#ifndef MIGRATION_HELPER_H
#define MIGRATION_HELPER_H

#pragma once

#include "CommitTag.h"
#include "host.h"
#include "RcmConfigurator.h"
#include "svtypes.h"
#include "ErrorLogger.h"
#include "MigrationPolicies.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <ace/Thread_Manager.h>
#include <ace/Event.h>


#define INM_CLOUD_ERROR_CODE    "ErrorCode"
#define INM_CLOUD_ERROR_MSG     "ErrorMessage"
#define VM_NAME                 "VmName"
#define APPLIANCE_FQDN          "FQDN"

namespace Migration {

    //ErrorName
    const std::string DRIVER_NOT_SUPPORTING_BLOCK_DRAIN_TAG = "DriverNotSupportingBlockDrainTag";
    const std::string DISK_NOT_IN_WO_MODE = "DiskNotInWOMode";
    const std::string VERIFY_PAIR_FAILED = "VerifyPairFailed";
    const std::string RCM_REGISTRATION_POLICY_FAILURE = "RcmRegistrationPolicyFailure";
    const std::string REGISTER_APPLIANCE_INITIALIZATION_FAILED =
        "RegisterApplianceInitializationFailed";
    const std::string SOURCE_AGENT_UNABLE_TO_CONNECT_WITH_APPLIANCE =
        "SourceAgentUnableToConnectWithAppliance";
    const std::string FINAL_REPLICATION_CYCLE_FAILURE = "FinalReplicationCyclePolicyFailure";
    const std::string RESUME_REPLICATION_FAILURE = "ResumeReplicationPolicyFailure";
    const std::string RCM_MIGRATION_POLICY_FAILURE = "RcmMigrationPolicyFailure";

}

class MigrationHelper {
public:
    MigrationHelper(bool initRequired = false);
    ~MigrationHelper() {};

    SVSTATUS CreateDriverStream(std::string &errMsg);

    std::string GetErrorMessage() { return m_errMsg; }

    SVSTATUS RegisterAgent(const RcmRegistratonMachineInfo& machineInfo,
        ExtendedErrorLogger::ExtendedErrors& extendedErrors);

    SVSTATUS RequestVacp(VacpConf::State requestState, VacpConf::State finalState,
        std::string& errMsg);

    SVSTATUS RequestStartVacp(std::string& errMsg);

    SVSTATUS RequestStopVacp(std::string& errMsg);

    SVSTATUS BlockDrainTag(const RcmFinalReplicationCycleInfo& replicationCycleInfo,
        const std::string& policyId,
        const std::vector<std::string>& protectedDiskIds,
        ExtendedErrorLogger::ExtendedErrors& extendedErrors);

    void IssueCommitTagNotify(const RcmFinalReplicationCycleInfo& replicationCycleInfo,
        const std::vector<std::string>& protectedDiskIds);

    void GetPnameDiskList(const std::set<std::string>& protectedDiskIds,
        std::vector<std::string>& pnameDiskIds);

#ifdef SV_UNIX
    SVSTATUS UpdatePNameInBookmarkOutput();
#endif

    SVSTATUS UnblockDrain(const std::vector<std::string> &pnameDiskIds,
        std::stringstream &errMsg);

    SVSTATUS FinalReplicationCycle(const RcmFinalReplicationCycleInfo& replicationCycleInfo,
        const std::string& policyId, const std::set<std::string>& protectedDiskIds,
        ExtendedErrorLogger::ExtendedErrors& extendedErrors);

#ifdef SV_UNIX
    SVSTATUS ModifyPName(const std::set<std::string>& protectedDiskIds,
        const std::vector<std::string>& pnameDiskIds, std::stringstream &strErrMsg);
#endif

    SVSTATUS ResumeReplication(const std::set<std::string>& protectedDiskIds,
        ExtendedErrorLogger::ExtendedErrors& extendedErrors);

    SVSTATUS Migrate(ExtendedErrorLogger::ExtendedErrors &extendedErrors);

private:
    SVSTATUS initializeRcmSettings(const RcmRegistratonMachineInfo& machineInfo, std::string& errMsg);

    std::string m_errMsg;

    ACE_Thread_Manager m_threadManager;

    boost::shared_ptr<ACE_Event> m_QuitEvent;

    DeviceStream::Ptr m_pDriverStream;

    TagCommitNotifier m_commitTag;
};

#endif
