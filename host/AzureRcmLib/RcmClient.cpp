/*
+-------------------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+-------------------------------------------------------------------------------------------------+
File        :   RcmClient.cpp

Description :   RcmClient class implementation

+-------------------------------------------------------------------------------------------------+
*/

#include "MachineEntity.h"
#include "RcmJobs.h"
#include "RcmClient.h"
#include "RcmClientImpl.h"
#include "RcmClientAzureImpl.h"
#include "RcmClientProxyImpl.h"
#include "SourceComponent.h"
#include "ConverterErrorCodes.h"
#include "RcmConfigurator.h"
#include "RcmLibConstants.h"
#include "Converter.h"
#include "RcmClientUtils.h"
#include "AgentConfig.h"

#include "HttpClient.h"
#include "portablehelperscommonheaders.h"
#include "datacacher.h"
#include "biosidoperations.h"
#ifdef SV_WINDOWS
#include "InmageDriverInterface.h"
#endif
#include "json_writer.h"
#include "json_reader.h"
#include "inmageex.h"
#include "InstallerErrors.h"

#include <string>
#include <list>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/chrono.hpp>

using namespace boost::chrono;
using namespace RcmClientLib;
using namespace AzureStorageRest;

void mountBootableVolumesIfRequried();

namespace RcmClientLib
{

    static std::string const RcmProtectionStateFile("RcmProtectionState.json");

#ifdef WIN32
    __declspec(thread) RCM_CLIENT_STATUS RcmClient::m_errorCode;
#else
    __thread RCM_CLIENT_STATUS RcmClient::m_errorCode;
#endif

    RcmClient::RcmClient(RcmClientImplTypes implType)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        std::stringstream errMsg;

        if (implType == RcmClientLib::RCM_AZURE_IMPL)
        {
            m_pimpl.reset(new RcmClientAzureImpl());
        }
        else if (implType == RcmClientLib::RCM_PROXY_IMPL)
        {
            m_pimpl.reset(new RcmClientProxyImpl());
        }
        else
        {
            errMsg << "Unknown RCM client implementation type.";
            throw INMAGE_EX(errMsg.str().c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }

    RcmClient::~RcmClient()
    {
    }

    /// \brief register machine with RCM
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_INVALIDARG on input validation failure
    /// \li SVE_ABORT if exception is caught
    /// \li SVE_FAIL if fails to post request to RCM
    /// \li SVE_HTTP_RESPONSE_FAILED if request returned error
    SVSTATUS RcmClient::RegisterMachine(QuitFunction_t qf, bool update)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS ret = SVS_OK ;
        std::stringstream errMsg;
#ifdef SV_WINDOWS
        InmageDriverInterface       inmageDriverInterface;
#endif
        boost::mutex::scoped_lock guard(m_registerMachineMutex);

        std::string systemBiosId = GetSystemUUID();
        
#ifdef SV_WINDOWS
        //for windows, match only biosid, not the byteswapped bios id
        bool byteswappedBiosIDMatch = false;
#else
        // for linux, match biosid as well as byteswapped bios id
        bool byteswappedBiosIDMatch = boost::iequals(BiosID::GetByteswappedBiosID(systemBiosId), RcmConfigurator::getInstance()->GetConfiguredBiosId());
#endif
        if (!boost::iequals(systemBiosId, RcmConfigurator::getInstance()->GetConfiguredBiosId()) && !byteswappedBiosIDMatch)
        {
            std::stringstream errMsg;
            errMsg << "The system BIOS ID "
                << systemBiosId
                << " does not match configured BIOS ID "
                << RcmConfigurator::getInstance()->GetConfiguredBiosId();
            
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.str().c_str());
            m_errorCode = RCM_CLIENT_BIOSID_MISMATCH_ERROR;
            return SVE_FAIL;
        }

        try{
            /// hostname as defined in the conf file, if found
            std::string hostName;
            if (RcmConfigurator::getInstance()->getUseConfiguredHostname()) {
                hostName = RcmConfigurator::getInstance()->getConfiguredHostname();
            }
            else {
#ifdef SV_WINDOWS
                hostName = Host::GetInstance().GetFQDN();
#else
                hostName = Host::GetInstance().GetHostName();
#endif
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: using hostname = %s\n", FUNCTION_NAME, hostName.c_str());

            std::string managementId;
            bool bManagementIdEmpty = false;

            std::string clusterManagementId;
            bool bClusterContext = false;

            do{

                if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                    break;

                managementId = RcmConfigurator::getInstance()->GetManagementId();
                bManagementIdEmpty = managementId.empty();

                DebugPrintf(SV_LOG_ALWAYS, "%s: using managementId = %s\n", FUNCTION_NAME, managementId.c_str());

#ifdef SV_WINDOWS
                clusterManagementId = RcmConfigurator::getInstance()->GetClusterManagementId();
                if (clusterManagementId.empty())
                    break;

                bClusterContext = true;
                DebugPrintf(SV_LOG_ALWAYS, "%s: using clusterManagementId = %s\n", FUNCTION_NAME, clusterManagementId.c_str());
#endif
            } while (false);

            std::string biosId = RcmConfigurator::getInstance()->GetConfiguredBiosId();

            if (biosId.empty() || hostName.empty() || bManagementIdEmpty)
            {
                errMsg << "Insufficient information to register machine. "
                    << " hostname "
                    << hostName
                    << " biosid "
                    << biosId;

                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.str().c_str());

                if (biosId.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_BIOSID_ERROR;
                else if (bManagementIdEmpty)
                    m_errorCode = RCM_CLIENT_EMPTY_MANAGEMENTID_ERROR;
                else if (hostName.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_HOSTNAME_ERROR;

                return SVE_INVALIDARG;
            }

            static HypervisorInfo_t hypervinfo;
            if (HypervisorInfo_t::HypervisorStateUnknown == hypervinfo.state)
            {
                DebugPrintf(SV_LOG_DEBUG, "Determining Hypervisor information\n");
                bool bisvirtual = IsVirtual(hypervinfo.name, hypervinfo.version);
                hypervinfo.state = bisvirtual ? HypervisorInfo_t::HyperVisorPresent :
                    HypervisorInfo_t::HypervisorNotPresent;
            }

            std::string hypervname;
            if (hypervinfo.state != HypervisorInfo_t::HyperVisorPresent)
            {
                hypervname = PhysicalMachine;
            }
            else
            {
                if (hypervinfo.name == "VMware")
                    hypervname = HypervisorVendorVmware;
                else if ((hypervinfo.name == "Microsoft HyperV") ||
                    (hypervinfo.name == "Microsoft"))
                    hypervname = HypervisorVendorHyperV;
                else
                {
                    std::vector < std::string > supportedHypervisorlist;
                    std::string physicalSupportedHypervisors = RcmConfigurator::getInstance()->getPhysicalSupportedHypervisors();
                    boost::split(supportedHypervisorlist, physicalSupportedHypervisors, boost::is_any_of(","));

                    std::vector < std::string >::const_iterator itr = supportedHypervisorlist.begin();
                    for (/**/; itr != supportedHypervisorlist.end(); itr++)
                    {
                        std::string hypervisorName = *itr;
                        if (boost::iequals(hypervinfo.name, hypervisorName))
                        {
                            hypervname = PhysicalMachine;
                            DebugPrintf(SV_LOG_DEBUG, "Hypervisor=%s is supported for physical to azure \n", 
                                hypervisorName.c_str());
                            break;
                        }
                    }
                    
                    if (hypervname.empty())
                    {
                        errMsg << "Error: unknown hypervisor type " << hypervinfo.name;

                        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.str().c_str());
                        m_errorCode = RCM_CLIENT_INVALID_HYPERVISOR_TYPE;

                        return SVE_INVALIDARG;
                    }            
                }
            }

            const CpuInfos_t & cpuinfos = Host::GetInstance().GetCpuInfos();
            const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
            const ENDIANNESS & e = OperatingSystem::GetInstance().GetEndianness();
            const OS_VAL & osval = OperatingSystem::GetInstance().GetOsVal();

            std::string osType = OsTypes[osval];
            std::string osArch =
                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes,
                NSOsInfo::SYSTEMTYPE, true);

            if (boost::iequals(osType, "Windows") && boost::iequals(osArch, "x64-based PC") ||
                boost::iequals(osType, "Linux") && boost::iequals(osArch, "64-bit"))
                osArch = OsArchitectureTypex64;

            // support Windows 32-bit OS that are allowed by installer for migration to Azure
            if (!IsAgentRunningOnAzureVm() &&
                boost::iequals(osType, "Windows") &&
                boost::iequals(osArch, "X86-based PC"))
            {
                osArch = OsArchitectureTypex86;
            }

            if (osArch != OsArchitectureTypex64  && osArch != OsArchitectureTypex86)
            {
                errMsg << "Error: unsupported OS architecture " << osArch << " on " << osType;

                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.str().c_str());
                m_errorCode = RCM_CLIENT_INVALID_SYSTEMTYPE;

                return SVE_INVALIDARG;
            }

            std::string osVolDiskExtents = RcmClientUtils::GetAttributeValue(osinfo.m_Attributes,
                NSOsInfo::SYSTEMDRIVE_DISKEXTENTS);

            /// Mount bootable volumes so that their vol info is also collected
            mountBootableVolumesIfRequried();

            /// gather volume information
            std::string hypervisorName;
            LocalConfigurator lc;
            bool isMigrateToRcm = (lc.getMigrationState() == Migration::REGISTRATION_PENDING);

            VolumeSummaries_t volumeSummaries;
            VolumeDynamicInfos_t volumeDynamicInfos;
            VolumeInfoCollector volumeCollector((DeviceNameTypes_t)GetDeviceNameTypeToReport(hypervisorName, isMigrateToRcm));

            volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true);

            DataCacher dataCacher;
            if (!isMigrateToRcm)
            {
                if (dataCacher.Init())
                {
                    if (dataCacher.PutVolumesToCache(volumeSummaries))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "volume summaries are updated in local cache.\n");
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to cache the volume summaries.\n", FUNCTION_NAME);
                        m_errorCode = RCM_CLIENT_DATA_CACHER_ERROR;

                        return SVE_FAIL;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to init data cacher.\n", FUNCTION_NAME);
                    m_errorCode = RCM_CLIENT_DATA_CACHER_ERROR;

                    return SVE_FAIL;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Not updating/initializing data cache for migrate to rcm requests.\n", FUNCTION_NAME);
            }
            
            std::string protectionScenario = RcmConfigurator::getInstance()->IsAzureToAzureReplication() ?
                ProetctionScenario::AzureToAzure : RcmConfigurator::getInstance()->IsAzureToOnPremReplication() ?
                ProetctionScenario::AzureToOnPrem : ProetctionScenario::OnPremToAzure;

            std::string failoverVmDetectionId = RcmConfigurator::getInstance()->getFailoverVmDetectionId();

            // get the agent resource id
            std::string agentResourceId;
            if (!RcmConfigurator::getInstance()->UpdateResourceID()) {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to generate the agent resource id.\n", FUNCTION_NAME);
                m_errorCode = RCM_CLIENT_EMPTY_RESOURCEID_ERROR;
                return SVE_FAIL;
            }
            else {
                agentResourceId = RcmConfigurator::getInstance()->getResourceId();
            }

            MachineEntity me(RcmConfigurator::getInstance()->getHostId(),
                                hostName,
                                biosId,
                                managementId,
                                update ? RcmConfigurator::getInstance()->GetVacpCertFingerprint() : std::string(),
                                osType,
                                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::NAME),
                                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::CAPTION),
                                osArch,
                                OsEndiannessTypes[e],
                                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::MAJOR_VERSION),
                                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::MINOR_VERSION),
                                RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::BUILD),
                                IsUEFIBoot() ? RcmLibFirmwareTypes::RCMLIB_FIRMWARE_TYPE_UEFI : RcmLibFirmwareTypes::RCMLIB_FIRMWARE_TYPE_BIOS,
                                protectionScenario,
                                failoverVmDetectionId,
                                osVolDiskExtents,
                                hypervname,
                                (hypervinfo.version.empty()) ? "NA" : hypervinfo.version,
                                agentResourceId);

#ifdef SV_WINDOWS
            if (bClusterContext)
            {   
                ret = m_pimpl->AddClusterDiscoveryInfo(me);
                if (ret != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: AddClusterDiscoveryInfo failed with error : %d .\n", FUNCTION_NAME, m_pimpl->GetErrorCode());
                    m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();

                    return SVE_FAIL;
                }
            }
#endif
            ret = m_pimpl->AddDiskDiscoveryInfo(me, volumeSummaries, volumeDynamicInfos, bClusterContext);
            if (ret != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: AddDiskDiscoveryInfo failed with error : %d .\n",
                    FUNCTION_NAME,
                    m_pimpl->GetErrorCode());

                m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();

                return SVE_FAIL;
            }

            ret = m_pimpl->AddNetworkDiscoveryInfo(me);
            if (ret != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: AddNetworkDiscoveryInfo failed with error : %d .\n",
                    FUNCTION_NAME,
                    m_pimpl->GetErrorCode());

                m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();

                return SVE_FAIL;
            }

            // add hardware info
            me.ProcessorCount = 0;
            for (ConstCpuInfosIter_t iter = cpuinfos.begin(); iter !=  cpuinfos.end(); iter++)
            {
                std::string numcores = RcmClientUtils::GetAttributeValue(iter->second.m_Attributes, NSCpuInfo::NUM_CORES);
                std::string numlogprocs = RcmClientUtils::GetAttributeValue(iter->second.m_Attributes, NSCpuInfo::NUM_LOGICAL_PROCESSORS);

                DebugPrintf(SV_LOG_DEBUG, "number of cores=%s, logical processors=%s.\n", numcores.c_str(), numlogprocs.c_str());

                me.ProcessorCount += boost::lexical_cast<uint32_t>(numcores);
            }

            DebugPrintf(SV_LOG_DEBUG, "number of sockets=%d, total number of cores=%d.\n", cpuinfos.size(), me.ProcessorCount);

            me.MemoryInBytes = Host::GetInstance().GetAvailableMemory();

            unsigned long long sysVolFreeSpace = 0;
            unsigned long long sysVolCapacity = 0;

#ifdef SV_WINDOWS
            me.OperatingSystemFolder = RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::SYSTEMDIR);
            me.OperatingSystemVolumeName = RcmClientUtils::GetAttributeValue(osinfo.m_Attributes, NSOsInfo::SYSTEMDRIVE);
#else
            if (!GetSysVolCapacityAndFreeSpace(me.OperatingSystemVolumeName, me.OperatingSystemFolder, sysVolFreeSpace, sysVolCapacity))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: GetSysVolCapacityAndFreeSpace failed.\n", FUNCTION_NAME);
                return SVE_FAIL;
            }
#endif
            DebugPrintf(SV_LOG_DEBUG,
                "%s: System Folder = %s, System Volume = %s.\n",
                FUNCTION_NAME,
                me.OperatingSystemFolder.c_str(),
                me.OperatingSystemVolumeName.c_str());

            if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
                me.IsCredentialLessDiscovery = RcmConfigurator::getInstance()->getIsCredentialLessDiscovery();
            else
                me.IsCredentialLessDiscovery = false;

            DebugPrintf(SV_LOG_DEBUG,
                "%s: Is Credential Less Discovery = %s.\n",
                FUNCTION_NAME,
                me.IsCredentialLessDiscovery ? "True" : "False");

            // serialize the Machine entity
            std::string strMachine = JSON::producer<MachineEntity>::convert(me);


            // retry the registration if it is periodic update
            const int RETRY_INTERVAL_IN_SECONDS = 60;
            const uint32_t  MAX_TIME_RETRIES_IN_SECONDS = 300;
            bool shouldRetry = false;

            steady_clock::time_point endTime = steady_clock::now();
            endTime += seconds(MAX_TIME_RETRIES_IN_SECONDS);

            DebugPrintf(SV_LOG_DEBUG, "%s registermachine input is: \n%s\n",
                FUNCTION_NAME,
                strMachine.c_str());

            do {
                std::string output;

                ret = m_pimpl->Post(update ? MODIFY_MACHINE_URI : REGISTER_MACHINE_URI,
                    strMachine,
                    output,
                    update ? std::string() : RcmConfigurator::getInstance()->GetClientRequestId());

                if (ret != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s failed with error : %d. Input : \n%s\n",
                        FUNCTION_NAME,
                        m_pimpl->GetErrorCode(),
                        strMachine.c_str());

                    m_errorCode = (RCM_CLIENT_STATUS) m_pimpl->GetErrorCode();

                    if (update && (m_errorCode == RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR))
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s: Immediate retry with interval %d seconds will occur in case \
host did not register atleast once.\n", FUNCTION_NAME, RETRY_INTERVAL_IN_SECONDS);

                        shouldRetry = (steady_clock::now() < endTime);
                    }
                    else
                    {
                        // if this is a periodic registration wait before a rediscovery
                        if (update && qf )
                            qf(RETRY_INTERVAL_IN_SECONDS);

                        shouldRetry = false;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "RegisterMachine succeeded\n");
                    shouldRetry = false;
                }
            } while (shouldRetry && qf && !qf(RETRY_INTERVAL_IN_SECONDS));
        }
        catch( std::string const& e ) {
            ret = SVE_ABORT;
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception %s\n", FUNCTION_NAME, e.c_str());
        }
        catch( char const* e ) {
            ret = SVE_ABORT;
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception %s\n", FUNCTION_NAME, e);
        }
        catch( std::exception const& e ) {
            ret = SVE_ABORT;
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception %s\n", FUNCTION_NAME, e.what());
        }
        catch(...) {
            ret = SVE_ABORT;
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception...\n", FUNCTION_NAME);
        }

        if (ret == SVE_ABORT)
            m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return ret;
    }


    /// \brief register source agent with RCM
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_INVALIDARG on input validation failure
    /// \li SVE_ABORT if exception is caught
    /// \li SVE_FAIL if fails to post request to RCM
    /// \li SVE_HTTP_RESPONSE_FAILED if request returned error
    SVSTATUS RcmClient::RegisterSourceAgent()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {
            std::string hostName;
            if (RcmConfigurator::getInstance()->getUseConfiguredHostname()) {
                hostName = RcmConfigurator::getInstance()->getConfiguredHostname();
            }
            else {
#ifdef SV_WINDOWS
                hostName = Host::GetInstance().GetFQDN();
#else
                hostName = Host::GetInstance().GetHostName();
#endif
            }

            std::string agentVersion(InmageProduct::GetInstance().GetProductVersion());
            std::string driverVersion(InmageProduct::GetInstance().GetDriverVersion());
            std::string installPath = RcmConfigurator::getInstance()->getInstallPath();
            std::string distroName = RcmClientUtils::GetAttributeValue(
                OperatingSystem::GetInstance().GetOsInfo().m_Attributes, NSOsInfo::DISTRO_NAME);
            std::string protectionScenario;
            std::vector<std::string> supportedJobs;
            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
            {
                protectionScenario = ProetctionScenario::AzureToAzure;
                supportedJobs.assign(RcmJobTypes::AzureToAzureSupportedJobs, 
                    RcmJobTypes::AzureToAzureSupportedJobs + sizeof(RcmJobTypes::AzureToAzureSupportedJobs) / sizeof(std::string));
            }
            else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
            {
                protectionScenario = ProetctionScenario::AzureToOnPrem;
                supportedJobs.assign(RcmJobTypes::AzureToOnPremSupportedJobs, 
                    RcmJobTypes::AzureToOnPremSupportedJobs + sizeof(RcmJobTypes::AzureToOnPremSupportedJobs) / sizeof(std::string));
            }
            else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
            {
                protectionScenario = ProetctionScenario::OnPremToAzure;
                supportedJobs.assign(RcmJobTypes::OnPremToAzureSupportedJobs, 
                    RcmJobTypes::OnPremToAzureSupportedJobs + sizeof(RcmJobTypes::OnPremToAzureSupportedJobs) / sizeof(std::string));
            }

            if (hostName.empty() ||
                agentVersion.empty() ||
                driverVersion.empty() ||
                installPath.empty() ||
                distroName.empty() ||
                protectionScenario.empty())
            {
                std::stringstream errMsg;
                errMsg << "Insufficient information to register source agent. "
                    << " hostname "
                    << hostName
                    << " installpath "
                    << installPath
                    << " agentversion "
                    << agentVersion
                    << " driverversion "
                    << driverVersion
                    << " distroName "
                    << distroName
                    << " protectionScenario "
                    << protectionScenario;

                DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());

                if (agentVersion.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_AGENT_VERSION_ERROR;
                else if (driverVersion.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_DRIVER_VERSION_ERROR;
                else if (hostName.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_HOSTNAME_ERROR;
                else if (installPath.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_INSTALLPATH_ERROR;
                else if (distroName.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_DISTRO_NAME_ERROR;
                else if (protectionScenario.empty())
                    m_errorCode = RCM_CLIENT_EMPTY_PROTECTION_SCENARIO_ERROR;

                return SVE_INVALIDARG;
            }

            SourceAgent sc(RcmConfigurator::getInstance()->getHostId(),
                hostName,
                agentVersion,
                driverVersion,
                installPath,
                distroName,
                protectionScenario,
                supportedJobs);

            std::string strSrcAgent = JSON::producer<SourceAgent>::convert(sc);

            std::string output;
            status = m_pimpl->Post(REGISTER_SOURCE_AGENT_URI,
                strSrcAgent,
                output,
                RcmConfigurator::getInstance()->GetClientRequestId());

            if (SVS_OK != status)
            {
                DebugPrintf(SV_LOG_ERROR, "RegisterSourceAgent failed with  %d\n",
                    m_pimpl->GetErrorCode());
                m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
            }
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status  == SVE_ABORT)
            m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    /// \brief unregister source agent with RCM
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_INVALIDARG on input validation failure
    /// \li SVE_ABORT if exception is caught
    /// \li SVE_FAIL if fails to post request to RCM
    /// \li SVE_HTTP_RESPONSE_FAILED if request returned error
    SVSTATUS RcmClient::UnregisterMachine()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        boost::mutex::scoped_lock guard(m_registerMachineMutex);

        std::string strSrcAgent = "\"";
        strSrcAgent += RcmConfigurator::getInstance()->getHostId();
        strSrcAgent += "\"";

        std::string output;
        status = m_pimpl->Post(UNREGISTER_MACHINE_URI,
            strSrcAgent,
            output,
            RcmConfigurator::getInstance()->GetClientRequestId());

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    /// \brief unregister source agent with RCM
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_INVALIDARG on input validation failure
    /// \li SVE_ABORT if exception is caught
    /// \li SVE_FAIL if fails to post request to RCM
    /// \li SVE_HTTP_RESPONSE_FAILED if request returned error
    SVSTATUS RcmClient::UnregisterSourceAgent()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string strSrcAgent = "\"";
        strSrcAgent += RcmConfigurator::getInstance()->getHostId();
        strSrcAgent += "\"";

        std::string output;
        status = m_pimpl->Post(UNREGISTER_SOURCE_AGENT_URI,
            strSrcAgent,
            output,
            RcmConfigurator::getInstance()->GetClientRequestId());

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    /// \brief sends request to RCM to create an auto upgrade job
    SVSTATUS RcmClient::InitiateOnPremToAzureAgentAutoUpgrade()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string output;
        status = m_pimpl->Post(INITIATE_ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE_URI,
            std::string(),
            output);

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    /// \brief return error code
    int32_t RcmClient::GetErrorCode()
    {
        return m_errorCode;
    }

    SVSTATUS RcmClient::GetReplicationSettings(std::string& serializedSettings,
        const std::map<std::string, std::string>& headers)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string strSrcAgent = "\"";
        strSrcAgent += RcmConfigurator::getInstance()->getHostId();
        strSrcAgent += "\"";

        if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
        {
            status = m_pimpl->PostWithHeaders(GET_ON_PREM_TO_AZURE_SOURCE_AGENT_SETTINGS_URI,
                strSrcAgent, serializedSettings, std::string(), headers);
        }
        else
        {
            status = m_pimpl->Post(
                RcmConfigurator::getInstance()->IsAzureToAzureReplication() ?
                GET_AZURE_TO_AZURE_SOURCE_AGENT_SETTINGS_URI : GET_AZURE_TO_ON_PREM_SOURCE_AGENT_SETTINGS_URI,
                strSrcAgent, serializedSettings);
        }
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::GetReplicationSettings(std::string& serializedSettings)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::map<std::string, std::string> headers;
        status = GetReplicationSettings(serializedSettings, headers);
 
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::GetRcmDetails(std::string& serializedSettings)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string strSrcAgent = "\"";
        strSrcAgent += RcmConfigurator::getInstance()->getHostId();
        strSrcAgent += "\"";

        status = m_pimpl->Post(GET_RCM_DETAILS,
            strSrcAgent, serializedSettings);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::UpdateAgentJobStatus(RcmJob& job)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {

            std::string strJob = JSON::producer<RcmJob>::convert(job);

            std::string output;
            status = m_pimpl->Post(POST_AGENT_JOB_STATUS_URI,
                strJob,
                output,
                job.Context.ClientRequestId);

            if (status != SVS_OK)
            {
                m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
            }
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::GetInstallerError(RcmJob& job, int exitcode, const std::string& jsonErrorFile)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        // Update the job status
        if (exitcode == AGENT_INSTALLATION_SUCCESS ||                           // Success
            exitcode == AGENT_INSTALL_WINDOWS_RECOMMENDED_REBOOT ||             // Windows recommended reboot
            exitcode == AGENT_INSTALL_LINUX_RECOMMENDED_REBOOT ||               // Linux recommended reboot
            exitcode == AGENT_INSTALL_VSS_INSTALLATION_FAILED_WITH_WARNINGS)    // Succeeded with warnings
        {
            job.JobStatus = RcmJobStatus::SUCCEEDED;
        }
        else
        {
            job.JobStatus = RcmJobStatus::FAILED;
        }
        DebugPrintf(SV_LOG_INFO, "Upgrade job status : %s, exit code %d\n", job.JobStatus.c_str(), exitcode);

        // Update the errors
        if (exitcode != AGENT_INSTALLATION_SUCCESS)
        {
            // Update only the internal errors
            if (exitcode == AGENT_INSTALL_VSS_INSTALLATION_FAILED_WITH_ERRORS ||
                exitcode == AGENT_INSTALL_VSS_INSTALLATION_FAILED_WITH_WARNINGS ||
                exitcode == AGENT_INSTALL_PREREQS_FAIL)
            {
                std::ifstream ifs(jsonErrorFile.c_str());
                if (!ifs.good()) 
                {
                    DebugPrintf(SV_LOG_ERROR, "Unable to open the file %s\n", jsonErrorFile.c_str());
                    return SVE_FAIL;
                }

                std::string installerErrors((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
                ExtendedErrors extendedErrors =
                    JSON::consumer<ExtendedErrors>::convert(installerErrors);

                for(std::vector<ExtendedError>::iterator eeitr = extendedErrors.Errors.begin();
                    eeitr != extendedErrors.Errors.end(); eeitr++)
                {
                    ExtendedError extendedError = *eeitr;
                    RcmJobError rcmError;
                    rcmError.ErrorCode = extendedError.error_name;
                    rcmError.Message = extendedError.default_message;
                    rcmError.Severity = "Error";
                    rcmError.MessageParameters.insert(
                        extendedError.error_params.begin(),
                        extendedError.error_params.end());

                    rcmError.ComponentErrorObj.ComponentId = "SourceAgent";
                    rcmError.ComponentErrorObj.ErrorCode = extendedError.error_name;
                    rcmError.ComponentErrorObj.Message = extendedError.default_message;
                    rcmError.ComponentErrorObj.Severity = "Error";
                    rcmError.ComponentErrorObj.MessageParameters.insert(
                        extendedError.error_params.begin(),
                        extendedError.error_params.end());

                    job.Errors.push_back(rcmError);
                }
            }
            else
            {
                std::string mappedErrorCode = GetInstallerErrorString(exitcode);
                if (mappedErrorCode == std::string())
                {
                    mappedErrorCode = boost::lexical_cast<std::string>(exitcode);
                }
                RcmJobError error;
                error.ErrorCode = mappedErrorCode;
                error.Severity = "Error";
                error.MessageParameters = std::map<std::string, std::string>();

                job.Errors.push_back(error);
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "Job Error count : %d\n", job.Errors.size());

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return SVS_OK;
    }

    RcmJob RcmClient::CreateRcmJobDetails(int exitCode,
        const std::string& jobDetails, const std::string& jsonErrorFile)
    {
        std::ifstream ifs(jobDetails.c_str());
        if (!ifs.good())
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to open the file %s\n", jobDetails.c_str());
            throw std::runtime_error("Unable to open the file " + jobDetails);
        }
        std::string jobstr((std::istreambuf_iterator<char>(ifs)),
            std::istreambuf_iterator<char>());
        RcmJob job = JSON::consumer<RcmJob>::convert(jobstr);
        GetInstallerError(job, exitCode, jsonErrorFile);
        return job;
    }

    SVSTATUS RcmClient::CompleteAgentUpgradeJob(int exitCode, const std::string& jobDetails, const std::string& jsonErrorFile)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {

            RcmJob job = CreateRcmJobDetails(exitCode, jobDetails, jsonErrorFile);
            status = UpdateAgentJobStatus(job);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "Update agent job status failed\n");
            }
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::RenewBlobContainerSasUri(const std::string& diskId,
        const std::string& containerType,
        std::string& response,
        const bool& isClusterVirtualNodeContext,
        const std::string &clientRequestId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string input = diskId;
        std::string api;
        bool isClusterVirtualNodeEnabled = false;
        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            api = RENEW_LOG_CONTAINER_SAS_URI;
            isClusterVirtualNodeEnabled = isClusterVirtualNodeContext;
        }
        else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            if (containerType == RenewRequestTypes::DiffSyncContainer)
            {
                api = RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_DIFFSYNC;
            }
            else if (containerType == RenewRequestTypes::ResyncContainer)
            {
                api = RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_RESYNC;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: unsupported renew request type %s.\n", FUNCTION_NAME, containerType.c_str());
                return SVE_FAIL;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: unsupported replication scenario.\n", FUNCTION_NAME);
            return SVE_FAIL;
        }

        std::map<std::string, std::string> headers;
        headers.insert(std::make_pair(X_MS_AGENT_FORCERENEWSASURI, "true"));
        status = m_pimpl->PostWithHeaders(api, input, response, clientRequestId, headers, isClusterVirtualNodeEnabled);

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::ApplianceLogin(const RcmRegistratonMachineInfo& machineInfo,
        std::string& response) const
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        AgentConfigurationMigrationInput loginInput;       
        loginInput.BiosId = GetSystemUUID();

#ifdef SV_WINDOWS
       loginInput.Fqdn = Host::GetInstance().GetFQDN();
#else
        loginInput.Fqdn = Host::GetInstance().GetHostName();
#endif
        
        DebugPrintf(SV_LOG_ALWAYS, "%s: using biosId = %s, and fqdn = %s \n", FUNCTION_NAME, 
            loginInput.BiosId.c_str(), loginInput.Fqdn.c_str());

        const std::string serializedInput =
            JSON::producer<AgentConfigurationMigrationInput>::convert(loginInput);
        
        status = m_pimpl->ApplianceLogin(machineInfo.getApplianceAddresses(), serializedInput,
            response, machineInfo.RcmProxyPort, machineInfo.Otp,
            CREATE_MOBILITY_AGENT_CONFIG_MIGRATION_TO_RCM_STACK,
            machineInfo.ClientRequestId);

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: RCM client error code = %d, status = %d\n",
                FUNCTION_NAME, m_errorCode, status);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::ALRApplianceLogin(const ALRMachineConfig& alrMachineConfig,
        std::string& response) const
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        do
        {
            CreateMobilityAgentConfigForOnPremAlrMachineInput alrConfigInput(
                RcmConfigurator::getInstance()->getResourceId(),
                alrMachineConfig.OnPremMachineBiosId,
                RcmConfigurator::getInstance()->GetFqdn());

            const std::string serializedInput =
                JSON::producer<CreateMobilityAgentConfigForOnPremAlrMachineInput>::convert(
                alrConfigInput);
            DebugPrintf(SV_LOG_ALWAYS, "%s: Using serialized input : %s \n", FUNCTION_NAME,
                serializedInput.c_str());

            std::vector<std::string> rcmProxyAddr;
            std::string port;
            uint32_t uiport;
            status = RcmConfigurator::getInstance()->GetRcmProxyAddress(
                rcmProxyAddr, uiport);
            if (status != SVS_OK)
            {
                break;
            }

            port = boost::lexical_cast<std::string>(uiport);
            status = m_pimpl->ApplianceLogin(rcmProxyAddr, serializedInput,
                response, port, alrMachineConfig.Passphrase,
                CREATE_MOBILITY_AGENT_CONFIG_FOR_ON_PREM_ALR_MACHINE);

            if (status != SVS_OK)
            {
                m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
                DebugPrintf(SV_LOG_ERROR, "%s: RCM client errorcode = %d, status = %d\n",
                    FUNCTION_NAME, m_errorCode, status);
            }
        } while(false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::CsLogin(const std::string &ipaddr,
        uint32_t uiPort)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string strPort = boost::lexical_cast<std::string>(uiPort);

        status = m_pimpl->CsLogin(ipaddr, strPort);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::VerifyClientAuth(const RcmProxyTransportSetting &rcmProxyTransportSettings,
        bool notUsed)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        status = m_pimpl->VerifyClientAuth(rcmProxyTransportSettings);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::RenewClientCertDetails(std::string& serializedSettings)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string strSrcAgent = AgentConfigHelpers::GetAgentConfigInputDecoded();
        status = m_pimpl->Post(CREATE_MOBILITY_AGENT_CONFIG, strSrcAgent, serializedSettings);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::CompleteSync(const std::string &input)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK; 
        std::string output;
        std::string uri;

        if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            uri = AZURE_TO_ONPREM_COMPLETE_SYNC_URI;
        }
        else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
        {
            uri = ONPREM_TO_AZURE_COMPLETE_SYNC_URI;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: unsupported resync complete scenario.\n", FUNCTION_NAME);
            return SVE_FAIL;
        }

        status = m_pimpl->Post(uri, input, output);

        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClient::PostAgentMessage(const std::string& endPoint,
        const std::string &serializedMessageInput,
        const std::map<std::string, std::string>& propertyMap)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        status = m_pimpl->PostAgentMessage(endPoint, serializedMessageInput, propertyMap);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)m_pimpl->GetErrorCode();
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }
}
