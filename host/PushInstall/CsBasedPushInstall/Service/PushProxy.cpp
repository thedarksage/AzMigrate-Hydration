
#include "CsCommunicationImpl.h"
#include "PushProxy.h"
#include "setpermissions.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc

using namespace PI;

PushProxy::PushProxy()
{
    m_stopThread = false ;
    m_maxPushThreads = 5;
} 

int PushProxy::open( void *args )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    this->activate( THR_BOUND );
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return 0;
}

int PushProxy::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

	//__asm int 3;
	int sleepSecs = 30;
	int pushInstallSvcJobStatusSeqNum = 1;
	boost::posix_time::ptime currentTime = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::ptime lastPIJobStatusUploadTime = currentTime;
	std::string pushInstallSvcJobStatusTelemetryFolderPath = "C:\\ProgramData\\ASR\\home\\svsystems\\PushInstallJobs\\";
	std::string pushInstallSvcJobStatusLocalFilePath = 
		CsPushConfig::Instance()->tmpFolder() + remoteApiLib::pathSeperator() +
		"PIJobStatus.log";
	std::string pushInstallSvcJobStatusTelemetryFilePath = 
		pushInstallSvcJobStatusTelemetryFolderPath +
		"PIJobStatus_" + boost::posix_time::to_iso_string(currentTime) + ".log";

	std::string pushInstallSvcJobStatusLocalMetadataFilePath = 
		CsPushConfig::Instance()->tmpFolder() + remoteApiLib::pathSeperator() + "metadata.json";
	std::string pushInstallSvcJobStatusTelemetryMetadataFilePath = 
		pushInstallSvcJobStatusTelemetryFolderPath + remoteApiLib::pathSeperator() + "metadata.json";

    while(!m_stopThread) 
    {
        try
        {
			currentTime = boost::posix_time::microsec_clock::local_time();
			if (currentTime - lastPIJobStatusUploadTime > boost::posix_time::minutes(30))
			{
				try
				{
					boost::property_tree::ptree metadataPT;
					metadataPT.put("PushServerName", CsPushConfig::Instance()->PushServerName());
					metadataPT.put("PushServerIP", CsPushConfig::Instance()->PushServerIp());
					metadataPT.put("PushServerHostID", CsPushConfig::Instance()->hostid());
					boost::property_tree::write_json(pushInstallSvcJobStatusLocalMetadataFilePath, metadataPT);

					if (!SVMakeSureDirectoryPathExists(pushInstallSvcJobStatusTelemetryFolderPath.c_str()).failed())
					{
						boost::filesystem::rename(
							pushInstallSvcJobStatusLocalFilePath.c_str(),
							pushInstallSvcJobStatusTelemetryFilePath.c_str());

						boost::filesystem::rename(
							pushInstallSvcJobStatusLocalMetadataFilePath.c_str(),
							pushInstallSvcJobStatusTelemetryMetadataFilePath.c_str());

						lastPIJobStatusUploadTime = currentTime;
						pushInstallSvcJobStatusTelemetryFilePath =
							pushInstallSvcJobStatusTelemetryFolderPath +
							"PIJobStatus_" + boost::posix_time::to_iso_string(currentTime) + ".log";
					}
					else
					{
						DebugPrintf(
							SV_LOG_ERROR,
							"Could not create the directory = %s\n",
							pushInstallSvcJobStatusTelemetryFolderPath.c_str());
					}
				}
				catch (std::exception & e)
				{
					DebugPrintf(SV_LOG_WARNING, "Renaming PI job status file to upload to telemetry failed with exception %s.\n Will try to upload again automatically in the next time PI service checks for jobs.\n", e.what());
				}
			}

			sleepSecs = CsPushConfig::Instance()->jobFetchIntervalInSecs();

			CsCommunicationImpl::Instance()->RegisterPushInstaller();

			PushInstallationSettings currentSettings;
			
			boost::property_tree::ptree pt;
			currentTime = boost::posix_time::microsec_clock::local_time();
			pt.put("TIMESTAMP", boost::posix_time::to_iso_extended_string(currentTime));
			pt.put("PushInstallSvcJobStatusSeqNum", pushInstallSvcJobStatusSeqNum);
			pt.put("PushInstallerHostId", CsPushConfig::Instance()->hostid());
			pt.put("ProcessId", ACE_OS::getpid());

			currentSettings = CsCommunicationImpl::Instance()->GetSettings();
			pushInstallSvcJobStatusSeqNum++;

			try
			{
				if (!SVMakeSureDirectoryPathExists(CsPushConfig::Instance()->tmpFolder().c_str()).failed())
				{
					if (currentSettings.piRequests.size() == 0)
					{
						pt.put("Jobdetails", "No jobs received from CS.");
					}
					else 
					{
						std::ostringstream ss(std::ios_base::app | std::ios_base::out);
						std::list<PushRequests>::iterator piRequestsBegIter = currentSettings.piRequests.begin();
						std::list<PushRequests>::iterator piRequestsEndIter = currentSettings.piRequests.end();
						while (piRequestsBegIter != piRequestsEndIter)
						{
							std::list<PushRequestOptions>::iterator pushRequestOptionsBegIter = piRequestsBegIter->prOptions.begin();
							std::list<PushRequestOptions>::iterator pushReuestOptionsEndIter = piRequestsBegIter->prOptions.end();
							while (pushRequestOptionsBegIter != pushReuestOptionsEndIter)
							{
								boost::property_tree::ptree jobsPT;
								PushRequestOptions pushRequestOptions = (*pushRequestOptionsBegIter);
								jobsPT.put("IPAddress", pushRequestOptions.ip.c_str());
								jobsPT.put("VmType", pushRequestOptions.vmType.c_str());
								jobsPT.put("vCenterIP", pushRequestOptions.vCenterIP.c_str());
								jobsPT.put("vmName", pushRequestOptions.vmName.c_str());
								jobsPT.put("ActivityId", pushRequestOptions.ActivityId.c_str());
								jobsPT.put("ClientRequestId", pushRequestOptions.ClientRequestId.c_str());
								jobsPT.put("ServiceActivityId", pushRequestOptions.ServiceActivityId.c_str());

								boost::property_tree::write_json(ss, jobsPT);
								pushRequestOptionsBegIter++;
							}

							piRequestsBegIter++;
						}
						pt.put("Jobdetails", ss.str());
					}

					std::ofstream fs(pushInstallSvcJobStatusLocalFilePath, std::ios_base::app | std::ios_base::out);
					boost::property_tree::write_json(fs, pt);
					fs.flush();
					fs.close();
				}
			}
			catch (std::exception & e)
			{
				std::stringstream ss;
				ss << "Getting jobs from CS failed with exception %s.\n" << e.what();
				pt.put("Jobdetails", ss.str());
				if (!SVMakeSureDirectoryPathExists(CsPushConfig::Instance()->tmpFolder().c_str()).failed())
				{
					std::ofstream fs(pushInstallSvcJobStatusLocalFilePath, std::ios_base::app | std::ios_base::out);
					boost::property_tree::write_json(fs, pt);
					fs.flush();
					fs.close();
				}
			}

            if( currentSettings.ispushproxy )
            {
                if ( !(m_settings == currentSettings) || m_activeJobsMap.size()== 0 )
                {
                    m_settings = currentSettings ;
                    m_maxPushThreads = currentSettings.m_maxPushInstallThreads;
                    dumpSettings();
                    createInstallationJobThreads();
                }
            }
            startInstallationJobThreads();
            checkAndUpdateInstallationJobStatus();
            removeCompletedFailedJobs() ;
            DebugPrintf(SV_LOG_DEBUG, "Going to sleep...\n") ;	
            if(this->QuitRequested(sleepSecs))
            {
                break;
            }
        }
		catch (std::exception & e)
        {
            DebugPrintf(SV_LOG_ERROR, "CS api getInstallHosts failed with exception %s\n", e.what()) ;
            if(m_stopThread == false)
            {
				if (this->QuitRequested(sleepSecs))
                {
                    break;
                }
            }
        }
		catch (...)
		{
			DebugPrintf(SV_LOG_ERROR, "CS api getInstallHosts failed with unknown exception\n");
			if (m_stopThread == false)
			{
				if (this->QuitRequested(sleepSecs))
				{
					break;
				}
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return 0 ;
}

void PushProxy::createInstallationJobThreads()
{
	//DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    PushInstallationSettings installSettings = m_settings;
    std::list<PushRequests>::iterator piRequestsBegIter = installSettings.piRequests.begin();
    std::list<PushRequests>::iterator piRequestsEndIter = installSettings.piRequests.end();
    while(piRequestsBegIter != piRequestsEndIter)
    {
        std::list<PushRequestOptions>::iterator pushRequestOptionsBegIter = piRequestsBegIter->prOptions.begin();
        std::list<PushRequestOptions>::iterator pushReuestOptionsEndIter = piRequestsBegIter->prOptions.end();
        while(pushRequestOptionsBegIter != pushReuestOptionsEndIter)
        {
            if(pushRequestOptionsBegIter->state == INSTALL_JOB_PENDING || pushRequestOptionsBegIter->state == UNINSTALL_JOB_PENDING)
            {
                if(m_activeJobsMap.find(pushRequestOptionsBegIter->ip) == m_activeJobsMap.end() && 
                    m_pushInstallerThreadPtrMap.find(pushRequestOptionsBegIter->ip) == m_pushInstallerThreadPtrMap.end())
                {
					VM_TYPE vmType = PHYSICAL;
					if (pushRequestOptionsBegIter->vmType == "vCenter") {
						vmType = VMWARE;
					}

					PushRequestOptions pushRequestOptions = (*pushRequestOptionsBegIter);
					DebugPrintf(SV_LOG_DEBUG, "\t IpAddress:%s \n", pushRequestOptions.ip.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t JobID:%s \n", pushRequestOptions.jobId.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t Installation Path:%s \n", pushRequestOptions.installation_path.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t Reboot Required:%d \n", pushRequestOptions.reboot_required);
					DebugPrintf(SV_LOG_DEBUG, "\t State:%d \n", pushRequestOptions.state);
					DebugPrintf(SV_LOG_DEBUG, "\t vmType:%s \n", pushRequestOptions.vmType.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t vCenterIP:%s \n", pushRequestOptions.vCenterIP.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t vmName:%s \n", pushRequestOptions.vmName.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t vmAccountId:%s \n",pushRequestOptions.vmAccountId.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t vcenterAccountId:%s \n", pushRequestOptions.vcenterAccountId.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t ActivityId:%s \n", pushRequestOptions.ActivityId.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t ClientRequestId:%s \n", pushRequestOptions.ClientRequestId.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\t ServiceActivityId:%s \n", pushRequestOptions.ServiceActivityId.c_str());

					boost::uuids::uuid uuid = boost::uuids::random_generator()();
					std::string pushInstallJobId = boost::lexical_cast<std::string>(uuid);

					PushJobDefinition jobObj(pushRequestOptionsBegIter->jobId,
						piRequestsBegIter->os_type,
						pushRequestOptionsBegIter->ip,
						vmType,
						pushRequestOptionsBegIter->vCenterIP,
						pushRequestOptionsBegIter->vcenterAccountId,
						pushRequestOptionsBegIter->vCenterUserName,
						pushRequestOptionsBegIter->vCenterPassword,
						pushRequestOptionsBegIter->vmName,
						pushRequestOptionsBegIter->vmAccountId,
						piRequestsBegIter->domain,
						piRequestsBegIter->username,
						piRequestsBegIter->password,
						pushRequestOptionsBegIter->ClientRequestId,
						pushRequestOptionsBegIter->ActivityId,
						pushRequestOptionsBegIter->ServiceActivityId,
						pushRequestOptionsBegIter->installation_path,
						pushRequestOptionsBegIter->reboot_required,
						PUSH_JOB_STATUS(pushRequestOptionsBegIter->state),
						InstallationMode::WMISSH,
						(PushInstallCommunicationBasePtr)(CsCommunicationImpl::Instance()),
						(PushConfigPtr)(CsPushConfig::Instance()),
						"",
						"",
						pushInstallJobId + "\\",
						pushInstallJobId + "_",
						std::string(),
						std::string(),
						std::string());

                    ActiveJobInfo activeJob;
                    activeJob.m_ipAddress = pushRequestOptionsBegIter->ip;
                    activeJob.m_jobStatus = JOB_NOT_STARTED;
                    activeJob.m_jobId = pushRequestOptionsBegIter->jobId;
                    m_activeJobsMap.insert(std::make_pair(pushRequestOptionsBegIter->ip, activeJob));
                    
                    PushInstallerThreadPtr jobThreadPtr;
                    jobThreadPtr.reset(new PushInstallerThread(jobObj, &m_ThreadManager));
                    m_pushInstallerThreadPtrMap.insert(make_pair(pushRequestOptionsBegIter->ip, jobThreadPtr));
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "The installation process for the IP: %s is alraedy in inserted in m_pushInstallerThreadPtrMap. JOB ID: %s \n", pushRequestOptionsBegIter->ip.c_str(), pushRequestOptionsBegIter->jobId.c_str());
                }
            }
            else
            {
                DebugPrintf( SV_LOG_DEBUG, "Received Non Pending state Job. IP %s, Status: %d n", pushRequestOptionsBegIter->ip.c_str(), pushRequestOptionsBegIter->state);
            }
            pushRequestOptionsBegIter++;
        }
        piRequestsBegIter++;
    }
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void PushProxy::startInstallationJobThreads()
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::map<std::string, PushInstallerThreadPtr>::iterator pushInstallerThreadPtrMapTempIter;
    std::map<std::string, PushInstallerThreadPtr>::iterator pushInstallerThreadPtrMapEndIter;

    std::map<std::string, ActiveJobInfo>::iterator activeJobMapBegIter = m_activeJobsMap.begin();
    std::map<std::string, ActiveJobInfo>::iterator activeJobMapEndIter = m_activeJobsMap.end();
    int i = 1;
    while(activeJobMapBegIter != activeJobMapEndIter)
    {
        DebugPrintf(SV_LOG_DEBUG, "Mmaximum no of Threads Configured: %d \n", m_maxPushThreads);
        if(m_maxPushThreads == -1 || i <= m_maxPushThreads)
        {
            if(activeJobMapBegIter->second.m_jobStatus == JOB_NOT_STARTED)
            {
                pushInstallerThreadPtrMapTempIter = pushInstallerThreadPtrMapEndIter = m_pushInstallerThreadPtrMap.end();
                pushInstallerThreadPtrMapTempIter = m_pushInstallerThreadPtrMap.find(activeJobMapBegIter->first);
                if(pushInstallerThreadPtrMapTempIter != pushInstallerThreadPtrMapEndIter)
                {
                    DebugPrintf(SV_LOG_DEBUG, "Starting the Push Install Process thread for IP: %s \n", activeJobMapBegIter->first.c_str());
                    if(pushInstallerThreadPtrMapTempIter->second.get()->Start() != SVS_OK)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Failed to Start the Push Install Process thread for IP: %s \n", activeJobMapBegIter->first.c_str());           
                        activeJobMapBegIter->second.m_jobStatus = JOB_FAILED;
                        activeJobMapBegIter->second.m_endTime = time(NULL);
                        std::stringstream updateStream;
                        updateStream << "Failed to Start the Push Install Process thread for" << " IPAddress: "<< activeJobMapBegIter->first;
						CsCommunicationImpl::Instance()->UpdateAgentInstallationStatus(activeJobMapBegIter->second.m_jobId, INSTALL_JOB_FAILED, updateStream.str(), "UNKNOWN-HOST-ID", updateStream.str());
                    }
                    else
                    {
                        activeJobMapBegIter->second.m_startTime = time(NULL);
                        activeJobMapBegIter->second.m_jobStatus = JOB_STARTED;
                        ACE_thread_t threadId = pushInstallerThreadPtrMapTempIter->second.get()->GetThreadId();
                        DebugPrintf(SV_LOG_DEBUG, "Push Install Process thread for JOB ID: %s, IP: %s is started successfully. ThreadID: (%#x). StartTime: " ULLSPEC "\n", activeJobMapBegIter->second.m_jobId.c_str(), activeJobMapBegIter->first.c_str(), threadId, activeJobMapBegIter->second.m_startTime);                
                        i++;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "Did not find the IP: %s in m_pushInstallerThreadPtrMap to start the thread. \n", activeJobMapBegIter->first.c_str());
                    activeJobMapBegIter->second.m_jobStatus = JOB_FAILED;
                }
            }
        }
        activeJobMapBegIter++;
    }
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void PushProxy::checkAndUpdateInstallationJobStatus()
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    // 1) check each thread is exited ot not. if any of existed set the Job status as COMPLETED/FAILED.
    SV_ULONGLONG currentTime = 0;
    std::map<std::string, PushInstallerThreadPtr>::iterator PushInstallerThreadPtrTempIter;
    std::map<std::string, PushInstallerThreadPtr>::iterator PushInstallerThreadPtrEndIter;
    std::map<std::string, ActiveJobInfo>::iterator activeJobBegIter = m_activeJobsMap.begin(); 
    std::map<std::string, ActiveJobInfo>::iterator activeJobEndIter = m_activeJobsMap.end();
    while(activeJobBegIter != activeJobEndIter)
    {
        if(activeJobBegIter->second.m_jobStatus == JOB_STARTED)
        {
            PushInstallerThreadPtrTempIter = PushInstallerThreadPtrEndIter = m_pushInstallerThreadPtrMap.end();
            PushInstallerThreadPtrTempIter = m_pushInstallerThreadPtrMap.find(activeJobBegIter->first);
            if(PushInstallerThreadPtrTempIter != PushInstallerThreadPtrEndIter)
            {
                if(PushInstallerThreadPtrTempIter->second.get()->IsActive() == false) // TO. DO 
                {
                    DebugPrintf(SV_LOG_DEBUG, "Setting the JOB as completed status, As this thread is exited already: IP Address: %s \n", activeJobBegIter->first.c_str());
                    activeJobBegIter->second.m_jobStatus = JOB_COMPLETED;
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "IP Address: %s is still active \n", activeJobBegIter->first.c_str());
                    do
                    {
                        // get the JOB STATUS and if it is delete pending then cancel the thread and set it as completed.
                        PUSH_JOB_STATUS cxJobStatus;
                        if(getIPStateFromSettings(activeJobBegIter->first, cxJobStatus))
                        {
                            if(cxJobStatus == DELETE_JOB_PENDING)
                            {
                                DebugPrintf(SV_LOG_DEBUG, "The Job for IP Address: %s is set as delete pending state at CX. so stopping this thread. \n", activeJobBegIter->first.c_str());
								CsCommunicationImpl::Instance()->UpdateAgentInstallationStatus(activeJobBegIter->second.m_jobId, DELETE_JOB_COMPLETED, "Deleted", "UNKNOWN-HOST-ID", "Deleted");
                                m_ThreadManager.cancel(PushInstallerThreadPtrTempIter->second.get()->GetThreadId());
                                activeJobBegIter->second.m_jobStatus = JOB_COMPLETED;
                            }
                        }
                    }while(false);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "Did not find the IP: %s in m_pushInstallerThreadPtrMap to update m_activeJobsMap. \n", activeJobBegIter->first.c_str());
                activeJobBegIter->second.m_jobStatus = JOB_COMPLETED;
            }
        }
        activeJobBegIter++;
    }
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n" , __FUNCTION__);
}

bool PushProxy::getIPStateFromSettings(const std::string& ipAddress, enum PUSH_JOB_STATUS& cxJobStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
    PushInstallationSettings installSettings = m_settings;
    std::list<PushRequests>::iterator piRequestsBegIter = installSettings.piRequests.begin();
    std::list<PushRequests>::iterator piRequestsEndIter = installSettings.piRequests.end();
    while(piRequestsBegIter != piRequestsEndIter)
    {
        std::list<PushRequestOptions>::iterator pushRequestOptionsBegIter = piRequestsBegIter->prOptions.begin();
        std::list<PushRequestOptions>::iterator pushReuestOptionsEndIter = piRequestsBegIter->prOptions.end();
        while(pushRequestOptionsBegIter != pushReuestOptionsEndIter)
        {
            if(strcmpi(pushRequestOptionsBegIter->ip.c_str(), ipAddress.c_str()) == 0)
            {
                bRet = true;
                cxJobStatus = (PUSH_JOB_STATUS)pushRequestOptionsBegIter->state;
                break;
            }
            pushRequestOptionsBegIter++;
        }
        piRequestsBegIter++;
    }
    switch(cxJobStatus)
    {
        case INSTALL_JOB_PENDING:
            DebugPrintf(SV_LOG_DEBUG, "INSTALL_JOB_PENDING.\n");
            break;
        case INSTALL_JOB_PROGRESS:
            DebugPrintf(SV_LOG_DEBUG, "INSTALL_JOB_PROGRESS.\n");
            break;
        case INSTALL_JOB_COMPLETED:
            DebugPrintf(SV_LOG_DEBUG, "INSTALL_JOB_COMPLETED.\n");
            break;
        case INSTALL_JOB_FAILED:
            DebugPrintf(SV_LOG_DEBUG, "INSTALL_JOB_FAILED.\n");
            break;
        case INSTALL_UPGRADE_BUILD_NOT_AVAILABLE:
            DebugPrintf(SV_LOG_DEBUG, "INSTALL_UPGRADE_BUILD_NOT_AVAILABLE.\n");
            break;
        case UNINSTALL_JOB_PENDING:
            DebugPrintf(SV_LOG_DEBUG, "UNINSTALL_JOB_PENDING.\n");
            break;
        case UNINSTALL_JOB_PROGRESS:
            DebugPrintf(SV_LOG_DEBUG, "UNINSTALL_JOB_PROGRESS.\n");
            break;
        case UNINSTALL_JOB_COMPLETED:
            DebugPrintf(SV_LOG_DEBUG, "UNINSTALL_JOB_COMPLETED.\n");
            break;
        case UNINSTALL_JOB_FAILED:
            DebugPrintf(SV_LOG_DEBUG, "UNINSTALL_JOB_FAILED.\n");
            break;
        case DELETE_JOB_PENDING:
            DebugPrintf(SV_LOG_DEBUG, "DELETE_JOB_PENDING.\n");
            break;
        case DELETE_JOB_COMPLETED:
            DebugPrintf(SV_LOG_DEBUG, "DELETE_JOB_COMPLETED.\n");
            break;
    };
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

void PushProxy::removeCompletedFailedJobs()
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::map<std::string, ActiveJobInfo>::iterator activeJobBegIter = m_activeJobsMap.begin(); 
    std::map<std::string, ActiveJobInfo>::iterator activeJobEndIter = m_activeJobsMap.end();
    std::string key = "";
    while(activeJobBegIter != activeJobEndIter)
    {
        key = "";
        if(activeJobBegIter->second.m_jobStatus == JOB_FAILED || 
            activeJobBegIter->second.m_jobStatus == JOB_COMPLETED)
        {
            key = activeJobBegIter->first;
            DebugPrintf(SV_LOG_DEBUG, "Removing the JOBS of IP Address: %s \n", key.c_str());
            activeJobBegIter++;
            m_activeJobsMap.erase(key);
            m_pushInstallerThreadPtrMap.erase(key);
            continue;
        }
        activeJobBegIter++;
    }
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void PushProxy::onServiceDeletion()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	CsCommunicationImpl::Instance()->UnregisterPushInstaller();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


void PushProxy::dumpSettings()
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    PushInstallationSettings installSettings = m_settings;
    DebugPrintf( SV_LOG_DEBUG, "#############################################\n" );
    DebugPrintf( SV_LOG_DEBUG, "PushProxy: %d \n", installSettings.ispushproxy );
    DebugPrintf( SV_LOG_DEBUG, "********** \n" );
    DebugPrintf( SV_LOG_DEBUG, "Max Thread Count: %d \n", installSettings.m_maxPushInstallThreads );
    std::list<PushRequests>::iterator pushRequestsIter = installSettings.piRequests.begin() ;
    while( pushRequestsIter != installSettings.piRequests.end() )
    {
        PushRequests pushRequests = (*pushRequestsIter) ;
        DebugPrintf( SV_LOG_DEBUG, "OS Value:%d \n", pushRequests.os_type );
        DebugPrintf( SV_LOG_DEBUG, "*************** \n" );
        std::list<PushRequestOptions>::iterator pushRequestOptionsIter = pushRequests.prOptions.begin() ;        
        while( pushRequestOptionsIter != pushRequests.prOptions.end() )
        {
            PushRequestOptions pushRequestOptions = (*pushRequestOptionsIter) ;
            DebugPrintf( SV_LOG_DEBUG, "\t IpAddress:%s \n", pushRequestOptions.ip.c_str() );
            DebugPrintf( SV_LOG_DEBUG, "\t JobID:%s \n", pushRequestOptions.jobId.c_str() );
            DebugPrintf( SV_LOG_DEBUG, "\t Installation Path:%s \n", pushRequestOptions.installation_path.c_str() );
            DebugPrintf( SV_LOG_DEBUG, "\t Reboot Required:%d \n", pushRequestOptions.reboot_required);
            DebugPrintf( SV_LOG_DEBUG, "\t State:%d \n", pushRequestOptions.state );
			DebugPrintf(SV_LOG_DEBUG, "\t vmType:%s \n", pushRequestOptions.vmType.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t vCenterIP:%s \n", pushRequestOptions.vCenterIP.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t vmName:%s \n", pushRequestOptions.vmName.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t vmAccountId:%s \n", pushRequestOptions.vmAccountId.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t vcenterAccountId:%s \n", pushRequestOptions.vcenterAccountId.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t ActivityId:%s \n", pushRequestOptions.ActivityId.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t ClientRequestId:%s \n", pushRequestOptions.ClientRequestId.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t ServiceActivityId:%s \n", pushRequestOptions.ServiceActivityId.c_str());
            pushRequestOptionsIter++ ;
        }
        pushRequestsIter++ ;  
    }
    DebugPrintf( SV_LOG_DEBUG, "#############################################\n" );
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

int PushProxy::close( u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n" , __FUNCTION__);
    return 0;
}

PushProxy::~PushProxy()
{
    m_ThreadManager.wait();
}

