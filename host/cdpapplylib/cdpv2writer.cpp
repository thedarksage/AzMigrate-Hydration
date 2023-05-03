//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv2writer.cpp
//
// Description: Contains CDPV2Writer class definition. And is used
// for applying differentials to retention logs and target volume.
//

#include <sstream>


#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>
#include <ace/Mem_Map.h>
#include <ace/OS.h>
#include <ace/File_Lock.h>
#include <ace/Guard_T.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>

#include "sharedalignedbuffer.h"

#include "localconfigurator.h"
#include "configurevxagent.h"
#include "inmageex.h"

#include "decompress.h"
#include "inmemorydifffile.h"
#include "differentialfile.h"
#include "cdpplatform.h"
#include "cdpglobals.h"
#include "cdputil.h"
#include "configwrapper.h"
#include "VacpUtil.h"
#include "cdpv3transaction.h"

#include "cdpv2writer.h"
#include "differentialsync.h"
#include "theconfigurator.h"
#include "cdplock.h"

#include "cdpcoalesce.h"
#include "portablehelpersmajor.h"
#include "snapshotrequestprocessor.h"

#include "inmsafecapis.h"
#include "inmalertdefs.h"

#include "setpermissions.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif /* SV_UNIX */

#ifdef SV_WINDOWS
#include <winioctl.h>
#endif

#include "logger.h"

#include "errorexception.h"
//#include "errorexceptionmajor.h"
#include "azureresyncrequiredexception.h"
#include "azurebadfileexception.h"
#include "azuremissingfileexception.h"
#include "azureinvalidopexception.h"
#include "azurecanceloperationexception.h"
#include "replicationpairoperations.h"
#include "azureclientresyncrequiredoperationexception.h"

#include "volumereporter.h"
#include "volumereporterimp.h"

using namespace std;

const int Tso_File_Size = 196;
const std::string Tso_File_SubString = "_tso_";

const int CDP_DISABLED_VALUE = 0;
const int CDP_ENABLED_V1 = 1;
const int CDP_ENABLED_V3 = 3;
const int CDP_ENABLED_UNSUPPORTED = 4;

ACE_Recursive_Thread_Mutex perf_lock;
ACE_Recursive_Thread_Mutex cksum_lock;
ACE_Recursive_Thread_Mutex mdfile_lock;



// ===================================================
//  public interfaces
//   
//  1) constructor
//    CDPV2Writer::CDPV2Writer
//
//  2) initialization routine
//     init()
//
//  3) Apply Differential
//     Applychanges
//
// ====================================================

/*
 * FUNCTION NAME :  CDPV2Writer::CDPV2Writer(const CDP_SETTINGS & settings)
 *
 * DESCRIPTION : CDPV2Writer constructor 
 *
 *
 * INPUT PARAMETERS : CDP_SETTINGS& settings, which contains info regarding
 *                    the retention policy.
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : none
 *
 */
CDPV2Writer::CDPV2Writer(const std::string & volumename,
                         const SV_ULONGLONG & source_capacity,
                         const CDP_SETTINGS & settings,
                         bool diffsync,                        
						 cdp_resync_txn * resync_txn_mgr,
						 AzureAgentInterface::AzureAgent::ptr resyncAzureAgent,
                         const VolumeSummaries_t *pVolumeSummariesCache,
                         Profiler pSyncApply,
                         const bool hideBeforeApplyingSyncData)
    : iocount_cv(iocount_mutex),
	m_resync_txn_mgr(resync_txn_mgr),
	m_AzureAgent(resyncAzureAgent),
    m_pVolumeSummariesCache(pVolumeSummariesCache),
    m_pSyncApply(pSyncApply),
    m_hideBeforeApplyingSyncData(hideBeforeApplyingSyncData)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_volumename = volumename;
    m_srccapacity = source_capacity;
    m_settings =settings;
    m_isInDiffSync= diffsync;
    m_resyncRequired =false;
    m_init =false;
    m_dbrecord_fetched =false;
    m_isvirtualvolume = false;
    m_volpackdriver_available = false;
    m_vsnapdriver_available = false;

#ifndef DP_SYNC_RCM // Note: Skipping for DataProtectionSyncRcm as useCfs is always false for forward protection
    cfs::getCfsPsData(TheConfigurator->getInitialSettings(), volumename, m_cfsPsData);
#endif

    //these are used in case of multiple sparse file only	
    m_ismultisparsefile = false;
    m_multisparsefilename = "";
    m_maxsparsefilepartsize = 0;
    m_max_rw_size = 0;
    m_sector_size = 512; //Default
    m_cache_cdpfile_handles = false;
    m_cdpdata_handles_hr = 0;
    m_sparse_enabled = true;
    m_punch_hole_supported = true;
    m_UseBlockingIoForCurrentInvocation = false;
	
    m_cache_volumehandle = false;
    m_volhandle = ACE_INVALID_HANDLE;
    m_vsnapctrldev = ACE_INVALID_HANDLE;

	// azure specific members initialization


	m_targetDataPlane = VOLUME_SETTINGS::UNSUPPORTED_DATA_PLANE;
	m_firstInvocation = true;
	m_isSessionHavingRecoverablePoints = false;
	m_isCurrentFileContainsRecoverablePoints = false;
	m_minUploadSize = 134217728; // 128 mb, this is re-fetched from config file in init routine
	m_minTimeGapBetweenUploads = 180; // 180 secs, this  is re-fetched from config file in init routine
	m_TimeGapBetweenFileArrivalCheck = 15; // 15 secs, this is re-fetched from config file in init routine 
	m_uploadTimestamp = ACE_OS::gettimeofday();
	m_uploadSize = 0;
	m_isLastFileinPSCache = false;
	m_bLogUploadedFilesToAzure = false;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

CDPV2Writer::~CDPV2Writer()
{


    close_volume_handle();

    if(ACE_INVALID_HANDLE != m_vsnapctrldev)
    {
        CloseVVControlDevice(m_vsnapctrldev);
        m_vsnapctrldev = ACE_INVALID_HANDLE;
    }

    if(!m_cdpdata_asyncwrite_handles.empty())
    {
        close_cdpdata_asyncwrite_handles();
    }

    if(!m_cdpdata_syncwrite_handles.empty())
    {
        close_cdpdata_syncwrite_handles();
    }

    if(!m_asyncread_handles.empty())
    {
        close_asyncread_handles();
    }

    if(!m_asyncwrite_handles.empty())
    {
        close_asyncwrite_handles();
    }

    if(!m_syncread_handles.empty())
    {
        close_syncread_handles();
    }

    if(!m_syncwrite_handles.empty())
    {
        close_syncwrite_handles();
    }
}

/*
 * FUNCTION NAME :  CDPV2Writer::init()
 *
 * DESCRIPTION : Creates the retention DB and required tables if
 *               they don't exist.
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::init()
{
    bool rv = true;

    //__asm int 3;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            if(m_init)
            {
                rv = true;
                break;
            }

            if(!GetConfigurator(&m_Configurator))
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get configurator instance\n" );
                rv = false;
                break;
            }

			m_baseline = m_Configurator->getSourceOSVal(m_volumename);
			m_targetDataPlane = m_Configurator->getTargetDataPlane(m_volumename);
			if (m_targetDataPlane == VOLUME_SETTINGS::UNSUPPORTED_DATA_PLANE) {
				DebugPrintf(SV_LOG_ERROR, "Received invalid target data plane for replication; repliation for target device %s cannot proceed.\n", m_volumename.c_str());
				rv = false;
				break;
			}


            LocalConfigurator localconfigurator;
            m_minDiskFreeSpaceForUncompression = localconfigurator.getMinDiskFreeSpaceForUncompression();
			
			if (isAzureTarget()){
				m_resync_chunk_size = localconfigurator.getFastSyncMaxChunkSizeForE2A();
			}
			else{
				m_resync_chunk_size = localconfigurator.getFastSyncMaxChunkSize();
			}

            m_noOfUncompressRetries = localconfigurator.getUncompressRetries();
            m_uncompressRetryInterval = localconfigurator.getUncompressRetryInterval();
            m_configMemoryForDrtds_Diffsync = localconfigurator.getMaxMemoryForDiffSyncFile();
            m_configMemoryForDrtds_Resync = localconfigurator.getMaxMemoryForResyncFile();
            if(isInDiffSync())
            {
                m_maxMemoryForDrtds = m_configMemoryForDrtds_Diffsync;
                m_ignoreCorruptedDiffs = localconfigurator.shouldIgnoreCorruptedDiffs();
            }
            else
            {
                m_maxMemoryForDrtds = m_configMemoryForDrtds_Resync;
                m_ignoreCorruptedDiffs = false;
            }

            m_allowOutOfOrderTS = localconfigurator.AllowOutOfOrderTS();
            m_allowOutOfOrderSeq = localconfigurator.AllowOutOfOrderSeq();
            m_setResyncRequiredOnOOD = (m_allowOutOfOrderTS || m_allowOutOfOrderSeq);
            m_ignoreOOD = localconfigurator.IgnoreOutOfOrder();
            m_preallocate = localconfigurator.isCdpDataFilePreAllocationEnabled();

            m_useUnBufferedIo = localconfigurator.useUnBufferedIo();
            m_cdpCompressed = localconfigurator.CDPCompressionEnabled();
            m_checkDI = localconfigurator.getDICheck();
            m_verifyDI = localconfigurator.getDIVerify();
            m_printPerf = localconfigurator.DPPrintPerfCounters();
            m_profile_source_iopattern = localconfigurator.DPProfileSourceIo();
            m_profile_volread = localconfigurator.DPProfileVolRead();
            m_profile_volwrite = localconfigurator.DPProfileVolWrite();
            m_profile_cdpwrite = localconfigurator.DPProfileCdpWrite();

            m_maxpendingios = localconfigurator.MaxAsyncIos();
            m_minpendingios = (m_maxpendingios -1);

            m_bmcaching = localconfigurator.DPBMCachingEnabled();
            m_bmcachesize = localconfigurator.DPBMCacheSize();
            m_bmunbuffered_io = localconfigurator.DPBMUnBufferedIo();
            m_bmblocksize = localconfigurator.DPBMBlockSize();
            m_bmblocksperentry = localconfigurator.DPBMBlocksPerEntry();
            m_bmcompression = localconfigurator.DPBMCompressionEnabled();
            m_bmmaxios = localconfigurator.DPBMMaxIos();
            m_bmmaxmem_forio = localconfigurator.DPBMMaxMemForIo();
            m_maxcdpv3FileSize = localconfigurator.getMaxCdpv3CowFileSize();
            m_reserved_cdpspace = localconfigurator.getSizeOfReservedRetentionSpace();

            m_max_rw_size = localconfigurator.getCdpMaxIOSize();
            m_sector_size = localconfigurator.getVxAlignmentSize();
            m_volpackdriver_available = localconfigurator.IsVolpackDriverAvailable();
            m_vsnapdriver_available = localconfigurator.IsVsnapDriverAvailable();
            m_cache_volumehandle = localconfigurator.DPCacheVolumeHandle();
            m_max_filehandles_tocache = localconfigurator.DPMaxRetentionFileHandlesToCache();
            m_cache_cdpfile_handles = (m_max_filehandles_tocache > 0);

			m_compressionChunkSize = localconfigurator.getCompressionChunkSize();
			m_compressionBufferSize = localconfigurator.getCompressionBufSize();
			m_cacheDirectoryPath = localconfigurator.getCacheDirectory();
			m_targetCheckSumDir = localconfigurator.getTargetChecksumsDir();
			m_installPath = localconfigurator.getInstallPath();
			m_minUploadSize = localconfigurator.getMinAzureUploadSize();
			m_minTimeGapBetweenUploads = localconfigurator.getMinTimeGapBetweenAzureUploads();
			m_TimeGapBetweenFileArrivalCheck = localconfigurator.getTimeGapBetweenFileArrivalCheck();
			m_uploadTimestamp = ACE_OS::gettimeofday();
			m_bLogUploadedFilesToAzure = (localconfigurator.getLogLevel() == SV_LOG_ALWAYS);

			m_cdp_max_redolog_size = localconfigurator.getCdpRedoLogMaxFileSize();
			m_cdp_flush_and_hold_writes_retry = localconfigurator.IsFlushAndHoldResumeRetryEnabled();
			m_cdp_flush_and_hold_resume_retry = localconfigurator.IsFlushAndHoldResumeRetryEnabled();


			m_unhidero_filepath = CDPUtil::getPendingActionsFilePath(m_volumename, ".unhideRO");
			m_unhiderw_filepath = CDPUtil::getPendingActionsFilePath(m_volumename, ".unhideRW");
			m_rollback_filepath = CDPUtil::getPendingActionsFilePath(m_volumename, ".rollback");

			m_transport_protocol = m_Configurator->getProtocol(m_volumename);
			m_secure_mode = m_Configurator->getSecureMode(m_volumename);
			m_inquasi_state = m_Configurator->isInQuasiState(m_volumename);

			m_useAsyncIOs = true;
			m_bmasyncio = true;
			m_useUnBufferedIo = true;
			m_bmunbuffered_io = true;
			m_cdpenabled = false;



			m_volguid = m_volumename;

			m_pair_state = m_Configurator->getPairState(m_volumename);
			if (isInFlushAndHoldPendingState())
			{
				//Get the FLUSH_AND_HOLD_REQUEST settings from CX.
				if (!getFlushAndHoldRequestSettingsFromCx())
				{
					rv = false;
					break;
				}

			}

			m_snapshotrequestprocessor_ptr.reset(new SnapShotRequestProcessor(m_Configurator));

			std::string appliedInfoPath = CDPUtil::get_last_fileapplied_info_location(m_volumename);
			m_appliedInfoDir = dirname_r(appliedInfoPath.c_str());
			if (SVMakeSureDirectoryPathExists(m_appliedInfoDir.c_str()).failed())
			{
				stringstream l_stdfatal;
				l_stdfatal << "Error detected in Function:" << FUNCTION_NAME
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "Error during Directory: " << m_appliedInfoDir << " creation.\n"
					<< "Error Message: " << Error::Msg() << "\n"
					<< "Action: Make Sure root directory exists and "
					<< "proper permissions are available.\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				rv = false;
				break;
			}
			if (!CDPUtil::get_last_fileapplied_information(m_volumename, m_lastFileApplied))
			{
				rv = false;
				break;
			}


			if (!GetDeviceProperties()){
				rv = false;
				break;
			}

			if (m_targetDataPlane == VOLUME_SETTINGS::INMAGE_DATA_PLANE) {

				m_dbname = m_settings.Catalogue();
				m_cdpdir = m_settings.CatalogueDir();
				m_cdpenabled = (m_settings.Status() == CDP_ENABLED);
				if (m_cdpenabled && isInInitialSync())
					m_cdpenabled = false;

				detect_cdp_version();

				DebugPrintf(SV_LOG_DEBUG, "cdp enabled: %d\n", m_cdpenabled);

				if (!CDPUtil::validate_cdp_settings(m_volumename, m_settings))
				{
					DebugPrintf(SV_LOG_ERROR, "invalid cdp settings for %s.\n", m_volumename.c_str());
					rv = false;
					break;
				}


				bool allow_rootvolume_retention = localconfigurator.allowRootVolumeForRetention();
				if (!allow_rootvolume_retention && m_cdpenabled)
				{
					std::string retentionRoot;
					if (!GetVolumeRootPath(m_cdpdir, retentionRoot))
					{
						DebugPrintf(SV_LOG_ERROR, "Unable to determine the root of %s\n", m_cdpdir.c_str());
						CDPUtil::QuitRequested(localconfigurator.DPDelayBeforeExitOnError());
						rv = false;
						break;
					}
					if (retentionRoot == "/")
					{
						stringstream l_stderr;
						l_stderr << "Retention path {" << m_cdpdir << "}  is mounted on /. Differentials  for "
							<< m_volumename
							<< " will not be written until the retention is mounted on non-root mount point.\n";
						DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
						CDPUtil::QuitRequested(localconfigurator.DPDelayBeforeExitOnError());
						rv = false;
						break;
					}
				}

            std::string sparsefile;
            //these are initialize in case of multiple sparse file only
            if(!m_volpackdriver_available)
            {
                m_isvirtualvolume = IsVolPackDevice(m_volumename.c_str(),sparsefile,m_ismultisparsefile);
                if(m_isvirtualvolume && m_ismultisparsefile)
                {
                    m_multisparsefilename = sparsefile;

                    std::string sparsepartfile = m_multisparsefilename;
                    sparsepartfile += SPARSE_PARTFILE_EXT ;
                    sparsepartfile += "0" ;

                    ACE_stat s;
                    if( sv_stat( getLongPathName(sparsepartfile.c_str()).c_str(), &s ) < 0 ) 
                    {
                        DebugPrintf(SV_LOG_ERROR, "backend file (%s) for %s is not available.Differentials cannot be applied\n",
                                    sparsepartfile.c_str(), m_volumename.c_str());
                        rv = false;
                        break;
                    }
                    m_maxsparsefilepartsize = s.st_size;
                    m_sparse_enabled = IsSparseFile(sparsepartfile);
                }
                else if(m_isvirtualvolume)
                {
                    m_sparse_enabled = IsSparseFile(sparsefile);
                }
                m_punch_hole_supported = m_sparse_enabled;
            }

            if(m_isvirtualvolume)
            {
                m_useAsyncIOs = localconfigurator.AsyncOpEnabled();
                m_bmasyncio = localconfigurator.DPBMAsynchIo();
				}
				else
            {
                m_useAsyncIOs = localconfigurator.AsyncOpEnabledForPhysicalVolumes();
                m_bmasyncio = localconfigurator.DPBMAsynchIoForPhysicalVolumes();
            }

            // solaris : always buffering i.e m_useUnBufferedIo=false
            // aix: always o_direct i.e m_useUnBufferedIo=true

#ifdef SV_SUN
            m_useUnBufferedIo=false;
            m_bmunbuffered_io=false;
#endif

#ifdef SV_AIX
            m_useUnBufferedIo=true;
            m_bmunbuffered_io=true;
#endif

            initialize_cdp_bitmap_v3();

            if (m_cdpenabled)
            {
                try
                {
                    securitylib::setPermissions(m_cdpdir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
                }
                catch(std::exception& e)
                {
                    stringstream l_stderr;
                    l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME
                             << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n";
                    CdpLogDirCreateFailAlert a(m_cdpdir, "Failed to access");
                    l_stderr << a.GetMessage() << ". " << e.what();
                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
                    CDPUtil::QuitRequested(localconfigurator.DPDelayBeforeExitOnError());
                    DebugPrintf(SV_LOG_ERROR, "%s\n", l_stderr.str().c_str());
                    rv = false;
                    break;
                }

                if(!CDPUtil::get_lowspace_value(m_cdpdir,m_cdpfreespace_trigger))
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed to get free space trigger for retention dir %s, target volume %s\n",m_cdpdir.c_str(),
                                m_volumename.c_str());
                    rv = false;
                    break;
                }

#ifdef SV_WINDOWS
                if(m_cdpCompressed)
                {
                    //do compress ioctl call to the dir handle
                    if(!EnableCompressonOnDirectory(m_cdpdir))
                    {
                        stringstream l_stderr;
                        l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                                   << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                                   << "Error during enabling compression on CDP Log Directory :" 
                                   << m_cdpdir << " .\n"
                                   << "Error Message: " << Error::Msg() << "\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    }
                }
                else
                {
                    if(!DisableCompressonOnDirectory(m_cdpdir))
                    {
                        stringstream l_stderr;
                        l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                                   << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                                   << "Error during disabling compression on CDP Log Directory :" 
                                   << m_cdpdir << " .\n"
                                   << "Error Message: " << Error::Msg() << "\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    }
                }
#endif

                CDPDatabase db(m_dbname);
                if(!db.initialize())
                {
                    rv = false;
                    break;
                }

                switch(get_cdp_version())
                {
                    case CDP_ENABLED_V1:
                        if(!CDPUtil::get_last_retentiontxn(m_cdpdir, m_lastCDPtxn))
                        {
                            rv = false;
                            break;
                        }
                        break;
                    case CDP_ENABLED_V3:
                        std::string txnfilename = cdpv3txnfile_t::getfilepath(m_cdpdir);
                        cdpv3txnfile_t txnfile(txnfilename);
                        if(!txnfile.read(m_txn, m_sector_size, m_useUnBufferedIo))
                        {
                            rv = false;
                            break;
                        }
                        break;
                }
            }
            if((!isInDiffSync()) && (get_cdp_version() == CDP_ENABLED_V1))
            {
                std::string none = "none";
                memset(&m_lastCDPtxn, 0, sizeof(m_lastCDPtxn.partialTransaction));
                inm_memcpy_s(m_lastCDPtxn.partialTransaction.diffFileApplied, sizeof(m_lastCDPtxn.partialTransaction.diffFileApplied), none.c_str(), none.length() + 1);

                if(!CDPUtil::update_last_retentiontxn(m_cdpdir, m_lastCDPtxn))
                {
                    rv = false;
                    break;
                }
            }


            FormatVolumeNameToGuid(m_volguid);

            if(!m_ismultisparsefile && m_cache_volumehandle)
            {
                if(!open_volume_handle())
                {
                    rv = false;
                    break;
                }
            }

            if(m_vsnapdriver_available)
            {
                if(0 != OpenInVirVolDriver(m_vsnapctrldev))
                {
                    DebugPrintf(SV_LOG_FATAL, "open vsnap control device failed.\n");
                    rv = false;
                    break;
                }
            }


            if(get_cdp_version() != CDP_DISABLED_VALUE)
            {
                m_cdpprune_filepath = m_cdpdir;
                m_cdpprune_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                m_cdpprune_filepath += "cdp_pruning";
            }

			}

			// If resync flag is set in local settings but CS settings does not have it,
			// this could be due to some bug, retry set resync required API on CS.
			if (!check_and_setresync_on_cs_ifrequired())
			{
				rv = false;
				break;
			}

        } while ( 0 );

        if(rv)
        {
            m_init = true;
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// ========================================================
//  replication mode routines
//  isInInitialSync
//  detect_cdp_version
// ========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::isInInitialSync()
 *
 * DESCRIPTION : figures out the current replication state
 *              
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if initial sync, else false
 *
 */
bool CDPV2Writer::isInInitialSync()
{
    bool rv = false;


    if(m_isInDiffSync)
        return false;
    ACE_stat db_stat ={0};
    // PR#10815: Long Path support
    if(sv_stat(getLongPathName(m_settings.Catalogue().c_str()).c_str(),&db_stat) != 0)
        return true;//db does not exist

    SV_ULONGLONG start_ts = 0;
    SV_ULONGLONG end_ts = 0;
    CDPDatabase db(m_settings.Catalogue());

    if(!db.getrecoveryrange(start_ts, end_ts))
    {
        DebugPrintf(SV_LOG_WARNING, "Failed reading the recoveryrange for db %s.\n",
                    m_settings.Catalogue().c_str());
        return false;
    }

    if((start_ts == 0)	|| (end_ts == 0))
        return true;//db is empty

    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::detect_cdp_version()
 *
 * DESCRIPTION : figures out the cdp state/version
 *              
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if initial sync, else false
 *
 */
void CDPV2Writer::detect_cdp_version()
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if(!m_cdpenabled)
    {
        m_cdpversion = CDP_DISABLED_VALUE;
        DebugPrintf(SV_LOG_DEBUG, "cdp version = %d\n", m_cdpversion);
        return;
    }

    if((m_dbname.size() > CDPV1DBNAME.size())&& (std::equal(m_dbname.begin() + m_dbname.size() - CDPV1DBNAME.size(), m_dbname.end(), CDPV1DBNAME.begin())))
    {
        m_cdpversion = CDP_ENABLED_V1;
        DebugPrintf(SV_LOG_DEBUG, "cdp version = %d\n", m_cdpversion);
	}
	else if ((m_dbname.size() > CDPV3DBNAME.size()) && (std::equal(m_dbname.begin() + m_dbname.size() - CDPV3DBNAME.size(), m_dbname.end(), CDPV3DBNAME.begin())))
    {
        m_cdpversion = CDP_ENABLED_V3;
        DebugPrintf(SV_LOG_DEBUG, "cdp version = %d\n", m_cdpversion);
	}
	else
    {
        DebugPrintf(SV_LOG_ERROR, "cdp catalogue path [%s] is incorrect.\n", m_dbname.c_str());
        m_cdpversion = CDP_ENABLED_UNSUPPORTED;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


//  ========================================================
//  initialize cdp bitmap if sparse retention
//  ========================================================


bool CDPV2Writer::initialize_cdp_bitmap_v3()
{
    if(get_cdp_version() == CDP_ENABLED_V3 )
    {
        //
        // bitmap used to keep track of the
        // timestamp in hrs for each block_aligned
        // i/o
        // used for figuring where the cow data
        // for the i/o should be written
        //

        m_cdpbitmap.setpath(m_cdpdir);

        if(m_bmasyncio)
            m_cdpbitmap.enable_asyncio();
        else
            m_cdpbitmap.disable_asyncio();

        if(m_bmcaching)
            m_cdpbitmap.enable_caching();
        else
            m_cdpbitmap.disable_caching();

        if(m_bmunbuffered_io)
            m_cdpbitmap.enable_directio();
        else
            m_cdpbitmap.disable_directio();

        if(m_bmcompression)
            m_cdpbitmap.enable_compression();
        else
            m_cdpbitmap.disable_compression();

        m_cdpbitmap.set_cache_size(m_bmcachesize);
        m_cdpbitmap.set_block_size(m_bmblocksize);
        m_cdpbitmap.set_blocks_per_entry(m_bmblocksperentry);
        m_cdpbitmap.set_maxios(m_bmmaxios);
        m_cdpbitmap.set_maxmem_forio(m_bmmaxmem_forio);

        if(!m_cdpbitmap.init())
            return false;
    }

    return true;
}


/*
 * FUNCTION NAME :  CDPV2Writer::Applychanges
 *
 * DESCRIPTION : Applies the diff file to the target volume. If cdp is enabled
 *               also writes the Copy on Write(COW) data from target volume to
 *               retention logs before applying the diff to volume. And finally
 *               send the updates to CX.
 *
 * INPUT PARAMETERS :
 *    filePath - absolute path to diff file in cache dir.
 *    volumename - Name of the target volume
 *    source - input buffer containing the differential
 *    sourceLength - length of input buffer
 *    offset - starting volume offset for resync chunk being applied
 *    bm - bitmap contaning information on already applied chunks
 *    diffSync - pointer to DifferentialSync instance
 *    volumeInfo - pointer to VolumeInfo object
 *                 used for figuring out recovery accuracy
 *    ctid - Continuation ID for split ios
 *
 * OUTPUT PARAMETERS :
 *     totalBytesApplied - bytes applied to target volume
 *     
 *
 * ALGORITHM :
 *    1.  Read differential file
 *    2.  If the data is compressed,uncompress
 *    3.  Parse the uncompressed data
 *    4.  Process any event based snapshots
 *    5.  Update sync state(if required)
 *    6.  findout accuracy mode
 *    7.  If retention is enabled update the retention db and the cdp data file
 *    8.  Write the differential data to target volume
 *    9.  Delete the differential file in cache directory
 *    10. If retention is enabled, delete the dummy file which is created while
 *        writing to retention & send updates about bookmarks, retention ranges
 *        etc. to CX 
 *
 * return value : true if Apply successfull and false otherwise
 *
 */
bool CDPV2Writer::Applychanges(const std::string & filePath, 
                               long long& totalBytesApplied,
                               char* source,
                               const SV_ULONG sourceLength,
                               SV_ULONGLONG* offset,
                               SharedBitMap* bm,
	                           int ctid,
	                           bool lastFileToProcess)
{
    bool rv = true;
    //__asm int 3
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            PerfTime applystart_time;
            getPerfTime(applystart_time);

            if(!init())
            {
                rv = false;
                break;
            }

            // performance variables
            m_diffsize = 0;
            m_diffread_time = 0;
            m_diffuncompress_time = 0;
            m_diffparse_time = 0;
            m_metadata_time = 0;
            m_cownonoverlap_computation_time = 0;
            m_volread_time = 0;
            m_cowwrite_time = 0;
            m_vsnapupdate_time = 0;
            m_dboperation_time = 0;
            m_volwrite_time = 0;
            m_volnonoverlap_computation_time = 0;
            m_bitmapread_time = 0;
            m_bitmapwrite_time = 0;

            // variables to be set per input file
            std::string fileToApply = filePath;
            m_isFileInMemory = (source != 0);
            m_isCompressedFile = isCompressedFile(fileToApply);
            m_recoveryaccuracy = 0;
            m_ctid = ctid;
            m_checksums.clear();
            m_pendingios = 0;
            m_error = false;
            m_memconsumed_for_io = 0;
            m_memtobefreed_on_io_completion = false;
            m_buffers_allocated_for_io.clear();
            m_cdp_data_filepaths.clear();
            m_cowlocations.clear();
            m_asyncwrite_handles.clear();
            m_syncwrite_handles.clear();
            m_asyncread_handles.clear();
            m_syncread_handles.clear();

            /*SRM:Start*/
            if(isInFlushAndHoldState())
            {
                CDPUtil::QuitRequested(180);
                return false; //TODO : Wait for state change or quit request.
            }

            if(isInResumePendingState())
            {
                std::string errmsg;
                int error_num = 0;
                do
                {
                    rv = replay_redo_logs(errmsg,error_num);
                    //If replaying redo logs fails keep retrying if retry is enabled.
                }while(!rv && m_cdp_flush_and_hold_resume_retry && !CDPUtil::QuitRequested(60)); 

                //If sending status to cx failed, retry.
                if(!CDPUtil::QuitRequested())
                {
                    while(!sendFlushAndHoldResumePendingStatusToCx(*TheConfigurator,m_volumename,rv,error_num,errmsg))
                    {
                        if(CDPUtil::QuitRequested(60))
                            break;
                    }
                }

                CDPUtil::QuitRequested(180);
                return false; //TODO : Wait for state change or quit request.
            }
            /*SRM:End*/

            if( isInDiffSync() )
            {
                m_sortCriterion.reset(new DiffSortCriterion(fileToApply.c_str(), m_volumename));
            }


			// ===========================================================
			// Azure specific change
			// if first invocation, 
			//   upload files that are pending
			//   update the last applied file information
			//   delete files that have been uploaded to azure
			//   reset first invocation flag
			// =============================================================

			if (isInDiffSync() && isAzureTarget() && isFirstInvocation()) {

				if (!AzureApplyPendingFilesFromPreviousSession(fileToApply))
				{
					rv = false;
					break;
				}
			}

            //
            // Get the last applied diff file information.
            //
            // We need not read this alway as the information is available in the
            // inmemory structure(m_lastFileApplied). This way we get considerable amount of
            // performance improvement
            //

            if( std::string(m_lastFileApplied.previousDiff.filename) == "none" )
            {
                m_processedDiffInfoAvailable = false;
            }
            else
            {
                m_processedSortCriterion.reset(new DiffSortCriterion(m_lastFileApplied.previousDiff.filename, m_volumename));
                m_processedDiffInfoAvailable = true;
            }



            //
            // TODO: this has to be initialized only in init routine
            // we are currently doing as we are changing the value of this config variables
            // later during apply and using it for memory remaining for drtds
            //
            if(isInDiffSync())
            {
                m_maxMemoryForDrtds = m_configMemoryForDrtds_Diffsync;
            }
            else
            {
                m_maxMemoryForDrtds = m_configMemoryForDrtds_Resync;
            }

            if(isInDiffSync())
            {
                bool stopApplyingDiffs = false;
                if(canSkipDifferential(fileToApply, stopApplyingDiffs))
                {
                    rv = true;
                    break;
                }
                else if (stopApplyingDiffs)
                {
                    rv = false;
                    break;
                }
            }

			//====================================================================
			// Azure Specific changes
			// When the target is azure, 
			//  For resync step2/diffsync
			//   the files are accumulated in <azureCachFolder> until
			//      The upload pending size reaches configured limit OR
			//      The recoverability of next file is modified OR
			//      There are no more files available in PS/cachemgr cache folder
			//   If any of the above conditions are met, it is ready for upload
			// 
			//=====================================================================

			if (isInDiffSync() && isAzureTarget()) {
				rv = AzureApply(fileToApply, lastFileToProcess);
				break;
			}

            boost::shared_array<char> pchanges;

            char * diff_data = NULL;
            SV_ULONG diff_len = 0;

            DiffPtr change_metadata(new (nothrow) Differential);
            if(!change_metadata)
            {
                DebugPrintf(SV_LOG_ERROR, "memory allocation for %s failed.\n",
                            fileToApply.c_str());
                rv = false;
                break;
            }

            PerfTime read_start_time;
            getPerfTime(read_start_time);

            // step 1 : read the file into memory        
            if(m_isFileInMemory)
            {
                // note: buffer is allocated/freed by caller
                diff_data = source;
                diff_len = sourceLength;

                // note: assumption caller has already uncompressed the data
                // TBD: we need to validate this
                m_isCompressedFile = false;

                //update memory availablity
                if(m_maxMemoryForDrtds > sourceLength)
                    m_maxMemoryForDrtds -= sourceLength;
                else
                    m_maxMemoryForDrtds = 0;
            }
            else
            {
                SV_ULONGLONG filesize = 0;
                ACE_stat db_stat ={0};
                if(sv_stat(getLongPathName(fileToApply.c_str()).c_str(),&db_stat) != 0)
                {
                    DebugPrintf( SV_LOG_ERROR, "STAT failed for file: %s.\n", filePath.c_str() );
                    rv = false;
                    break;
                }
                filesize = db_stat.st_size;

                if(filesize <= m_maxMemoryForDrtds)
                {

                    if(!readdatatomemory(fileToApply, &diff_data, diff_len, filesize))
                    {
                        rv = false;
                        break;
                    }
                    m_isFileInMemory = true;
                    pchanges.reset(diff_data);

                    //update memory availablity
                    if(m_maxMemoryForDrtds > diff_len)
                        m_maxMemoryForDrtds -= diff_len;
                    else
                        m_maxMemoryForDrtds = 0;
                }
            }

            PerfTime read_end_time;
            getPerfTime(read_end_time);
            m_diffread_time = getPerfTimeDiff(read_start_time, read_end_time);

            PerfTime uncompress_start_time;
            getPerfTime(uncompress_start_time);

            // step 2: uncompress the buffer        

            // validate assumption: buffer passed by caller is always uncompressed
            if(source && m_isCompressedFile)
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "In memory data from the caller is compressed for %s.\n",
                            fileToApply.c_str());
                rv = false;
                break;
            }

            if(m_isCompressedFile)
            {
                // 
                // if input buffer is supplied:
                //    (1)try inMemoryToInMemory uncompress, 
                //       reset pchanges to uncompressed buffer, adjust memory usage
                //    (2) if it fails, 
                //        try inMemoryToDisk uncompress, 
                //        free pchanges, reduce memory usage, set m_isFileInMemory to false
                //    (3) if this also fails, 
                //        free pchanges, reduce memory usage, try DiskToDiskUncompress, 
                //        set m_isFileInMemory to false
                //    (4)  if this also fails, bail out
                // else if data is on disk
                //       try DiskToDiskUncompress, fails bail out
                // 

                int noOfRetriesLeft = m_noOfUncompressRetries;
                int attempts = 0;
                do
                {
                    rv = true;
                    if(m_isFileInMemory)
                    {
                        char * pgunzip =  NULL;
                        SV_ULONG gunziplen = 0;
                        if(InMemoryToInMemoryUncompress(fileToApply, diff_data, diff_len, &pgunzip, gunziplen))
                        {
                            m_maxMemoryForDrtds += diff_len;
                            pchanges.reset(pgunzip, free);
                            diff_data = pgunzip;
                            diff_len = gunziplen;

                            //update memory availablity
                            if(m_maxMemoryForDrtds > gunziplen)
                                m_maxMemoryForDrtds -= gunziplen;
                            else
                                m_maxMemoryForDrtds = 0;
                        }
                        else
                        {
                            if(uncompressInMemoryToDisk(fileToApply, diff_data, diff_len))
                            {
                                m_maxMemoryForDrtds += diff_len;
                                pchanges.reset();
                                m_isFileInMemory = false;
                            }
                            else
                            {
                                m_maxMemoryForDrtds += diff_len;
                                pchanges.reset();
                                if(!uncompressOnDisk(fileToApply))
                                {
                                    rv = false;
                                }
                                m_isFileInMemory = false;
                            }
                        }
                    }
                    else
                    {
                        if(!uncompressOnDisk(fileToApply))
                        {
                            rv = false;
                        }
                    }

                    if(!rv)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                                    "Attempt: %d to uncompress the diff: %s for target volume: %s failed.\n",
                                    ++attempts, fileToApply.c_str(), m_volumename.c_str());
                    }
                }while(--noOfRetriesLeft && !rv && !CDPUtil::QuitRequested(m_uncompressRetryInterval));

                if(!rv && noOfRetriesLeft != 0)
                {
                    // we came here because quit is requested in the middle
                    rv = false;
                    break;
                }

                // if we failed to uncompress incomming diff file in all the attempts
                if(!rv && noOfRetriesLeft == 0)
                {
                    std::string cacheDir = dirname_r(fileToApply.c_str());
                    SV_ULONGLONG userQuota,totalCapacity,freeSpaceLeft = 0;

                    // get free space available in cache dir
                    if(!GetDiskFreeSpaceP(cacheDir, &userQuota, &totalCapacity, &freeSpaceLeft))
                    {
                        DebugPrintf(SV_LOG_ERROR, "FAILED GetDiskFreeSpaceP for %s: \n",
                                    cacheDir.c_str());
                        freeSpaceLeft = 0;
                    }

                    if(freeSpaceLeft > m_minDiskFreeSpaceForUncompression)
                    {
                        // Uncompress failed while enough free space is available
                        if(m_ignoreCorruptedDiffs)
                        {
                            // Take backup of corrupted diff and continue
                            if(!renameCorruptedFileAndSetResyncRequired(fileToApply))
                            {
                                // backup of corrupted diff failed.
                                // Cannot proceed further return false.
                                rv = false;
                                break;
                            }
                            // backup of corrupted diff is successfull. Return true so the
                            // caller can proceed with rest of the diffs
                            rv = true;
                            break;
                        }
                        else
                        {
                            // This diff may be corrupted but m_ignoreCorruptedDiffs is not set
                            // Print an error and return failure to the caller
                            DebugPrintf(SV_LOG_ERROR,
                                        "Diff file: %s for target volume: %s is corrupted cannot apply.\n",
                                        fileToApply.c_str(), m_volumename.c_str());
                            rv = false;
                            break;
                        }
                    }
                    else
                    {
                        // Enough free space is not avaiable on cache dir which may be an issue
                        // for uncmpress failure. Print an error msg and return false so the caller
                        // may reattempt to apply this diff in next invocation.
                        std::stringstream msg;
                        msg << "Uncompress failed for diff file: " << fileToApply
                            << " for target volume: " << m_volumename
                            << ". Low free space on cache dir " << cacheDir << "("
                            << freeSpaceLeft << "bytes) may be a reason." << std::endl;
                        DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                        rv = false;
                        break;
                    }
                }
            } 

            PerfTime uncompress_end_time;
            getPerfTime(uncompress_end_time);
            m_diffuncompress_time = getPerfTimeDiff(uncompress_start_time, uncompress_end_time);

            PerfTime parse_start_time;
            getPerfTime(parse_start_time);

            // step 3: parse the data
            // if file in memory, try inmemory parse
            // if file on disk, try ondiskparser
            if(m_isFileInMemory)
            {
                if(!parseInMemoryFile(fileToApply, diff_data, diff_len, change_metadata))
                {
                    rv = false;
                }
            }
            else
            {
                if(!parseOnDiskFile(fileToApply, change_metadata))
                {
                    rv = false;
                }
            }

            // if parse failed take backup of the corrupted diff set ResyncRequired and continue
            if(!rv)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to parse the diff %s.\n", FUNCTION_NAME, fileToApply.c_str());
                std::string gzfile = fileToApply + ".gz";
                ACE_stat db_stat ={0};
                if(sv_stat(getLongPathName(gzfile.c_str()).c_str(),&db_stat) == 0)
                {
                    if(ACE_OS::unlink(getLongPathName(fileToApply.c_str()).c_str()) < 0)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                                    "Unable to delete the partially uncompress file %s\n",
                                    fileToApply.c_str());
                    }
                    break;
                }
                if(m_ignoreCorruptedDiffs)
                {
                    // Take backup of unparsable diff and continue
                    if(!renameCorruptedFileAndSetResyncRequired(fileToApply))
                    {
                        // backup of unparsable diff failed.
                        // Cannot proceed further return false.
                        rv = false;
                        break;
                    }
                    // backup of corrupted diff is successfull. Return true so the
                    // caller can proceed with rest of the diffs
                    rv = true;
                    break;
                }
                else
                {
                    // This Sync is corrupted
                    std::stringstream excmsg;
                    excmsg << FUNCTION_NAME << " failed to parse sync file - Sync file: " << fileToApply << " for target volume: " << m_volumename << " is corrupted cannot apply.";
                    throw CorruptResyncFileException(excmsg.str());
                }
            }

            /*If the volume is FLUSH_AND_HOLD_WRITES_PENDING state, check if we crossed the requested point.
             *If we didn't cross the requested point and resync required is set for the volume return a failure to CX.
             *If we crossed the requested point, create redologs and rollback the volume and return statue to CX.
             */
            if(isInFlushAndHoldPendingState())
            {
                bool crossed_requested_ts = false; 
                bool app_consistent_point = false;
                CDPEvent cdpevent;
                bool resync_required = m_Configurator->isResyncRequiredFlag(m_volumename);
                crossed_requested_ts = isCrossedRequestedPoint(change_metadata->starttime(),app_consistent_point,cdpevent);
                if(!crossed_requested_ts && resync_required) //We did not cross the requested point to pause but resync required flag is set.
                {
                    std::string errmsg = "Resync required flag is set for the volume " + m_volumename;
                    while(!sendFlushAndHoldWritesPendingStatusToCx(*TheConfigurator,m_volumename,false,0,errmsg))
                    {
                        if(CDPUtil::QuitRequested(60))
                            break;
                    }
                    CDPUtil::QuitRequested(180);
                    rv = false;//TODO : Wait for state change or quit request
                    break;
                }
                else if (crossed_requested_ts ) 
                {
                    DebugPrintf(SV_LOG_DEBUG,"%s : Flush and Hold Writes Pending: requested point crossed. Rollback volume\n",FUNCTION_NAME);
                    CDPDRTDsMatchingCndn cndn;

                    std::string error_msg;
                    int error_num = 0;
                    do
                    {
                        if(app_consistent_point)
                        {
                            cndn.toEvent(cdpevent.c_eventvalue);
                            cndn.toTime(cdpevent.c_eventtime);
                        }
                        else
                        {
                            cndn.toTime(m_timeto_pause_apply_ts);
                        }

                        if(!is_valid_point(cndn,error_msg))
                        {
                            rv = false;
                            break;
                        }

                        rv = rollback_volume_to_requested_point(cndn,error_msg,error_num);
                    }while(!rv && m_cdp_flush_and_hold_writes_retry && !CDPUtil::QuitRequested(60));

                    if(!CDPUtil::QuitRequested())
                    {
                        while(!sendFlushAndHoldWritesPendingStatusToCx(*TheConfigurator,m_volumename,rv,error_num,error_msg))
                        {
                            if(CDPUtil::QuitRequested(60))
                                break;
                        }
                    }

                    CDPUtil::QuitRequested(180);
                    rv = false; //TODO : Wait for state change or quit request
                    break;
                }
            }

            /* The pair state might get updated in the configurator but we quit only after applying the current diff,
             * so we should not check for pair_state before applying the differential*/
            if(!isInDiffSync() && isResyncDataAlreadyApplied(offset, bm, change_metadata))
            {
                rv = true;
                break;
            }

            PerfTime parse_end_time;
            getPerfTime(parse_end_time);
            m_diffparse_time = getPerfTimeDiff(parse_start_time, parse_end_time);

            // step 4: process any event based snapshots
            if( isInDiffSync() && !process_event_snapshots(change_metadata))
            {
                rv = false;
                break;
            }

            // step 5: update pair replication state
            if( isInDiffSync() && !update_quasi_state(change_metadata))
            {
                rv = false;
                break;
            }


            // step 6: calculate recovery accuracy mode
            if(!set_accuracy_mode(fileToApply,change_metadata))
            {
                rv = false;
                break;
            }

            // step 7: update cdp journal and volume
            if(!updatecdp(m_volumename, fileToApply, diff_data,change_metadata))
            {
                rv = false;
                //If retention disk space is less than free space trigger send an alert to CX.

                if(isInDiffSync() && (get_cdp_version()!=CDP_DISABLED_VALUE))
                {
                    SV_ULONGLONG freespace = 0;
                    if(GetDiskFreeCapacity(m_cdpdir.c_str(),freespace))
                    {
                        if(freespace < (m_cdpfreespace_trigger + m_reserved_cdpspace))
                        {
                            RetentionDirLowSpaceAlert a(m_cdpdir, freespace);
                            DebugPrintf(SV_LOG_DEBUG, "%s\n", a.GetMessage().c_str());
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Applychanges : Could not retrieve disk space for dir %s\n",m_cdpdir.c_str());
                    }

                }
                break;
            }

            totalBytesApplied = change_metadata->DrtdsLength();
            m_diffsize = change_metadata->DrtdsLength();

            // step 8: Update and write the Applied Info structure
            // to cdplastprocessedfile.dat on disk
            if( isInDiffSync() )
            {
                //update previous diff information before deleting 
                if(!update_previous_diff_information(basename_r(fileToApply.c_str()), change_metadata->version()))
                {
                    rv = false;
                    break;
                }
            }


            // step 9: remove the local file
            // we assume if the caller provides the source buffer it takes care of deleting the file
            if(!source)
            {
                if(!DeleteLocalFile(fileToApply))
                {
                    DebugPrintf(SV_LOG_ERROR, "unlink %s failed. error %d\n", 
                                fileToApply.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }
            }

            PerfTime applyend_time;
            getPerfTime(applyend_time);
            m_total_time = getPerfTimeDiff(applystart_time, applyend_time);

            WriteProfilingData(m_volumename);

        } while (0);

    }
    catch (CorruptResyncFileException& dpe) {
        // re-throw CorruptResyncFileException
        throw dpe;
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// =========================================================
//
// Functions for performing ordering checks
//
//	canSkipDifferential
//	isInOrderDiff
//  isDiffEndTimeGreaterThanStartTime
//	isFirstFile
//	isSequentialFile
//	isInOrderDueToS2CommitFailure
//	isOODDueToS2CommitFailure
//	isCrashEventDiff
//	isDiffPriorToCrashEvent
//	isDiffPriorToProcessedFile
//	isMissingDiffAvailable
//	isMissingDiffAvailableInCacheDir
//	isMissingDiffAvailableOnPS
//
// =========================================================

/*
 * FUNCTION NAME : CDPV2Writer::canSkipDifferential
 *
 * DESCRIPTION : Checks if the file can be skipped
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - helpful in finding the resync start time, transport settings etc.
 * 
 * OUTPUT PARAMETERS : stopApplyingDiffs - This will be set to true in cases like
 *                           1.) the current diff is OOD but AllowOutOfOrderSeq and AllowOutOfOrderTS
 *                               are set to false
 *                           2.) if the current diff should not be applied and also cannot be ignored.
 *                               For example: we got a crash event file but we failed to persist this info
 *                                            In this case we can neither apply this file nor we can
 *                                            ignore it. So stopApplyingDiffs will be set to true.
 *
 * Algorithm : 
 *     CanSkipDifferential:
 *        1.) If this diff is not from this resync session
 *             a. Delete the diff
 *             b. return true(i.e. Continue with rest of the diffs)
 *        2.) If this is a repeating diff
 *             a. Delete the diff 
 *             b. return true(i.e. Continue with rest of the diffs)
 *        3.) If this is follower of the processed differetial
 *             a. return false(i.e. we need not skip this diff, apply it)
 *            Else
 *             a. If this is a version 1 diff
 *                 i. If m_setResyncRequiredOnOOD is set
 *                     1. Set Resync Required
 *                     2. return false(i.e. we need not skip this diff, apply it)
 *                 ii. Else
 *                     1. Log an error message
 *                     2. Stop applying diffs
 *             b. If this is a crash event diff
 *                 i. Persist crash event
 *                 ii. return false(i.e. we need not skip this diff, apply it)
 *             c. If this diff is prior to crash event
 *                 i. Delete this diff
 *                 ii. return true(i.e. Continue with rest of the diffs)
 *             d. If this diff is OOD due to s2 commit failure
 *                 i. Delete diff
 *                 ii. Set Resync Required
 *                 iii. return true(i.e. Continue with rest of the diffs)
 *             e. If this diff is prior to processed diff
 *                 i. If m_setResyncRequiredOnOOD is set
 *                     1. Delete this diff 
 *                     2. Set Resync Required
 *                     3. return true(i.e. Continue with rest of the diffs)
 *                 ii. Else
 *                     1. Log an error message
 *                     2. Stop applying diffs
 *             f. This is an OOD file
 *                 i. If m_setResyncRequiredOnOOD is set
 *                     1. Check the availability of the expected diff in cache dir and target GUID
 *                        If the such file is not found
 *                         a. Log an OOD message
 *                         b. Set Resync Required
 *                         c. Stop applying diffs for this run, so that the missing file will be
 *                            processed in next invocation
 *                     2. else
 *                         a. Stop applying diffs for this run, so that the missing file will be
 *                            processed in next invocation
 *                 ii. Else
 *                     1. Log an error message
 *                     2. Stop applying diffs
 *
 * return value : true if we can skip this diff and apply the rest of them
 *                false if we need not skip this diff. In this case the output parameter
 *                'stopApplyingDiffs' will be set to true when we need to wait for other diffs
 *                or will be set to false if we can apply the current diff
 *
 */
bool CDPV2Writer::canSkipDifferential(const std::string& filePath, bool& stopApplyingDiffs)
{
    bool skipDiff = true;
    stopApplyingDiffs = true;

    do
    {
        //Step 1: Is this diff from current resync session
        // All the diffs which contain the end time < resync start time should be of previous resync
        // session. All such files can be ignored(just unlink and continue with rest of the diffs)
        if(!isDiffFromCurrentResyncSession(filePath))
        {
            // PR#10815: Long Path support
            if( m_transport_protocol != TRANSPORT_PROTOOL_MEMORY && 
                ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
            {
                std::stringstream msg;
                msg << "Failed to delete diff: " << filePath << " for target volume: "
                    << m_volumename << " which is from previous resync session. "
                    << "error no: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                skipDiff = false;
                stopApplyingDiffs = true;
                break;
            }
            //as the file is from different resync session we need not apply
            //just return true so the caller will apply rest of the diffs
            skipDiff = true;
            break;
        }

        //Step 2: Is this a repeating differential(is it same as last processed diff)

        //remove '.gz' from current diff file name if it exists
        std::string currentDiff = basename_r(filePath.c_str());
        std::string::size_type idx = currentDiff.rfind(".gz");
        if( (idx != std::string::npos) && (currentDiff.length() - idx) == 3 )
            currentDiff.erase(idx, currentDiff.length());

        //remove '.gz' from processed diff file name if it exists
        std::string ProcessedDiff = m_lastFileApplied.previousDiff.filename;
        idx = ProcessedDiff.rfind(".gz");
        if( (idx != std::string::npos) && (ProcessedDiff.length() - idx) == 3 )
            ProcessedDiff.erase(idx, ProcessedDiff.length());

        //check if the processed file is same as the current diff file
        if( ProcessedDiff == currentDiff )
        {
            //This diff is repeating
            DebugPrintf(SV_LOG_ERROR, "Note: This is not an error: Got a repeating diff file %s. Ignoring it.\n",
                        filePath.c_str());
            // PR#10815: Long Path support
            if(m_transport_protocol != TRANSPORT_PROTOOL_MEMORY && 
               ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to delete repeating file %s. error no: %d\n",
                            filePath.c_str(), ACE_OS::last_error());
                skipDiff = false;
                stopApplyingDiffs = true;
                break;
            }
            skipDiff = true;
            break;
        }

        //Step 3: Is this a follower of the processed differetial
        if( !isInOrderDiff(filePath) )
        {
            std::string crashEventDiffFileName = "";

            //Step a: Is the diff file version going back
            //i.e. did we get a older version diff than last processed diff
            //this can happen in cases like upgrading the active cluster node and
            //failover to the passive node which is not yet upgraded
            if( m_processedDiffInfoAvailable
                && m_sortCriterion->IdVersion() < m_processedSortCriterion->IdVersion() )
            {
                if(m_setResyncRequiredOnOOD)
                {
                    //set resync required and apply the diff
                    std::stringstream msg;
                    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                    msg << "Got a version " << m_sortCriterion->IdVersion() << " diff: " << m_sortCriterion->Id()
                        << " after applying version " << m_processedSortCriterion->IdVersion() << " diff: "
                        << m_processedSortCriterion->Id() << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                    ResyncReasonStamp resyncReasonStamp;
                    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                    m_resyncRequired = true;
                    if(!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                    {
                        skipDiff = false;
                        stopApplyingDiffs = true;
                        break;
                    }
                    skipDiff = false;
                    stopApplyingDiffs = false;
                    break;
                }
                else
                {
                    //stop applying the diffs as m_setResyncRequiredOnOOD is not set
                    std::stringstream msg;
                    msg << "Got a version " << m_sortCriterion->IdVersion() << " diff: " << m_sortCriterion->Id()
                        << " after applying version " << m_processedSortCriterion->IdVersion() << " diff: "
                        << m_processedSortCriterion->Id() << "\nStopped applying diffs for the target volume "
                        << m_volumename << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
            }

            //Step b: Is this a crash event differential file(generated by source if it comes up after crash)
            else if(isCrashEventDiff(filePath))
            {
                //persist the crash event
                persistSourceSystemCrashEvent(filePath);
                skipDiff = false;
                stopApplyingDiffs = false;
                break;
            }

            //Step c: Is this diff prior to crash event
            // When we process a crash event file we are not sure if we missed any diff prior to
            // this diff as the previous end time is not available in the crash event file.
            // So there is a chance of getting the file older than the crash event file. Such files
            // can be ignored as source sets resync required when it sends a crash event file.
            else if(isSourceSystemCrashEventSet(crashEventDiffFileName)
                    && isDiffPriorToCrashEvent(filePath, crashEventDiffFileName))
            {
                DebugPrintf(SV_LOG_ERROR, "Note: This is not an error:\n"
                            "Got a diff(%s) which is prior to crash event: %s. Ignoring...\n",
                            filePath.c_str(), crashEventDiffFileName.c_str());
                if(m_transport_protocol != TRANSPORT_PROTOOL_MEMORY &&
                   ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to delete diff: %s which is prior to crash event."
                                " error no: %d\n", filePath.c_str(), ACE_OS::last_error());
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
                skipDiff = true;
                break;
            }

            //Step d: Is this diff an out of order diff due to s2 commit failure
            // In case if s2 sends and renames a diff file on the PS but fails to commit this
            // dirty block to the driver, driver may return the previous time same as for the
            // processed diff's previous end time. In such case we need to figure out if the
            // diff we are processing is out of order based on the end times of the processed
            // and the current differentials.
            else if(m_processedDiffInfoAvailable && isOODDueToS2CommitFailure(filePath))
            {
                std::stringstream msg;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                msg << "Recieved a file of lesser timestamp or sequence due to s2 commit failure."
                    << " Incoming file: " << m_sortCriterion->Id() << std::endl
                    << ", Last processed file: "	<< m_processedSortCriterion->Id()
                    << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                m_resyncRequired = true;
                if(!CDPUtil::store_and_sendcs_resync_required(m_volumename,msg.str(), resyncReasonStamp))
                {
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
                if(m_transport_protocol != TRANSPORT_PROTOOL_MEMORY && 
                   ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to delete the file %s. error no: %d\n",
                                filePath.c_str(), ACE_OS::last_error());
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
                skipDiff = true;
                break;
            }

            //Step e: Is this diff proir to procesed file
            // Applying files that are older than the processed differential may increase the difference
            // between the source and target volume if there are overlapping changes or are too older.
            // So we do not apply such files rather we stop applying diffs or just set resync,
            // delete them and continue applying the rest of the diffs
            else if(m_processedDiffInfoAvailable && isDiffPriorToProcessedFile(filePath))
            {
                if(m_setResyncRequiredOnOOD)
                {
                    //set resync required and continue applying rest of the diffs
                    std::stringstream msg;
                    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                    msg << "Recieved a file of lesser timestamp or sequence for " << m_volumename
                        << ". Incoming File: " << m_sortCriterion->Id()
                        << ", Last Processed file: " << m_processedSortCriterion->Id()
                        << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                    ResyncReasonStamp resyncReasonStamp;
                    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                    if(!m_ignoreOOD)
                    {
                        m_resyncRequired = true;
                        if(!CDPUtil::store_and_sendcs_resync_required(m_volumename,msg.str(), resyncReasonStamp))
                        {
                            skipDiff = false;
                            stopApplyingDiffs = true;
                            break;
                        }
                    }

                    if(m_transport_protocol != TRANSPORT_PROTOOL_MEMORY && 
                       ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to delete file %s"
                                    " error no: %d\n",
                                    filePath.c_str(), ACE_OS::last_error());
                        skipDiff = false;
                        stopApplyingDiffs = true;
                        break;
                    }
                    skipDiff = true;
                    break;
                }
                else
                {
                    //stop applying the diffs as m_setResyncRequiredOnOOD is not set
                    std::stringstream msg;
                    msg << "Recieved a file of lesser timestamp or sequence for " << m_volumename << std::endl
                        << "Incoming File: " << m_sortCriterion->Id() << std::endl
                        << "Last Processed file: " << m_processedSortCriterion->Id() << std::endl
                        << "\nStopped applying diffs for the target volume " << m_volumename << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
            }
            else
            {
                // This is an OOD. i.e. We missed one or more diff files between the processed
                // diff and the current one, or we might have missed the first file itself
                if(m_setResyncRequiredOnOOD)
                {
                    // check if the missing file is availavle on PS or in cache dir
                    if(!isMissingDiffAvailable(filePath))
                    {
                        // If the missing diff is not available set resync required and apply the current diff
                        std::stringstream msg;
                        const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                        if(m_processedDiffInfoAvailable)
                        {
                            msg << "Few files are missing for " << m_volumename
                                << ". Incoming file: " << m_sortCriterion->Id()
                                << ", Last Processed file: "	<< m_processedSortCriterion->Id()
                                << ". Marking resync for the target device " << m_volumename << ".";
                        }
                        else
                        {
                            msg << "Missing first file for " << m_volumename << std::endl
                                << ". Incoming file: " << m_sortCriterion->Id() << std::endl
                                << ", Resync start timestamp: " << m_Configurator->getResyncStartTimeStamp(m_volumename)
                                << ", sequence no: " << m_Configurator->getResyncStartTimeStampSeq(m_volumename) << std::endl
                                << ". Marking resync for the target device " << m_volumename << ".";
                        }
                        msg << " with resyncReasonCode=" << resyncReasonCode;
                        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                        ResyncReasonStamp resyncReasonStamp;
                        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                        m_resyncRequired = true;
                        if(!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                        {
                            skipDiff = false;
                            stopApplyingDiffs = true;
                            break;
                        }
                        skipDiff = false;
                        stopApplyingDiffs = false;
                        break;
                    }
                    else
                    {
                        // We found the missing diff on PS or target cache dir lets return false and stop
                        // applying diffs, so in the next invocation we get a chance to process the missing diff
                        skipDiff = false;
                        stopApplyingDiffs = true;
                        break;
                    }
                }
                else
                {
                    //stop applying the diffs as m_setResyncRequiredOnOOD is not set
                    std::stringstream msg;
                    if(m_processedDiffInfoAvailable)
                    {
                        msg << "Few files are missing for " << m_volumename << std::endl
                            << " Incoming file: " << m_sortCriterion->Id() << std::endl
                            << " Last Processed file: "	<< m_processedSortCriterion->Id() << std::endl
                            << "Stopped applying diffs for the target volume "
                            << m_volumename << std::endl;
                    }
                    else
                    {
                        msg << "Missing first file for " << m_volumename << std::endl
                            << "Incoming file: " << m_sortCriterion->Id() << std::endl
                            << "Resync start timestamp: " << m_Configurator->getResyncStartTimeStamp(m_volumename)
                            << ", sequence no: " << m_Configurator->getResyncStartTimeStampSeq(m_volumename) << std::endl
                            << "Stopped applying diffs for the target volume " << m_volumename << std::endl;
                    }
                    DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                    skipDiff = false;
                    stopApplyingDiffs = true;
                    break;
                }
            }
        }
        else
        {
            skipDiff = false;
            stopApplyingDiffs = false;
            break;
        }
    }while(false);

    // Handled following scenarios as part of Bug 17417959: [MT forward]
    // Improper diff resulting from driver version 9.53.6594.1 with end time less
    // than start time causing replication to get stuck.
    //
    // Scenario 1: Diff file has end timestamp less than previous timestamp
    // Last applied file: P100_E110
    // Pending files to process:
    // P114_E112->chain is broken.File ETS is higher than Last applied file.
    //            Missing diff matches P110_E114 and replication gets stuck
    //            in infinite loop.
    // P110_E114
    //
    // Solved by:
    // (Solution.A) E{TS} < P{TS} in the file that is being processed.
    // So delete this file and mark for resync.
    //
    // Scenario 2:
    // Last applied file: P100_E110
    // Pending files to process:
    // P99_E111->chain is broken, file ETS is higher. Missing diff will match with
    //          P110_E114 and replication will continue to be in infinite loop.
    // P110_E114
    //
    // Solved by:
    // (Solution.B) E{TS} of sequential file(P110_E114} is higher than file being
    // processed(P99_E111).File is sent in incorrect order by source. Replication
    // is marked for resync.
    //
    // Scenario 3:
    // Last applied file: P100_E110
    // Pending files to process:
    // P110_E102-> chain is intact.
    // P102_E113-> chain is again intact.
    // Replication does not get stuck, but is a potential DI scenario.
    //
    // Solved by:
    // (Solution.A) E{ TS} < P{ TS} in the file that is being processed.
    // So delete this file and mark for resync.
    if (!isDiffEndTimeGreaterThanStartTime(filePath))
    {
        std::stringstream msg;
        msg << "Recieved a file with end time stamp less than start time stamp. "
            << "Incoming file: " << m_sortCriterion->Id() << std::endl;
        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());

        if (m_resyncRequired)
        {
            // no DI issue as resync is already marked, delete this file as it is not properly formed.
            if (m_transport_protocol != TRANSPORT_PROTOOL_MEMORY &&
                ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to delete the file %s. error no: %d\n",
                    filePath.c_str(), ACE_OS::last_error());
                skipDiff = false;
                stopApplyingDiffs = true;
            }
            else
            {
                // File was successfully deleted.
                skipDiff = true;
            }
        }
        else
        {
            TRANSPORT_CONNECTION_SETTINGS transportsettings = m_Configurator->getTransportSettings(m_volumename);

            // Check for silent DI issue
            if (m_transport_protocol == TRANSPORT_PROTOCOL_FILE &&
                isSilentDataIntergityIssueDetected(filePath, transportsettings))
            {
                std::stringstream msg;
                msg << "Improper file: " << m_sortCriterion->Id() << "is in order wrt to last processed file. "
                    << "Potential DI issue detected!";
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
            }

            if (m_setResyncRequiredOnOOD)
            {
                do
                {
                    std::stringstream msg;
                    const std::string& resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                    msg << "Recieved a file with end time stamp less than start time stamp."
                        << " Incoming file: " << m_sortCriterion->Id() << std::endl
                        << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                    ResyncReasonStamp resyncReasonStamp;
                    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                    m_resyncRequired = true;
                    if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                    {
                        skipDiff = false;
                        stopApplyingDiffs = true;
                        break;
                    }
                    if (m_transport_protocol != TRANSPORT_PROTOOL_MEMORY &&
                        ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to delete the file %s. error no: %d\n",
                            filePath.c_str(), ACE_OS::last_error());
                        skipDiff = false;
                        stopApplyingDiffs = true;
                        break;
                    }

                    skipDiff = true;
                    break;

                } while (false);
            }
            else
            {
                //stop applying the diffs as m_setResyncRequiredOnOOD is not set
                std::stringstream msg;
                msg << "Recieved a file with end time stamp less than start time stamp."
                    << " Incoming file: " << m_sortCriterion->Id() << std::endl
                    << ". Stopped applying diffs for the target volume "
                    << m_volumename << std::endl;
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                skipDiff = false;
                stopApplyingDiffs = true;
            }
        }
    }

    return skipDiff;
}

/*
 * FUNCTION NAME : CDPV2Writer::isDiffEndTimeGreaterThanStartTime
 *
 * DESCRIPTION : Checks if this diff is properly formed and has end time greater than start time
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if the diff end time is greater than start time, false otherwise
 *
 */
bool CDPV2Writer::isDiffEndTimeGreaterThanStartTime(const std::string& filePath)
{
    return m_sortCriterion->EndTime() > m_sortCriterion->PreviousEndTime() ||
        (m_sortCriterion->EndTime() == m_sortCriterion->PreviousEndTime() &&
            m_sortCriterion->EndTimeSequenceNumber() >
            m_sortCriterion->PreviousEndTimeSequenceNumber()) ||
        (m_sortCriterion->EndTime() == m_sortCriterion->PreviousEndTime() &&
            m_sortCriterion->EndTimeSequenceNumber() ==
            m_sortCriterion->PreviousEndTimeSequenceNumber() &&
            m_sortCriterion->ContinuationId() > m_sortCriterion->PreviousContinuationId());
}

/*
 * FUNCTION NAME : CDPV2Writer::isInOrderDiff
 *
 * DESCRIPTION : Checks if this diff is in order with respect to the last processed diff
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - helpful in finding the resync start time, transport settings etc.
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if current diff is in order diff, false otherwise
 *
 */
bool CDPV2Writer::isInOrderDiff(const std::string& filePath)
{
    bool rv = false;

    switch(m_sortCriterion->IdVersion())
    {
	case DIFFIDVERSION1:
            // Version 1 diffs do not contain sufficient information for finding if we missed
            // any diff inbetween. For this reason we just need to return true.
            //
            // One special case is that if the last processed file is a version 3 diff and
            // we are now seeing version 1 diffs, it means there is some thing wrong at the source
            // in such case we return false.
            if(m_processedDiffInfoAvailable)
            {
                if(m_processedSortCriterion->IdVersion() > m_sortCriterion->IdVersion())
                {
                    //Processed file is a newer file than the current one.
                    //This can be due to improper order of cluster node upgrades.
                    rv = false;
                    break;
                }
            }

            //ordering checks are not applicable to version 1 diffs. So ver 1 files are
            //always considered as in order files
            rv = true;
            break;

	case DIFFIDVERSION3:
            // Version 3 files contain end time of the previous differential in the file name
            // which helps in finding out the missing diff files. The following checks 
            // are for finding out if we are processing the files in proper order.
            //
            // For the first differential we do not have the last processed file info for
            // performing ordering checks. So we find if current diff is first diff file based
            // on special tests such as the file name containing P0_0_0, resyncStartTagTime
            // falling inbetween previous end time and current end time.
            if(isFirstFile(filePath))
            {
                rv = true;
                break;
            }
            // if this file is not a first file, we should have applied the first file in the last run
            // and hence we should have the last processed info available for performing the ordering checks
            // If we dont have this info we return false as we cannot perform the ordering checks
            else if( !m_processedDiffInfoAvailable || m_processedSortCriterion->IdVersion() < DIFFIDVERSION3 )
            {
                rv = false;
                break;
            }
            // Check if this diff is a successor of the last processed diff file
            else if (isSequentialFile(filePath))
            {
                rv = true;
                break;
            }
            // There is an exceptional case where the last processed files end time stamp doesn't
            // match the current files previous end time.
            // s2 sends a file to PS and successfulluy renames it but fails to commit this to the
            // driver. In such case the new diff contains previous end time same as that of the
            // file already sent to PS but the end times may differ.
            // This check helps us to find out if we are applying diffs in proper order in such situation
            else if(isInOrderDueToS2CommitFailure(filePath))
            {
                rv = true;
                break;
            }

            // if we did not return from any above conditions it means we are processing out of order
            // set rv = false
            rv = false;
            break;
    }

    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isFirstFile
 *
 * DESCRIPTION : Finds if this is a first file or this resync session base on conditions like
 *               file name containing P0_0_0, or based on the resync start time
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - helpful in finding the resync start time, transport settings etc.
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this is a valid first file, false otherwise
 *
 */
bool CDPV2Writer::isFirstFile(const std::string& filePath)
{
    bool rv = false;

    do
    {
        // If the previous endtime == 0 it means this is the first file
        if( m_sortCriterion->PreviousEndTime() == 0 &&
            m_sortCriterion->PreviousEndTimeSequenceNumber() == 0 &&
            m_sortCriterion->PreviousContinuationId() == 0 )
        {
            //File with previous endtime == 0 is allowed only in 2 cases
            //1.) when processedFileInfo is not available.
            if( !m_processedDiffInfoAvailable )
            {
                rv = true;
                break;
            }
            //2.) when the processed file is of lesser version than the current file
            else
            {
                if( m_processedSortCriterion->IdVersion() < DIFFIDVERSION3 )
                {
                    rv = true;
                    break;
                }
            }

            //as the above 2 cases are not true, this cannot be first file
            //theres something wrong
            std::stringstream msg;
            msg << "Got a diff with preEndTime == 0 for target volume: " << m_volumename
                << ". But this is not a first file." << std::endl
                << "Last processed file: " << m_lastFileApplied.previousDiff.filename << std::endl
                << "Current diff file: " << filePath << std::endl;
            DebugPrintf( SV_LOG_ERROR, "%s", msg.str().c_str() );
            rv = false;
            break;
        }
        // if the resync start time falls in between the previous end time and the current end time
        // this should be a first file
        else if(isFirstFileForThisResyncSession(*m_sortCriterion.get()))
        {
            rv = true;
            break;
        }
        else
        {
            // if the last processed file is an older version file and we got a version 3 file
            // return true
            // this is possible in case failover from a older agent node to an already upgraded cluster node
            if( m_processedDiffInfoAvailable && m_processedSortCriterion->IdVersion() < DIFFIDVERSION3)
            {
                rv = true;
                break;
            }
        }
    }while(false);

    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isSequentialFile
 *
 * DESCRIPTION : Checks if this diff is a successer of the last processed diff
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this is a successer of the last processed diff, false otherwise
 *
 */
bool CDPV2Writer::isSequentialFile(const std::string& filePath)
{
    if(m_processedDiffInfoAvailable)
    {
        // Normal case where processed file's end time == current file's prev end time
        if( m_processedSortCriterion->EndTime() == m_sortCriterion->PreviousEndTime() &&
            m_processedSortCriterion->EndTimeSequenceNumber() == m_sortCriterion->PreviousEndTimeSequenceNumber() &&
            m_processedSortCriterion->ContinuationId() == m_sortCriterion->PreviousContinuationId() )
        {
            return true;
        }
    }

    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isInOrderDueToS2CommitFailure
 *
 * DESCRIPTION : In case if s2 sends and renames a diff file on the PS but fails to commit this
 *               dirty block to the driver, driver may return the previous time same as for the
 *               pervious diff.
 *               In such case we cannot decide if this diff is in order diff or out of order diff
 *               unless we have the end times of both the diff files. So we apply the file which
 *               we see first and when we find that the next file is due to s2 commit failure we
 *               check the end times of the processed file and the current diff to decide if the
 *               order we are appling is correct
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true is this is in order due to s2 commit failure, false otherwise
 *
 */
bool CDPV2Writer::isInOrderDueToS2CommitFailure(const std::string& filePath)
{
    if( m_processedDiffInfoAvailable )
    {
        // In case if s2 sends and renames a diff file on the PS but fails to commit this
        // dirty block to the driver, driver may return the previous time same as for the
        // pervious diff. This check is to handle such case
        if( m_processedSortCriterion->PreviousEndTime() == m_sortCriterion->PreviousEndTime() &&
            m_processedSortCriterion->PreviousEndTimeSequenceNumber() == m_sortCriterion->PreviousEndTimeSequenceNumber() &&
            m_processedSortCriterion->PreviousContinuationId() == m_sortCriterion->PreviousContinuationId() )
        {
            // out of the two files with same previous endtime find out which is recent one.
            // If the current diff is recent one
            //     no issue just return true
            if( m_processedSortCriterion->EndTime() < m_sortCriterion->EndTime() ||
                (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
                 m_processedSortCriterion->EndTimeSequenceNumber() < m_sortCriterion->EndTimeSequenceNumber()) ||
                (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
                 m_processedSortCriterion->EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
                 m_processedSortCriterion->ContinuationId() <= m_sortCriterion->ContinuationId()) )
            {
                std::stringstream msg;
                msg << "Note: This is not an error: Applying diff file " << filePath
                    << " after applying " << m_processedSortCriterion->Id()
                    << " to the target volume " << m_volumename << ". "
                    << "This is possible if s2 failed commiting dirty block to the driver,"
                    << " but commited to CX..." << std::endl;
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                return true;
            }
        }
    }

    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isOODDueToS2CommitFailure
 *
 * DESCRIPTION : In case if s2 sends and renames a diff file on the PS but fails to commit this
 *               dirty block to the driver, driver may return the previous time same as for the
 *               pervious diff.
 *               In such case we cannot decide if this diff is in order diff or out of order diff
 *               unless we have the end times of both the diff files. So we apply the file which
 *               we see first and when we find that the next file is due to s2 commit failure we
 *               check the end times of the processed file and the current diff to decide if the
 *               order we are appling is correct
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this file is out of order due to s2 commit failure, false otherwise
 *
 */
bool CDPV2Writer::isOODDueToS2CommitFailure(const std::string& filePath)
{
    if( m_processedDiffInfoAvailable )
    {
        // In case if s2 sends and renames a diff file on the PS but fails to commit this
        // dirty block to the driver, driver may return the previous time same as for the
        // pervious diff. This check is to handle such case
        if( m_processedSortCriterion->PreviousEndTime() == m_sortCriterion->PreviousEndTime() &&
            m_processedSortCriterion->PreviousEndTimeSequenceNumber() == m_sortCriterion->PreviousEndTimeSequenceNumber() &&
            m_processedSortCriterion->PreviousContinuationId() == m_sortCriterion->PreviousContinuationId() )
        {
            // if the processed file newer than the current diff, we processed OOD, return true
            if( m_processedSortCriterion->EndTime() > m_sortCriterion->EndTime() ||
                (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
                 m_processedSortCriterion->EndTimeSequenceNumber() > m_sortCriterion->EndTimeSequenceNumber()) ||
                (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
                 m_processedSortCriterion->EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
                 m_processedSortCriterion->ContinuationId() > m_sortCriterion->ContinuationId()) )
            {
                return true;
            }
        }
    }

    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isCrashEventDiff
 *
 * DESCRIPTION : When the source is not able to figure out the previous end time for some reason
 *               like persistent store corruption due to source system crash, it sends a diff file
 *               with previous end time set to MAX values. This function checks if the current
 *               diff is such file 
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this is a crash event file, false otherwise
 *
 */
bool CDPV2Writer::isCrashEventDiff(const std::string& filePath)
{
    // If we get the previous timestamp as MAX it means the driver was
    // not able to figure out the previous time stamp(likely in a crash event)
    if( m_sortCriterion->PreviousEndTime() == 0xFFFFFFFFFFFFFFFFULL &&
        (m_sortCriterion->PreviousEndTimeSequenceNumber() == 0xFFFFFFFFFFFFFFFFULL ||
         m_sortCriterion->PreviousEndTimeSequenceNumber() == 0xFFFFFFFF) &&
        m_sortCriterion->PreviousContinuationId() == 0xFFFFFFFF )
    {
        return true;
    }

    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isDiffPriorToCrashEvent
 *
 * DESCRIPTION : When a crash event diff is sent by source, target persists this information in a
 *               local file. This information is used for finding out if we are processing diffs
 *               prior to crash event. If they are older they can be dropped without applying to
 *               target as resync required will be set by source
 *
 * INPUT PARAMETERS : filePath - path to current diff file
 *                    crashEventDiffFileName - crash event diff file name which helps in finding the
 *                    end time of the crash event diff
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this diff is prior to creas event diff's end time, false otherwise
 *
 */
bool CDPV2Writer::isDiffPriorToCrashEvent(const std::string& filePath, const std::string& crashEventDiffFileName)
{
    DiffSortCriterion crashEventSortCriterion(crashEventDiffFileName.c_str(), m_volumename);

    if( crashEventSortCriterion.EndTime() > m_sortCriterion->EndTime() ||
        (crashEventSortCriterion.EndTime() == m_sortCriterion->EndTime() &&
         crashEventSortCriterion.EndTimeSequenceNumber() > m_sortCriterion->EndTimeSequenceNumber()) ||
        (crashEventSortCriterion.EndTime() == m_sortCriterion->EndTime() &&
         crashEventSortCriterion.EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
         crashEventSortCriterion.ContinuationId() > m_sortCriterion->ContinuationId()) )
    {
        return true;
    }
    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isDiffPriorToProcessedFile
 *
 * DESCRIPTION : Checks the end time of the current diff and the last processed file
 *               and figures out if the current diff is older than the last processed file
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if this diff's end time is prior to the processed file's end time
 *                false otherwise
 *
 */
bool CDPV2Writer::isDiffPriorToProcessedFile(const std::string& filePath)
{
    if( m_processedDiffInfoAvailable )
    {
        if( m_processedSortCriterion->EndTime() > m_sortCriterion->EndTime() ||
            (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
             m_processedSortCriterion->EndTimeSequenceNumber() > m_sortCriterion->EndTimeSequenceNumber()) ||
            (m_processedSortCriterion->EndTime() == m_sortCriterion->EndTime() &&
             m_processedSortCriterion->EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
             m_processedSortCriterion->ContinuationId() > m_sortCriterion->ContinuationId()) )
        {
            return true;
        }
    }

    return false;
}

/*
 * FUNCTION NAME : CDPV2Writer::isMissingDiffAvailable
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - helpful in finding the resync start time, transport settings etc.
 * 
 * OUTPUT PARAMETERS : none
 *
 * Algorithm : 
 *
 *    1. Check the cache dir for the file that has the previous end time = processed file’s end time.
 *       If found such file, return true
 *    2. Check the target GUID in CX for the file that has the previous end time = processed file’s
 *       end time. If found such file
 *           a. sleep 180 seconds honoring quit request
 *           b. return true
 *    3. Sleep for 60 seconds honoring quit request
 *    4. Retry steps 1 - 3 three times
 *
 * return value : return true if the missing diff is found in PS or cache dir, false otherwise
 *
 */
bool CDPV2Writer::isMissingDiffAvailable(const std::string& filePath)
{
    bool rv = false;

    try
    {
        int noOfRetries = 3;
        int waitTimeBetweenRetries = 60; //seconds
        TRANSPORT_CONNECTION_SETTINGS transportsettings = m_Configurator->getTransportSettings(m_volumename);

        do
        {
            DebugPrintf(SV_LOG_ERROR, "Note: This is not an error: Waiting for missing diff file. volume:%s file : %s\n",
                        m_volumename.c_str(), filePath.c_str());


            //check in the cache directory
            if( m_transport_protocol != TRANSPORT_PROTOOL_MEMORY && 
                isMissingDiffAvailableInCacheDir(filePath, transportsettings) )
            {
                rv = true;
                break;
            }

            //check in the target GUID on PS
            if(/*target is not same as PS*/m_transport_protocol != TRANSPORT_PROTOCOL_FILE &&
               m_transport_protocol != TRANSPORT_PROTOOL_MEMORY )
            {
                if(isMissingDiffAvailableOnPS(filePath,  transportsettings))
                {
                    //File is on PS and cachemgr may take some time to download the diff
                    //Wait for some time and exit
                    DebugPrintf(SV_LOG_DEBUG, "Note: This is not an error: Waiting for the missing diff file...\n");
                    int waitTimeForDownloadingTheDiff = 180; //seconds
                    if(CDPUtil::QuitRequested(waitTimeForDownloadingTheDiff))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Quit is requested by service, not waiting for missing diff.\n");
                    }
                    rv = true;
                    break;
                }
            }
        }while(--noOfRetries && !rv && !CDPUtil::QuitRequested(waitTimeBetweenRetries));
    }

    // on exception, we will consider file is available in cache directory or PS
    // and let dataprotection exit and re-check on next invocation
    catch ( ContextualException& ce )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volumename.c_str());
    }
    catch( exception const& e )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volumename.c_str());
    }
    catch ( ... )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volumename.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isMissingDiffAvailableInCacheDir
 *
 * DESCRIPTION : This function lists the diffs in the cache dir and finds if the expected(missing diff)
 *               diff is available there
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - 
 *                    transportsettings - transport settings for this pair
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if the missing file is found in cache dir, false otherwise
 *
 */
bool CDPV2Writer::isMissingDiffAvailableInCacheDir(const std::string& filePath, 
                                                   const TRANSPORT_CONNECTION_SETTINGS& transportsettings)
{
    bool rv = false;

    try
    {
        do
        {
            std::string cacheLocation = dirname_r(filePath.c_str());

            const std::string lockPath = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A + "pending_diffs.lck";
            // PR#10815: Long Path support
            ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
            ACE_Guard<ACE_File_Lock> guard(lock);

            if(!guard.locked())
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                            FUNCTION_NAME,ACE_OS::last_error(),
                            ACE_OS::strerror(ACE_OS::last_error()),lockPath.c_str());

                // on exception, we will consider file is available in cache directory or PS
                // and let dataprotection exit and re-check on next invocation
                rv = true;
                break;
            }

            CxTransport::ptr cxTransport;
            try {
                cxTransport.reset(new CxTransport(TRANSPORT_PROTOCOL_FILE,
                                                  transportsettings,
                                                  m_secure_mode,
                                                  m_cfsPsData.m_useCfs,
                                                  m_cfsPsData.m_psId));
            }
            // on exception, we will consider file is available in cache directory or PS
            // and let dataprotection exit and re-check on next invocation
            catch (std::exception const & e) 
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: %s\n", FUNCTION_NAME, e.what());
                rv = true;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                rv = true;
                break;
            }

            std::string fullLocation = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            fullLocation += m_Configurator->getVxAgent().getDiffTargetFilenamePrefix();
            FileInfos_t fileInfos;
            if (!cxTransport->listFile(fullLocation, fileInfos))
            {
                DebugPrintf(SV_LOG_ERROR,
                            "FAILED %s cxTransport->GetFileList %s failed with error:%s\n",
                            FUNCTION_NAME, fullLocation.c_str(), cxTransport->status());
                rv = true;
                break;
            }


            DiffSortCriterion::OrderedEndTime_t orderedList;
            // prepare an ordered list
            FileInfos_t::iterator iter(fileInfos.begin());
            FileInfos_t::iterator iterEnd(fileInfos.end());
            for (/* empty */; iter != iterEnd; ++iter) {
                orderedList.insert(DiffSortCriterion((*iter).m_name.c_str(), m_volumename));
            }

            if(!orderedList.empty())
            {
                if(m_processedDiffInfoAvailable)
                {
                    if( (*orderedList.begin()).PreviousEndTime() == m_processedSortCriterion->EndTime() &&
                        (*orderedList.begin()).PreviousEndTimeSequenceNumber() == m_processedSortCriterion->EndTimeSequenceNumber() &&
                        (*orderedList.begin()).PreviousContinuationId() == m_processedSortCriterion->ContinuationId() )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found the Missing Diff: %s, in target cache dir: %s\n",
                                    (*orderedList.begin()).Id().c_str(), cacheLocation.c_str());
                        rv = true;
                    }
                }
                else
                {
                    if(isFirstFileForThisResyncSession(*orderedList.begin()))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found the missing first file: %s, in target cache dir: %s\n",
                                    (*orderedList.begin()).Id().c_str(), cacheLocation.c_str());
                        rv = true;
                    }
                }

                if (rv)
                {
                    // If missing diff (say MD) is found, check that missing diff (MD) has end
                    // time less than the current diff (CD). This is to ensure that upon next
                    // diff enumeration, MD gets picked up ahead of CD (as diffs get picked up
                    // in increasing order of end time) and replication is able to proceed.
                    // If it is otherwise, replication will get stuck indefinitely- so to avoid
                    // that, we will throw error so that replication gets marked for resync.
                    if ((*orderedList.begin()).EndTime() > m_sortCriterion->EndTime() ||
                        ((*orderedList.begin()).EndTime() == m_sortCriterion->EndTime() &&
                            (*orderedList.begin()).EndTimeSequenceNumber() > m_sortCriterion->EndTimeSequenceNumber()) ||
                        ((*orderedList.begin()).EndTime() == m_sortCriterion->EndTime() &&
                            (*orderedList.begin()).EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
                            (*orderedList.begin()).ContinuationId() > m_sortCriterion->ContinuationId()))
                    {
                        if (m_setResyncRequiredOnOOD)
                        {
                            std::stringstream msg;
                            const std::string& resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                            msg << "In-order file end time is greater than current out of order file end time. "
                                << "Last applied file: " << m_processedSortCriterion->Id() << std::endl
                                << "Current out of order file: " << m_sortCriterion->Id() << std::endl
                                << "In-order file found: " << (*orderedList.begin()).Id() << std::endl
                                << "Deleting current out of order file to avoid replication getting stuck in an infinite loop and "
                                << "marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                            ResyncReasonStamp resyncReasonStamp;
                            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                            m_resyncRequired = true;
                            if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                            {
                                break;
                            }
                            if (m_transport_protocol != TRANSPORT_PROTOOL_MEMORY &&
                                ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to delete the file %s. error no: %d\n",
                                    filePath.c_str(), ACE_OS::last_error());
                                break;
                            }
                        }
                        else
                        {
                            std::stringstream msg;
                            msg << "In-order file end time is greater than current out of order file end time. "
                                << "Last applied file: " << m_processedSortCriterion->Id() << std::endl
                                << "Current out of order file: " << m_sortCriterion->Id() << std::endl
                                << "In-order file found: " << (*orderedList.begin()).Id() << std::endl;
                            DebugPrintf(SV_LOG_ERROR, "Volume:%s %s", m_volumename.c_str(), msg.str().c_str());
                        }
                    }

                    // Missing diff has been found, stop further processing and exit routine.
                    break;
                }
            }

            if(m_processedDiffInfoAvailable)
            {
                DebugPrintf(SV_LOG_ERROR, "Note. This is not an error. Could not find the missing diff in target cache dir: %s, Processed diff is: %s\n",
                            cacheLocation.c_str(), m_processedSortCriterion->Id().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Note. This is not an error. Could not find the missing first file in target cache dir: %s\n",
                            cacheLocation.c_str());
            }

        }while(false);
    }
    // on exception, we will consider file is available in cache directory or PS
    // and let dataprotection exit and re-check on next invocation
    catch ( ContextualException& ce )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volumename.c_str());
    }
    catch( exception const& e )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volumename.c_str());
    }
    catch ( ... )
    {
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volumename.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isMissingDiffAvailableOnPS
 *
 * DESCRIPTION : This function lists the diffs in target GUID on PS, and finds if the expected(missing diff)
 *               diff is available there
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    volumeInfo - 
 *                    transportsettings - transport settings for this pair
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES : 
 *
 * return value : true if the missing file is found in target GUID on PS, false otherwise
 *
 */
bool CDPV2Writer::isMissingDiffAvailableOnPS(const std::string& filePath, 
                                             const TRANSPORT_CONNECTION_SETTINGS& transportsettings)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volumename.c_str());

    try
    {
        do
        {
            CxTransport::ptr cxTransport;
            try {
                cxTransport.reset(new CxTransport(m_transport_protocol,
                                                  transportsettings,
                                                  m_secure_mode,
                                                  m_cfsPsData.m_useCfs,
                                                  m_cfsPsData.m_psId));
            }
            catch (std::exception const& e) 
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: %s\n", FUNCTION_NAME, e.what());
                //Failed to contact PS so we are not sure if the missing diff file available on PS
                //We cannot return false(meaning file not available) unless we make sure the file is not available
                //So return true(i.e wait for some time and search for the same diff file on next run).
                rv = true;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                //Failed to contact PS so we are not sure if the missing diff file available on PS
                //We cannot return false(meaning file not available) unless we make sure the file is not available
                //So return true(i.e wait for some time and search for the same diff file on next run).
                rv = true;
                break;
            }

            std::string cxLocation = m_Configurator->getVxAgent().getDiffTargetSourceDirectoryPrefix() + "/";
            cxLocation += m_Configurator->getVxAgent().getHostId() + "/";
            cxLocation += GetVolumeDirName(m_volumename) + "/";
            cxLocation += m_Configurator->getVxAgent().getDiffTargetFilenamePrefix();

            FileInfos_t fileInfos;
            if (!cxTransport->listFile(cxLocation, fileInfos))
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "\n%s encountered error (%s) while fetching file list for %s\n", 
                            FUNCTION_NAME, cxTransport->status(), m_volumename.c_str());
                //Failed to contact PS so we are not sure if the missing diff file available on PS
                //We cannot return false(meaning file not available) unless we make sure the file is not available
                //So return true(i.e wait for some time and search for the same diff file on next run).
                rv = true;
                break;
            }

            DiffSortCriterion::OrderedEndTime_t orderedList;
            // prepare an ordered list
            FileInfos_t::iterator iter(fileInfos.begin());
            FileInfos_t::iterator iterEnd(fileInfos.end());
            for (/* empty */; iter != iterEnd; ++iter) {
                orderedList.insert(DiffSortCriterion((*iter).m_name.c_str(), m_volumename));
            }

            if(!orderedList.empty())
            {
                if(m_processedDiffInfoAvailable)
                {
                    if( (*orderedList.begin()).PreviousEndTime() == m_processedSortCriterion->EndTime() &&
                        (*orderedList.begin()).PreviousEndTimeSequenceNumber() == m_processedSortCriterion->EndTimeSequenceNumber() &&
                        (*orderedList.begin()).PreviousContinuationId() == m_processedSortCriterion->ContinuationId() )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found the Missing Diff: %s, in target GUID on PS: %s\n",
                                    (*orderedList.begin()).Id().c_str(), cxLocation.c_str());
                        rv = true;
                    }
                }
                else
                {
                    if(isFirstFileForThisResyncSession(*orderedList.begin()))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found the missing first file: %s, in target GUID on PS: %s\n",
                                    (*orderedList.begin()).Id().c_str(), cxLocation.c_str());
                        rv = true;
                    }
                }

                if (rv)
                {
                    // If missing diff (say MD) is found, check that missing diff (MD) has end
                    // time less than the current diff (CD). This is to ensure that upon next
                    // diff enumeration, MD gets picked up ahead of CD (as diffs get picked up
                    // in increasing order of end time) and replication is able to proceed.
                    // If it is otherwise, replication will get stuck indefinitely- so to avoid
                    // that, we will throw error so that replication gets marked for resync.
                    if ((*orderedList.begin()).EndTime() > m_sortCriterion->EndTime() ||
                        ((*orderedList.begin()).EndTime() == m_sortCriterion->EndTime() &&
                            (*orderedList.begin()).EndTimeSequenceNumber() > m_sortCriterion->EndTimeSequenceNumber()) ||
                        ((*orderedList.begin()).EndTime() == m_sortCriterion->EndTime() &&
                            (*orderedList.begin()).EndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
                            (*orderedList.begin()).ContinuationId() > m_sortCriterion->ContinuationId()))
                    {
                        if (m_setResyncRequiredOnOOD)
                        {
                            std::stringstream msg;
                            const std::string& resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                            msg << "In-order file end time is greater than current out of order file end time. "
                                << "Last applied file: " << m_processedSortCriterion->Id() << std::endl
                                << "Current out of order file: " << m_sortCriterion->Id() << std::endl
                                << "In-order file found: " << (*orderedList.begin()).Id() << std::endl
                                << "Deleting current out of order file to avoid replication getting stuck in an infinite loop and "
                                << "marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                            ResyncReasonStamp resyncReasonStamp;
                            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                            m_resyncRequired = true;
                            if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                            {
                                break;
                            }
                            if (m_transport_protocol != TRANSPORT_PROTOOL_MEMORY &&
                                ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to delete the file %s. error no: %d\n",
                                    filePath.c_str(), ACE_OS::last_error());
                                break;
                            }
                        }
                        else
                        {
                            std::stringstream msg;
                            msg << "In-order file end time is greater than current out of order file end time. "
                                << "Last applied file: " << m_processedSortCriterion->Id() << std::endl
                                << "Current out of order file: " << m_sortCriterion->Id() << std::endl
                                << "In-order file found: " << (*orderedList.begin()).Id() << std::endl;
                            DebugPrintf(SV_LOG_ERROR, "Volume:%s %s", m_volumename.c_str(), msg.str().c_str());
                        }
                    }

                    // Missing diff has been found, stop further processing and exit routine.
                    break;
                }
            }

            if(m_processedDiffInfoAvailable)
            {
                DebugPrintf(SV_LOG_ERROR, "Note. This is not an error. Could not find the missing diff in target GUID on PS: %s, Processed diff is: %s\n",
                            cxLocation.c_str(), m_processedSortCriterion->Id().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Note. This is not an error. Could not find the missing first file in target GUID on PS: %s\n",
                            cxLocation.c_str());
            }

        } while(0);
    } 
    catch ( ContextualException& ce )
    {
        //Failed to contact PS so we are not sure if the missing diff file available on PS
        //We cannot return false(meaning file not available) unless we make sure the file is not available
        //So return true(i.e wait for some time and search for the same diff file on next run).
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volumename.c_str());
    }
    catch( exception const& e )
    {
        //Failed to contact PS so we are not sure if the missing diff file available on PS
        //We cannot return false(meaning file not available) unless we make sure the file is not available
        //So return true(i.e wait for some time and search for the same diff file on next run).
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volumename.c_str());
    }
    catch ( ... )
    {
        //Failed to contact PS so we are not sure if the missing diff file available on PS
        //We cannot return false(meaning file not available) unless we make sure the file is not available
        //So return true(i.e wait for some time and search for the same diff file on next run).
        rv = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volumename.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volumename.c_str());
    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isSilentDataIntergityIssueDetected
 *
 * DESCRIPTION : This function checks if an improper file is in order wrt last processed diff
 *               and thus would have resulted in silent DI issue.
 *
 * INPUT PARAMETERS : filePath - path to the diff file being applied
 *                    transportsettings - transport settings for this pair
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if silent DI issue is detected, false otherwise
 *
 */
bool CDPV2Writer::isSilentDataIntergityIssueDetected(const std::string& filePath,
    const TRANSPORT_CONNECTION_SETTINGS& transportsettings)
{
    bool rv = false;

    try
    {
        do
        {
            std::string cacheLocation = dirname_r(filePath.c_str());

            const std::string lockPath = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A + "pending_diffs.lck";
            // PR#10815: Long Path support
            ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS, 0);
            ACE_Guard<ACE_File_Lock> guard(lock);

            if (!guard.locked())
            {
                DebugPrintf(SV_LOG_DEBUG,
                    "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                    FUNCTION_NAME, ACE_OS::last_error(),
                    ACE_OS::strerror(ACE_OS::last_error()), lockPath.c_str());

                // on exception, we will consider there is no DI issue
                rv = false;
                break;
            }

            CxTransport::ptr cxTransport;
            try {
                cxTransport.reset(new CxTransport(TRANSPORT_PROTOCOL_FILE,
                    transportsettings,
                    m_secure_mode,
                    m_cfsPsData.m_useCfs,
                    m_cfsPsData.m_psId));
            }
            // on exception, we will consider there is no DI issue
            catch (std::exception const& e)
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: %s\n", FUNCTION_NAME, e.what());
                rv = false;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                rv = false;
                break;
            }

            std::string fullLocation = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            fullLocation += m_Configurator->getVxAgent().getDiffTargetFilenamePrefix();
            FileInfos_t fileInfos;
            if (!cxTransport->listFile(fullLocation, fileInfos))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "FAILED %s cxTransport->GetFileList %s failed with error:%s\n",
                    FUNCTION_NAME, fullLocation.c_str(), cxTransport->status());
                rv = false;
                break;
            }


            DiffSortCriterion::OrderedEndTime_t orderedList;
            // prepare an ordered list
            FileInfos_t::iterator iter(fileInfos.begin());
            FileInfos_t::iterator iterEnd(fileInfos.end());
            for (/* empty */; iter != iterEnd; ++iter) {
                orderedList.insert(DiffSortCriterion((*iter).m_name.c_str(), m_volumename));
            }

            if (!orderedList.empty())
            {
                if (m_processedDiffInfoAvailable)
                {
                    if ((*orderedList.begin()).PreviousEndTime() == m_sortCriterion->EndTime() &&
                        (*orderedList.begin()).PreviousEndTimeSequenceNumber() == m_sortCriterion->EndTimeSequenceNumber() &&
                        (*orderedList.begin()).PreviousContinuationId() == m_sortCriterion->ContinuationId())
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found that next incoming file: %s is in order wrt current file: %s\n",
                            (*orderedList.begin()).Id().c_str(), m_sortCriterion->Id().c_str());
                        rv = true;
                    }
                }
                else
                {
                    if (isFirstFileForThisResyncSession(*orderedList.begin()))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found that next incoming file: %s is the first file for this resync session as hence it is in order.\n",
                            (*orderedList.begin()).Id().c_str());
                        rv = true;
                    }
                }
            }

            DebugPrintf(SV_LOG_ERROR, "Note. This is not an error. Found that next incoming file: %s is OOD wrt to current file: %s\n",
                cacheLocation.c_str(), m_sortCriterion->Id().c_str());

        } while (false);
    }
    // on exception, we will consider there is no DI issue
    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), m_volumename.c_str());
    }
    catch (exception const& e)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), m_volumename.c_str());
    }
    catch (...)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, m_volumename.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::isActionPendingOnVolume
 *
 * DESCRIPTION : check whether the volume undergoes rollback,rw/ro unhide state
 *
 * INPUT PARAMETERS : None
 *    
 *
 * OUTPUT PARAMETERS :
 *     
 * return value : true if pending action is there and false otherwise
 *
 */

bool CDPV2Writer::isActionPendingOnVolume()
{
    try
    {

        ACE_stat db_stat ={0};
        if((sv_stat(getLongPathName(m_rollback_filepath.c_str()).c_str(),&db_stat) == 0) 
           && ( (db_stat.st_mode & S_IFREG) == S_IFREG ))
        {
            return true;
        }
        if((sv_stat(getLongPathName(m_unhiderw_filepath.c_str()).c_str(),&db_stat) == 0) 
           && ( (db_stat.st_mode & S_IFREG) == S_IFREG ))
        {
            return true;
        }
        if((sv_stat(getLongPathName(m_unhidero_filepath.c_str()).c_str(),&db_stat) == 0) 
           && ( (db_stat.st_mode & S_IFREG) == S_IFREG ))
        {
            return true;
        }
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }
    return false;
}


// =========================================================
// differential reading routines:
//   canReadFileInToMemory
//   readdatatomemory
// =========================================================

/*
 * FUNCTION NAME : CDPV2Writer::canReadFileInToMemory()
 *
 * DESCRIPTION : figures out if memory is available
 *               to read the whole file into memory
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if resync data previously applied, else false
 *
 */
bool CDPV2Writer::canReadFileInToMemory(const std::string& filePath, const SV_ULONGLONG &filesize)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {

        rv = (filesize <= m_maxMemoryForDrtds);

    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::readdatatomemory
 *
 * DESCRIPTION : Reads the diff file from disk to memory
 *
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir.
 *                    
 * 
 * OUTPUT PARAMETERS : diff_data - starting address of the file in memory
 *                     diff_len - length of the file in memory in bytes.
 *
 * ALGORITHM : 
 *
 *     1.  Open the file and get the size of data.
 *     2.  Allocate a new buffer of length equal to file size
 *     3.  Read the file contents to this newly created buffer
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::readdatatomemory(const std::string & filePath, 
                                   char ** diff_data, SV_ULONG & diff_len,
                                   SV_ULONGLONG filesize)
{
    bool rv = true;
    SV_ULONG maxsizetoread = m_max_rw_size;
    ACE_HANDLE infile=ACE_INVALID_HANDLE;

    try
    {
        do
        {
            infile = ACE_OS::open(getLongPathName(filePath.c_str()).c_str(), O_RDONLY );
            if (infile == ACE_INVALID_HANDLE )
            {
                DebugPrintf(SV_LOG_ERROR, "%s unable to open %s.\n", FUNCTION_NAME, filePath.c_str());
                rv = false;
                break;
            }

            posix_fadvise_wrapper(infile,0,0, INM_POSIX_FADV_WILLNEED);

            SV_ULONG bytestoread = 0;
            SV_ULONG bytesread = 0;

            char * dst = new(nothrow) char [ filesize ] ;
            if(!dst)
            {
                DebugPrintf(SV_LOG_ERROR, "%s memory allocation for %s failed.\n", 
                            FUNCTION_NAME, filePath.c_str());
                rv = false;
                break;
            }

            while(filesize > 0)
            {
                bytestoread = filesize > maxsizetoread ? maxsizetoread : filesize;

                if((bytestoread = ACE_OS::read(infile,dst+bytesread,bytestoread)) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s read failed for file %s, Error %d.\n", 
                                FUNCTION_NAME, filePath.c_str(),ACE_OS::last_error());
                    rv = false;
                    break;
                }
                bytesread += bytestoread;
                filesize -= bytestoread;
            }

            if(rv)
            {
                *diff_data = dst;
                diff_len = bytesread;
            }
            else
            {
                delete[] dst;
            }

        } while (0);

        if (ACE_INVALID_HANDLE != infile)
            ACE_OS::close( infile);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    return rv;
}

// ===========================================================
//  uncompress routines:
//    isCompressedFile
//    InMemoryToInMemoryUncompress
//    uncompressInMemoryToDisk
//    uncompressOnDisk
// ===========================================================

/*
 * FUNCTION NAME : CDPV2Writer::isCompressedFile()
 *
 * DESCRIPTION : figures out if data is compressed
 *               
 *
 * INPUT PARAMETERS : full path name of the file
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if data is compressed, else false
 *
 */
bool CDPV2Writer::isCompressedFile(const std::string& filePath)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string::size_type gzpos = filePath.rfind(".gz");
    if (std::string::npos != gzpos && (filePath.length() - gzpos) <= 3)
    {
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::InMemoryToInMemoryUncompress
 *
 * DESCRIPTION : Uncompress the compressed file
 *
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 *                    diff_data - starting address of the compressed file
 *                                contents in memory
 *                    diff_len - length of the compressed file contents
 *                               in memory(in Bytes)
 *
 * OUTPUT PARAMETERS : pgunzip - starting address of unzipped file in memory
 *                     gunzip_len - unzipped length of the file in memory(in Bytes)
 *
 * ALGORITHM :
 * 
 *     1.  Uncompress the data 
 *
 * return value : true if successfull unzipped and false otherwise
 *
 */
bool CDPV2Writer::InMemoryToInMemoryUncompress(const std::string & filePath, 
                                               const char * diff_data,
                                               SV_ULONG& diff_len, 
                                               char ** pgunzip, 
                                               SV_ULONG &  gunzip_len)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        GzipUncompress uncompresser;
        if(!uncompresser.UnCompress(diff_data, diff_len, pgunzip, gunzip_len))
        {
            DebugPrintf(SV_LOG_ERROR, "uncompress for %s failed\n", filePath.c_str());
            rv = false;
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::uncompressInMemoryToDisk(std::string & filePath, 
                                           const char * diff_data,
                                           SV_ULONG& diff_len)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            GzipUncompress uncompresser;

            std::string uncompressedFileName = filePath;
            std::string::size_type idx = uncompressedFileName.rfind(".gz");
            uncompressedFileName.erase(idx, uncompressedFileName.length());

            if(!uncompresser.UnCompress(diff_data, diff_len, uncompressedFileName))
            {
                DebugPrintf(SV_LOG_ERROR, "uncompress for %s failed\n", filePath.c_str());
                //remove partially uncompressed file
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(uncompressedFileName.c_str()).c_str());

                rv = false;
                break;
            }

            if(ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR, "unlink %s failed. error %d\n", 
                            filePath.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            filePath = uncompressedFileName;
        }while(false);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


bool CDPV2Writer::uncompressOnDisk(std::string& filePath)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"Inside %s %s\n",FUNCTION_NAME, filePath.c_str());
    do
    {
        GzipUncompress uncompresser;
        std::string uncompressedFileName = filePath;

        std::string::size_type idx = filePath.rfind(".gz");
        if (std::string::npos == idx || (filePath.length() - idx) > 3) 
        {
            rv = true;
            break; // uncompress not needed;
        }

        uncompressedFileName.erase(idx, uncompressedFileName.length());
        if(!uncompresser.UnCompress(filePath,uncompressedFileName))
        {
            std::ostringstream msg;
            msg << "Uncompress failed for "
                << filePath
                << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());

            // remove partial uncompressed file
            ACE_OS::unlink(getLongPathName(uncompressedFileName.c_str()).c_str());
            rv = false;
            break;
		}
		else
        {
            ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str());
            filePath.erase(idx, filePath.length());
            rv = true;
            break;
        }

    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// ===============================================================
//  exception handling:
// 
//  1) compressed file corruption
//     renameCorruptedFileAndSetResyncRequired
//     persistResyncRequired
// ===============================================================

/*
 * FUNCTION NAME :  CDPV2Writer::renameCorruptedFileAndSetResyncRequired
 *
 * DESCRIPTION : persists the resync required information locally, sets
 *               the resync required at CX and renames the corrupted file
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTE :
 * 
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::renameCorruptedFileAndSetResyncRequired(const std::string& filePath)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            std::string fileName = basename_r(filePath.c_str());

            if( m_transport_protocol != TRANSPORT_PROTOOL_MEMORY )
            {
                std::string corruptedFilePath = dirname_r(filePath.c_str()) + ACE_DIRECTORY_SEPARATOR_CHAR_A
                    + "corrupted_" + fileName;

                // PR#10815: Long Path support
                if(ACE_OS::rename(getLongPathName(filePath.c_str()).c_str(),
                                  getLongPathName(corruptedFilePath.c_str()).c_str()) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to rename %s to %s. error no: %d\n",
                                filePath.c_str(), corruptedFilePath.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }
            }

            //update previous diff information before deleting 
            if(!update_previous_diff_information(fileName, m_lastFileApplied.previousDiff.SVDVersion))
            {
                rv = false;
                break;
            }

            std::stringstream msg;
            const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
            msg << "Marking resync: either uncompress or parse of file " << filePath << " for the target volume "
                << m_volumename << " failed." ;

            if(m_transport_protocol != TRANSPORT_PROTOOL_MEMORY)
            {
                msg << " A copy of this file is stored in target cache directory and resync required flag is set for " 
                    << m_volumename << std::endl;
            }
            msg << " with resyncReasonCode=" << resyncReasonCode;

            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
            ResyncReasonStamp resyncReasonStamp;
            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
            m_resyncRequired = true;
            if(!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
            {
                rv = false;
                break;
            }

        }while(false);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



// ============================================================
//  Differential parsing routines
//    parseInMemoryFile
//    parseOnDiskFile
// ============================================================

/*
 * FUNCTION NAME :  CDPV2Writer::parseInMemoryFile
 *
 * DESCRIPTION : Parse the in memory diff file and get the metadata from it.
 *
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 *                    diff_data - starting address of the uncompressed file
 *                                contents in memory
 *                    diff_len - length of the uncompressed file contents in
 *                               memory(in Bytes)
 * 
 * OUTPUT PARAMETERS : metadata - will be filled with the metadata corresponding
 *                                to the diff file 'filePath'
 *
 * ALGORITHM :
 * 
 *     1. Parse the inmemory diff data and extracts the metadata
 *
 * return value : true if successfully parsed and false otherwise
 *
 */
bool CDPV2Writer::parseInMemoryFile(const std::string & filePath, const char * diff_data, 
                                    const SV_ULONG & diff_len, DiffPtr & metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            InMemoryDiffFile inmemoryparser(filePath, diff_data, diff_len);
            inmemoryparser.SetBaseline(m_baseline);
            inmemoryparser.SetSourceVolumeSize(m_srccapacity);
            if(!inmemoryparser.ParseDiff(metadata))
            {
                rv = false;
                break;
            }
        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


bool CDPV2Writer::parseOnDiskFile(const std::string & filePath, DiffPtr & metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            DifferentialFile onDiskParser(filePath);
            onDiskParser.SetBaseline(m_baseline);
            onDiskParser.SetSourceVolumeSize(m_srccapacity);

            if( SV_SUCCESS != onDiskParser.Open(BasicIo::BioReadExisting |
                                                BasicIo::BioShareRW | BasicIo::BioBinary) )
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to open file %s with error %ul\n",
                    FUNCTION_NAME, filePath.c_str(), onDiskParser.LastError());
                rv = false;
                break;
            }

            if(!onDiskParser.ParseDiff(metadata))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to parse file %s with error %ul\n",
                    FUNCTION_NAME, filePath.c_str(), onDiskParser.LastError());
                rv = false;
                break;
            }
        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

// =========================================================
//  exception and validation routines:
//  
//  1) files from current resync session
//        isDiffFromCurrentResyncSession
//   
//  2) resync files
//        isResyncDataAlreadyApplied
//  
//  3) validate if the file can be applied
//         persistSourceSystemCrashEvent
//         isFirstFileForThisResyncSession
//         isSourceSystemCrashEventSet
// =========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::isDiffFromCurrentResyncSession
 *
 * DESCRIPTION : checks if the differential is from the same resync session.
 *               Source agent updates the resync start timestamp to CX.
 *               Target receives it through VolumeInfo object and uses it
 *               for finding if this diff is generated after current resync
 *               start timestamp
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 *                    volInfo - contains the resync start timestamp sent
 *                              by source to CX
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTE :
 * 
 * return value : true if the diff's end time stamp is > the resync start time stamp
 *                otherwise returns false
 */
bool CDPV2Writer::isDiffFromCurrentResyncSession(const std::string& filePath)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            SV_ULONGLONG diffEndTime = m_sortCriterion->EndTime();
            SV_ULONGLONG diffEndTimeSeq = m_sortCriterion->EndTimeSequenceNumber();

            SV_ULONGLONG  resyncStartTime    = m_Configurator->getResyncStartTimeStamp(m_volumename);//volInfo -> QuasiStartTimeStamp();
            SV_ULONG      resyncStartTimeSeq = m_Configurator->getResyncStartTimeStampSeq(m_volumename);//volInfo -> QuasiStartTimeStampSeq();

            // do not apply files which bear timestamp earlier than
            // the resync start time. we just delete them
            if( (diffEndTime < resyncStartTime) ||
                ((diffEndTime == resyncStartTime) && (diffEndTimeSeq <  resyncStartTimeSeq)) )
            {
                std::stringstream msg;
                msg << "Ignoring differential file " << filePath << " with timestamp less than"
                    << " resync start time(TS:" << resyncStartTime << " Seq:" << resyncStartTimeSeq << ").\n";
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                rv = false;
                break;
            }
        }while(false);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::isResyncDataAlreadyApplied()
 *
 * DESCRIPTION : figures out if current resync chunk 
 *               is already applied
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : true if resync data previously applied, else false
 *
 */
bool CDPV2Writer::isResyncDataAlreadyApplied(SV_ULONGLONG* offset, 
                                             SharedBitMap*bm, 
                                             DiffPtr change_metadata)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /*
     *  In Resync phase the info about the chunks for which 
     *  data has been applied will be written to a bitmap file
     *  This code block will be bypassed in diffsync mode...
     *  By Suresh
     */
    if(offset != NULL )
    {
        cdp_drtdv2s_iter_t drtdsBegin = change_metadata->DrtdsBegin() ;
        cdp_drtdv2s_iter_t drtdsIterEnd = change_metadata->DrtdsEnd();
        if( drtdsBegin != drtdsIterEnd )
        {
            cdp_drtdv2_t drtd = *drtdsBegin;				
            *offset = drtd.get_volume_offset();

        }
        if( bm != NULL )
        {
            int chunkIndex = *offset / m_resync_chunk_size;
            if( bm->isBitSet( chunkIndex * 2 ) && bm->isBitSet( (chunkIndex *  2) + 1) )
            {
                DebugPrintf(SV_LOG_DEBUG, "The Sync data already applied for chunk %d\n", chunkIndex);
                rv = true;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "About to apply sync data starting at offset %llu.\n", *offset);
            }
        }
    }
    /* End of Change */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME : CDPV2Writer::persistSourceSystemCrashEvent()
 *
 * DESCRIPTION : save the crash event filename in a local file named 'sourceSystemCrashed' in the
 *               AppliedInfo directory
 *
 * INPUT PARAMETERS : filePath - name of the current diff being applied(nothing but the crash event
 *                               diff
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if sucessfully persisted the crash event, else false
 */
bool CDPV2Writer::persistSourceSystemCrashEvent(const std::string& filePath)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        std::string sourceSystemCrashEventFilePath = m_appliedInfoDir;
        sourceSystemCrashEventFilePath += ACE_DIRECTORY_SEPARATOR_STR_A;
        sourceSystemCrashEventFilePath += "sourceSystemCrashed";

        // PR#10815: Long Path support
        std::ofstream crashEventFile(getLongPathName(sourceSystemCrashEventFilePath.c_str()).c_str());

        if(!crashEventFile) 
        {
            std::stringstream msg;
            msg << "Failed to create source crash file " << sourceSystemCrashEventFilePath << " for volume "
                << m_volumename << ". error no: " << ACE_OS::last_error() << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
        }
        else
        {
            crashEventFile << basename_r(filePath.c_str());
            crashEventFile.close();
            rv = true;
        }
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME : CDPV2Writer::isFirstFileForThisResyncSession()
 *
 * DESCRIPTION : First file for the resync session contains
 *               previous end time < resync start time <= current end time.
 *
 * INPUT PARAMETERS : sortCriterion - the Diff which has to be checked with the resync start time
 *                    volumeInfo - helps in finding the resync start time for the target volume
 * 
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if previous end time < resync start time <= current end time, else false
 */
bool CDPV2Writer::isFirstFileForThisResyncSession(const DiffSortCriterion& sortCriterion)
{
    SV_ULONGLONG resyncStartTimeStamp = m_Configurator->getResyncStartTimeStamp(m_volumename);
    SV_ULONGLONG resyncEndTimeStamp   = m_Configurator->getResyncEndTimeStamp(m_volumename);
    SV_ULONG resyncStartTimeStampSeq  = m_Configurator->getResyncStartTimeStampSeq(m_volumename);

    if(!m_processedDiffInfoAvailable && resyncEndTimeStamp == 0 && resyncStartTimeStampSeq == 0)
    {
        // May be the pair is forcefully moved to diffsync
        // This is a special case where we can not determine if this is a first file
        // Just return true
        DebugPrintf(SV_LOG_ERROR, "Note: this is not an error: Resync start time is 0 for the pair with target"
                    " volume %s treating the current diff as the first file.\nCurrent diff: %s\n", m_volumename.c_str(),
                    sortCriterion.Id().c_str());
        return true;
    }
    else
    {
        // If the resync start time falls inbetween the previous end time and the current end time return true
        return(
            (sortCriterion.PreviousEndTime() < resyncStartTimeStamp ||
             (sortCriterion.PreviousEndTime() == resyncStartTimeStamp &&
              sortCriterion.PreviousEndTimeSequenceNumber() < resyncStartTimeStampSeq)
             )
            &&
            (sortCriterion.EndTime() > resyncStartTimeStamp ||
             (sortCriterion.EndTime() == resyncStartTimeStamp &&
              sortCriterion.EndTimeSequenceNumber() >= resyncStartTimeStampSeq)
             )
               );
    }

    return false;
}



bool CDPV2Writer::isSourceSystemCrashEventSet(std::string& crashEventDiffFileName)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        std::string sourceSystemCrashEventFilePath = m_appliedInfoDir;
        sourceSystemCrashEventFilePath += ACE_DIRECTORY_SEPARATOR_STR_A;
        sourceSystemCrashEventFilePath += "sourceSystemCrashed";

        ACE_stat file_stat ={0};
        // PR#10815: Long Path support
        if(sv_stat(getLongPathName(sourceSystemCrashEventFilePath.c_str()).c_str(),&file_stat) == 0)
        {
            // PR#10815: Long Path support
            std::ifstream crashEventFile(getLongPathName(sourceSystemCrashEventFilePath.c_str()).c_str());
            if(!crashEventFile)
            {
                std::stringstream msg;
                msg << "Failed to open crash event file " << sourceSystemCrashEventFilePath
                    << " for reading for volume " << m_volumename << ". error no: "
                    << ACE_OS::last_error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            }
            else
            {
                std::stringstream fileName;
                fileName << crashEventFile.rdbuf();
                crashEventFile.close();
                crashEventDiffFileName = fileName.str();
                if(crashEventDiffFileName != "")
                {
                    rv = true;
                }
            }
        }
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



// =========================================================
//  cdpv1 routines
//   
//  1) partial transaction exists? 
//      isPartiallyApplied
// 
//  2) previous (partial) transaction
//       get_partial_transaction_information
//       update_partial_transaction_information
//
//  3) 
//     update_end_timestamp_information

// =========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::isPartiallyApplied
 *
 * DESCRIPTION : Checks if any partial transaction exists
 *
 * INPUT PARAMETERS : diffFileName - diff name that is going to be applied
 *                    lastCDPtxn - last retention transaction information
 *                    
 * OUTPUT PARAMETERS : 
 *             partiallyApplied     - true if this file is partially applied 
 *
 * NOTE :  
 * 
 * return value : true if this file is partially applied false otherwise
 */
bool CDPV2Writer::isPartiallyApplied(const std::string& diffFileName, const CDPLastRetentionUpdate& lastCDPtxn,
                                     bool& partiallyApplied)
{
    bool rv = true;

    do
    {
        if(std::string(m_lastCDPtxn.partialTransaction.diffFileApplied) != "none")
        {
            if( std::string(lastCDPtxn.partialTransaction.diffFileApplied) == diffFileName)
            {
                partiallyApplied = true;
                break;
            }
            else
            {
                // Got a different diff file than the partially applied diff
                // This should not happen anytime.
                if(m_setResyncRequiredOnOOD)
                {
                    std::stringstream msg;
                    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                    msg << "Internal logic error - Recieved new file before completly processing the previous file."
                        << " Incoming file: " << diffFileName
                        << ", Previous file: " << lastCDPtxn.partialTransaction.diffFileApplied
                        << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                    ResyncReasonStamp resyncReasonStamp;
                    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                    m_resyncRequired = true;
                    if(!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
                    {
                        rv = false;
                        break;
                    }

                    //clear the partial transaction info and continue applying
                    std::string none = "none";
                    memset(&m_lastCDPtxn, 0, sizeof(m_lastCDPtxn.partialTransaction));
                    inm_memcpy_s(m_lastCDPtxn.partialTransaction.diffFileApplied, sizeof(m_lastCDPtxn.partialTransaction.diffFileApplied), none.c_str(), none.length() + 1);
                    if(!CDPUtil::update_last_retentiontxn(m_cdpdir, m_lastCDPtxn))
                    {
                        rv = false;
                        break;
                    }

                    //we have set resync required & also cleared the partail transaction info,
                    //so we can apply current differential
                    partiallyApplied = false;
                    break;
                }
                else
                {
                    std::stringstream msg;
                    msg << "Internal logic error. Diff file " << lastCDPtxn.partialTransaction.diffFileApplied
                        << " is partially applied. And got a new diff " << diffFileName
                        << "\nCannot apply new diff unless previous partial transaction is completed." << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "Volume:%s %s", m_volumename.c_str(),msg.str().c_str());
                    rv = false;
                    break;
                }
            }
        }
        else
        {
            partiallyApplied = false;
            break;
        }
    }while(false);

    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::get_partial_transaction_information
 *
 * DESCRIPTION : gets the partially transaction from applied information structure
 *
 * INPUT PARAMETERS : none
 *                    
 * OUTPUT PARAMETERS : 
 *             fileapplied        - incomming diff file name
 *             changesapplied     - no of changes applied in last run
 *
 * NOTE :  Partial transaction information is maintained in a file on the disk.
 *         This information should remain as is untill the specific thread fully
 *         completes the transaction. For this reason 'partial transaction information'
 *         cannot be shared among threads.
 *
 *         In resync no differential is partially applied. If in case we fail to apply
 *         the differential completely it will be rolled back on the spot.
 *
 *         In diff sync partial applies are possible.
 *
 *     So,
 *         1.) In diffsync this function gets the partial transaction details.
 *         2.) And in resync where multiple threads call this function it always returns
 *             default values.
 * 
 * return value : true if successfull and false otherwise
 */
bool CDPV2Writer::get_partial_transaction_information(std::string& fileapplied,
                                                      SV_UINT& changesapplied)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    fileapplied = "none";
    changesapplied = 0;

    try
    {
        do
        {
            if(isInDiffSync())
            {
                fileapplied = m_lastCDPtxn.partialTransaction.diffFileApplied;
                changesapplied = m_lastCDPtxn.partialTransaction.noOfChangesApplied;
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::update_partial_transaction_information
 *
 * DESCRIPTION : updates the partial transaction details in applied information struct
 *               and writes it to the disk
 *
 * INPUT PARAMETERS : 
 *             fileapplied        - incomming partially applied diff file name
 *             changesapplied     - no of changes applied in this run
 *                    
 * OUTPUT PARAMETERS : none
 *
 * NOTE :  Partial transaction information is maintained in a file on the disk.
 *         This information should remain as is untill the specific thread fully
 *         completes the transaction. For this reason 'partial transaction information'
 *         cannot be shared among threads.
 *
 *         In resync no differential is partially applied. If in case we fail to apply
 *         the differential completely it will be rolled back on the spot.
 *
 *         In diff sync partial applies are possible.
 *
 *     So,
 *         1.) In diffsync this function updates the partial transaction details.
 *         2.) And in resync it does nothing.
 * 
 * return value : true if successfull and false otherwise
 */
bool CDPV2Writer::update_partial_transaction_information(const std::string& fileapplied,
                                                         const SV_UINT& changesapplied)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(isInDiffSync())
            {
                inm_memcpy_s(m_lastCDPtxn.partialTransaction.diffFileApplied, sizeof(m_lastCDPtxn.partialTransaction.diffFileApplied), fileapplied.c_str(), fileapplied.length() + 1);
                m_lastCDPtxn.partialTransaction.noOfChangesApplied = changesapplied;

                rv = CDPUtil::update_last_retentiontxn(m_cdpdir, m_lastCDPtxn);
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
        DebugPrintf(SV_LOG_ERROR,
                    "diff being applied: %s\n",
                    fileapplied.c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
        DebugPrintf(SV_LOG_ERROR,
                    "diff being applied: %s\n",
                    fileapplied.c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
        DebugPrintf(SV_LOG_ERROR,
                    "diff being applied: %s\n",
                    fileapplied.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::update_end_timestamp_information
 *
 * DESCRIPTION : update the end time stamp/seq no. in applied information structure
 *               and on the disk
 *
 * INPUT PARAMETERS : 
 *             sequenceNumber - latest seq no. from the retention store
 *             endTimeStamp   - latest time stamp from the retention store
 *             continuationId - continuationId
 *                    
 * OUTPUT PARAMETERS : none
 *
 * NOTE :  
 * 
 * return value : true if successfull and false otherwise
 */
bool CDPV2Writer::update_end_timestamp_information(const SV_ULONGLONG& sequenceNumber,
                                                   const SV_ULONGLONG& endTimeStamp,
                                                   const SV_UINT& continuationId)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(isInDiffSync())
            {
                m_lastCDPtxn.retEndTimeStamp.SequenceNumber = sequenceNumber;
                m_lastCDPtxn.retEndTimeStamp.TimeInHundNanoSecondsFromJan1601 = endTimeStamp;
                m_lastCDPtxn.retEndTimeStamp.ContinuationId = continuationId;

                //rv = CDPUtil::update_last_retentiontxn(m_cdpdir, m_lastCDPtxn);
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



/*
 * FUNCTION NAME :  CDPV2Writer::update_previous_diff_information
 *
 * DESCRIPTION : updates the previously diff details in the applied info struct
 *               and also on the disk
 *
 * INPUT PARAMETERS : 
 *             filename - previous diff filename
 *             version  - if the prev diff contains per io changes
 *                    
 * OUTPUT PARAMETERS : none
 *
 * NOTE :  
 * 
 * return value : true if successfull and false otherwise
 */
bool CDPV2Writer::update_previous_diff_information(const std::string& filename,
                                                   const SV_UINT& version)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(isInDiffSync())
            {
                inm_memcpy_s(m_lastFileApplied.previousDiff.filename, sizeof(m_lastFileApplied.previousDiff.filename), filename.c_str(), filename.length() + 1);
                m_lastFileApplied.previousDiff.SVDVersion = version;

                rv = CDPUtil::update_last_fileapplied_information(m_volumename, m_lastFileApplied);
            }

        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


// ============================================
//  timestamp order validation routines
// ============================================

bool CDPV2Writer::verify_file_version(const std::string &diffname,DiffPtr change_metadata)
{

    bool version_ok = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s\n",
		FUNCTION_NAME, diffname.c_str());

    do
    {
        if(!isInDiffSync())
        {
            DebugPrintf(SV_LOG_DEBUG, "ordering checks are not applicable in resync mode.\n");
            version_ok = true;
            break;
        }

        if( !m_processedDiffInfoAvailable )
        {
            // We do not have previous diff information we need not proceed with
            // other checks. Just return true
            version_ok = true;
            break;
        }

        if(change_metadata -> version() < m_lastFileApplied.previousDiff.SVDVersion)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "Incoming diff : %s version : %d is less than previous diff: %s version %d.\n",
                        diffname.c_str(), 
                        change_metadata -> version(), 
                        m_lastFileApplied.previousDiff.filename, 
                        m_lastFileApplied.previousDiff.SVDVersion);
            version_ok = false;
            break;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s\n",
		FUNCTION_NAME, diffname.c_str());

    return version_ok;
}


bool CDPV2Writer::verify_increasing_timestamp(const std::string & diffname,DiffPtr change_metadata)
{
    //
    // note: do not add try/catch in this routine
    //

    bool in_order = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s\n",
		FUNCTION_NAME, diffname.c_str());

    do
    {
        if(!isInDiffSync())
        {
            DebugPrintf(SV_LOG_DEBUG, "ordering checks are not applicable in resync mode.\n");
            in_order = true;
            break;
        }

        if( !m_processedDiffInfoAvailable )
        {
            // We do not have previous diff information we need not proceed with
            // other checks. Just return true
            in_order = true;
            break;
        }

        // 
        // validations:
        // on continuation file ie
        //       start time == start time of previous diff
        //       end time == end time of previous diff
        //       start seq == start seq of previous diff
        //      end seq == end seq of previous diff
        //  continuation id > continuation id of previous diff
        //
        // on non continuation file:
        //    start time of incoming diff should be greater than or equal to end time of previos diff 
        //
        // if last inode is per i/o 
        //   start sequence should be greater than end sequence of previous diff
        //

        // 
        //  checks:
        //  1. decreasing continuation id
        //  2. decreasing timestamp
        //  3. decreasing sequence

        // checks for continuation file
        if ( (change_metadata -> actualEndTime() == m_processedSortCriterion->EndTime())
             && (change_metadata -> EndTimeSequenceNumber() == m_processedSortCriterion->EndTimeSequenceNumber()))
        {
            if(change_metadata -> ContinuationId() < m_processedSortCriterion->ContinuationId())
            {
                in_order = false;
			}
			else
            {
                in_order = true;
            }
            break;
        }


        // checks for non continuation files
        if( change_metadata -> actualStartTime() < m_processedSortCriterion->EndTime() )
        {
            in_order = false;
            break;
        }

        if(m_lastFileApplied.previousDiff.SVDVersion >= SVD_PERIOVERSION)
        {
            // check for increasing sequence no only for non-continuation file with global sequence numbers
            if( change_metadata -> StartTimeSequenceNumber() <= m_processedSortCriterion->EndTimeSequenceNumber() )
            {
                in_order = false;
                break;
            }
        }

        in_order = true;

    } while (0);

    if(!in_order)
    {
        OutOfOrderDiffAlert a(m_volumename, m_lastFileApplied.previousDiff.filename,
                              m_processedSortCriterion->EndTime(), m_processedSortCriterion->EndTimeSequenceNumber(),
                              m_processedSortCriterion->ContinuationId(), diffname,
                              change_metadata -> actualStartTime(), change_metadata -> StartTimeSequenceNumber(),
                              change_metadata -> actualEndTime(), change_metadata -> EndTimeSequenceNumber(),
                              change_metadata -> ContinuationId());
        std::stringstream msg;
        const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
        msg << a.GetMessage() << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
        m_resyncRequired = true;
        CDPUtil::store_and_sendcs_resync_required(m_volumename, a.GetMessage(), resyncReasonStamp);
        SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s\n",
		FUNCTION_NAME, diffname.c_str());

    return in_order;
}

bool CDPV2Writer::adjust_timestamp(DiffPtr change_metadata, const cdpv3transaction_t &txn)
{
    bool rv = true;
    bool adjust_start_ts = false;
    bool adjust_end_ts = false;
    bool adjust_start_seq = false;
    bool adjust_end_seq = false;
    bool adjust_required = false;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if(!isInDiffSync())
    {
        adjust_required = true;
        adjust_start_ts = true;
        adjust_end_ts = true;
        adjust_start_seq = true;
        adjust_end_seq = true;
    }

    if(change_metadata -> starttime() < txn.m_ts.m_ts)
    {
        adjust_required = true;
        adjust_start_ts = true;
    }

    if(change_metadata -> endtime() < txn.m_ts.m_ts)
    {
        adjust_required = true;
        adjust_end_ts = true;
    }


    if((change_metadata -> version() >= SVD_PERIOVERSION) &&
       (change_metadata-> StartTimeSequenceNumber() < txn.m_ts.m_seq))
    {
        adjust_required = true;
        adjust_start_seq = true;
    }

    if((change_metadata -> version() >= SVD_PERIOVERSION) &&
       (change_metadata-> EndTimeSequenceNumber() < txn.m_ts.m_seq))
    {
        adjust_required = true;
        adjust_end_seq = true;
    }

    if(adjust_required)
    {
        DebugPrintf(SV_LOG_DEBUG, 
                    "adjusting timestamp/seq to ensure increasing timestamp/seq in retention store.\n");

        if(adjust_start_ts)
        {
            change_metadata-> StartTime(txn.m_ts.m_ts);
        }

        if(adjust_end_ts)
        {
            change_metadata-> EndTime(txn.m_ts.m_ts);
        }

        if(adjust_start_seq)
        {
            change_metadata-> StartTimeSequenceNumber(txn.m_ts.m_seq);
        }

        if(adjust_end_seq)
        {
            change_metadata-> EndTimeSequenceNumber(txn.m_ts.m_seq);
        }

        // Following lines have been commented to address PR# 21981
        //if(isInDiffSync())
        //    change_metadata -> type(OUTOFORDER_TS);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::adjust_timestamp(DiffPtr change_metadata, const CDPLastRetentionUpdate& lastCDPtxn)
{
    bool rv = true;
    bool adjust_start_ts = false;
    bool adjust_end_ts = false;
    bool adjust_start_seq = false;
    bool adjust_end_seq = false;
    bool adjust_required = false;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if(!isInDiffSync())
    {
        adjust_required = true;
        adjust_start_ts = true;
        adjust_end_ts = true;
        adjust_start_seq = true;
        adjust_end_seq = true;
    }

    if(change_metadata -> starttime() < lastCDPtxn.retEndTimeStamp.TimeInHundNanoSecondsFromJan1601)
    {
        adjust_required = true;
        adjust_start_ts = true;
    }

    if(change_metadata -> endtime() < lastCDPtxn.retEndTimeStamp.TimeInHundNanoSecondsFromJan1601)
    {
        adjust_required = true;
        adjust_end_ts = true;
    }


    if((change_metadata -> version() >= SVD_PERIOVERSION) &&
       (change_metadata-> StartTimeSequenceNumber() < lastCDPtxn.retEndTimeStamp.SequenceNumber))
    {
        adjust_required = true;
        adjust_start_seq = true;
    }

    if((change_metadata -> version() >= SVD_PERIOVERSION) &&
       (change_metadata-> EndTimeSequenceNumber() < lastCDPtxn.retEndTimeStamp.SequenceNumber))
    {
        adjust_required = true;
        adjust_end_seq = true;
    }

    if(adjust_required)
    {
        DebugPrintf(SV_LOG_DEBUG, 
                    "adjusting timestamp/seq to ensure increasing timestamp/seq in retention store.\n");

        if(adjust_start_ts)
        {
            change_metadata-> StartTime(lastCDPtxn.retEndTimeStamp.TimeInHundNanoSecondsFromJan1601);
        }

        if(adjust_end_ts)
        {
            change_metadata-> EndTime(lastCDPtxn.retEndTimeStamp.TimeInHundNanoSecondsFromJan1601);
        }

        if(adjust_start_seq)
        {
            change_metadata-> StartTimeSequenceNumber(lastCDPtxn.retEndTimeStamp.SequenceNumber);
        }

        if(adjust_end_seq)
        {
            change_metadata-> EndTimeSequenceNumber(lastCDPtxn.retEndTimeStamp.SequenceNumber);
        }

        // Following lines have been commented to address PR# 21981
        //if(isInDiffSync())
        //    change_metadata -> type(OUTOFORDER_TS);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

// ==========================================================
//   snapshot routine
//     process_event_snapshots
// ==========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::process_event_snapshots
 *
 * DESCRIPTION : Process event based snapshots 
 *
 *
 * INPUT PARAMETERS : diffSync - Pointer to DifferentialSync instance
 *                    volumeInfo - pointer to target VolumeInfo
 *                    change_metadata - metadata of the diff file we are processing
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *    1.  if this differentail contains any tags then process any event based snapshot
 *        requests for these tags
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::process_event_snapshots(DiffPtr change_metadata)
{
    bool rv = true;


    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 

    try
    {
        do
        {

            if(!change_metadata -> NumUserTags())
            {
                rv = true;
                break;
            }

            svector_t bookmarks;

            UserTagsIterator uiter = change_metadata -> UserTagsBegin();
            UserTagsIterator uend = change_metadata -> UserTagsEnd();

            for ( /* empty */ ; uiter != uend; ++uiter)	
            {
                SvMarkerPtr usertag = *uiter;
                std::string bookmark = usertag -> Tag().get();
                bookmarks.push_back(bookmark);
            }

            if(!m_snapshotrequestprocessor_ptr->processSnapshotRequest(m_volumename,bookmarks))
            {
                rv = false;
                break;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// ===========================================================
//  replication state change from resync step 1 to step 2
//     update_quasi_state
// ===========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::update_quasi_state
 *
 * DESCRIPTION : Sends a request to CX to change the state of this pair
 *               from quasi state(Resync II) to diffsync
 *
 * INPUT PARAMETERS : volumeInfo - pointer to VolumeInfo for which the state
 *                                 change is to be requested
 *                    change_metadata - metadata of the diff file being
 *                                      processed
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  If the differential start time > quasi end time
 *         send state change request to CX(from ResyncII to DiffSync)
 *
 * return value : false if state change request failed, true otherwise
 *
 */
bool CDPV2Writer::update_quasi_state(DiffPtr change_metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 

    try
    {
        do
        {
            if(m_inquasi_state)
            {
                SV_ULONGLONG diffStartTime = change_metadata ->actualStartTime();
                SV_ULONGLONG  diffStartTimeSeq = change_metadata ->StartTimeSequenceNumber();

                SV_ULONGLONG  quasiEndTime    = m_Configurator->getResyncEndTimeStamp(m_volumename);//volumeInfo -> QuasiEndTimeStamp();
                SV_ULONG      quasiEndTimeSeq = m_Configurator->getResyncEndTimeStampSeq(m_volumename);//volumeInfo -> QuasiEndTimeStampSeq();

                if( (diffStartTime > quasiEndTime) ||  
                    ((diffStartTime == quasiEndTime) && (diffStartTimeSeq > quasiEndTimeSeq)) 
                    )
                {
                    if(!EndQuasiState())
                    {
                        rv = false;
                        break;
                    }
                }
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::check_and_setresync_on_cs_ifrequired
 *
 * DESCRIPTION : Calls SetResync API on CS if resync flag is set in local settings but missing on CS.
 *
 * INPUT PARAMETERS : none
 *
 * OUTPUT PARAMETERS : none
 *
 *
 * return value : True if the routine completed successfully, false otherwise.
 *
 */
bool CDPV2Writer::check_and_setresync_on_cs_ifrequired()
{
	bool rv = true;
	bool resyncSetInLocalSettings = false;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	do
	{
		if (!isInDiffSync())
		{
			DebugPrintf(
				SV_LOG_DEBUG, 
				"Replication for %s is not in diffsync state, will not check for resync flag mismatch.\n",
				m_volumename.c_str());

			rv = true;
			break;
		}

		std::string resyncRequiredFilePath = m_appliedInfoDir;
		resyncRequiredFilePath += ACE_DIRECTORY_SEPARATOR_STR_A;
		resyncRequiredFilePath += "resyncRequired";
		ACE_stat file_stat = { 0 };
		// PR#10815: Long Path support
		if (sv_stat(getLongPathName(resyncRequiredFilePath.c_str()).c_str(), &file_stat) == 0)
		{
			resyncSetInLocalSettings = true;
		}
		else
		{
			resyncSetInLocalSettings = false;
		}

		// If resync flag is set in local settings but CS settings does not have it,
		// this could be due to some bug, retry set resync required API on CS.
		if (resyncSetInLocalSettings && !m_Configurator->isResyncRequiredFlag(m_volumename))
		{
            const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
            std::stringstream msg("Resync required flag is set on MT, but is not set in settings received from CS.");
			msg << " Retrying setting resync flag on CS for device : " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
            ResyncReasonStamp resyncReasonStamp;
            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

			if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp))
			{
				rv = false;
				break;
			}
		}

	} while (0);

	return rv;
}

// ============================================================
//  routines to determine recovery accuracy
//     set_accuracy_mode
// ============================================================

/*
 * FUNCTION NAME :  CDPV2Writer::set_accuracy_mode
 *
 * DESCRIPTION : Set the accuracy mode for the changes to be applied
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 *                    volumeInfo - pointer to VolumeInfo for which we apply changes
 *                    change_metadata - metadata of the diff file we are processing
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *    1. Based on the file name the accuracy mode is set 
 *
 * return value : true if accuracy mode is set and false otherwise
 *
 */
bool CDPV2Writer::set_accuracy_mode(const std::string & filePath,
                                    DiffPtr change_metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); 

    try
    {
        do
        {
            // If resync Required due to targetvolume unhide readwrite
            // recovery accuracy should be set as not gauranteed
            std::string resyncRequiredFilePath = m_appliedInfoDir;
            resyncRequiredFilePath += ACE_DIRECTORY_SEPARATOR_STR_A;
            resyncRequiredFilePath += "resyncRequired";
            ACE_stat file_stat ={0};
            // PR#10815: Long Path support
            if(sv_stat(getLongPathName(resyncRequiredFilePath.c_str()).c_str(),&file_stat) == 0)
            {
                m_resyncRequired = true;
            }
            else
            {
                m_resyncRequired = false;
            }

            std::string localFileName = basename_r(filePath.c_str());

            if(!isInDiffSync())
            {
                //In Resync accuracy mode is always not gauranteed
                m_recoveryaccuracy = 2;
                rv = true;
                break;
            }

            //control comes here only when pair is in diffsync
            if(m_resyncRequired)
            {
                m_recoveryaccuracy = 2;
                rv = true;
                break;
            }

            // Mode = Not Gauranteed
            // Required Conditions:
            // 
            // 1. Resync Step 2
            // OR
            // 
            // 2.   Differential Sync + Resync Required flag 
            //            + (if set by source current differential > resync required flag timestamp ) 
            //
            //

            else if ( (m_inquasi_state)
                      || (isInDiffSync()
                          && m_Configurator->isResyncRequiredFlag(m_volumename)
                          && (!(m_Configurator->getResyncRequiredCause(m_volumename) == VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ) 
                              || (change_metadata -> actualStartTime() > m_Configurator->getResyncRequiredTimestamp(m_volumename))
                              ))
                      )
            {
                rv = true;
                m_recoveryaccuracy = 2; //Not Gauranteed
                break;
            }

            //
            // Mode = Exact
            // Required Conditions:
            //     Differential sync + No Resync Required Flag + write ordered (WE/WC)
            //

            else if (isInDiffSync() 
                     && !m_Configurator->isResyncRequiredFlag(m_volumename) 
                     && ( std::string::npos != localFileName.find("_WE") 
                          || std::string::npos != localFileName.find("_WC") 
                          || std::string::npos != localFileName.find("_FE")
                          || std::string::npos != localFileName.find("_FC") )
                     )
            {
                rv = true;
                m_recoveryaccuracy = 0; // exact
                break;
            }

            // Mode  =  Approximate
            // Required Conditions:
            //   Differential Sync + No Resync Requiered Flag + Metadata mode (ME/MC)


            else if (isInDiffSync()
                     && !m_Configurator->isResyncRequiredFlag(m_volumename) 
                     && ( std::string::npos != localFileName.find("_ME") 
                          || std::string::npos != localFileName.find("_MC") )
                     )
            {
                rv = true;
                m_recoveryaccuracy = 1; // approximate
                break;
            }

            rv = false;

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


// =================================================================
//  update volume without retention enabled
//   updatevolume_nocdp
//     construct_nonoverlapping_drtds
//       disjoint
//       contains
//       r_overlap
//       l_overlap
//       iscontained
// =================================================================

/*
 * FUNCTION NAME :  CDPV2Writer::updatevolume_nocdp
 *
 * DESCRIPTION : Updates the volume with changes from incoming differential
 *
 *
 * INPUT PARAMETERS : volumename - target volume name from which COW will be read
 *                    filePath - absolute path to diff file in cache dir
 *                    change_metadata - metadata of the diff file we are processing
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM : 
 * 
 *  while drtds pending
 *      get next drtd
 *      find the non overlap region after comparing with rest of the drtds 
 *      
 *          update vsnaps for cow before making change on the volume
 *          
 *
 * return value : true if updation successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevolume_nocdp(const std::string & volumename,
                                     const std::string & filePath, 
                                     char * diff_data,
                                     DiffPtr change_metadata)
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        do
        {

            assert(m_cdpenabled == false);
            if(m_cdpenabled)
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s assumption m_cdpenabled=false failed.\n", FUNCTION_NAME);
                rv = false;
                break;
            }


            std::string  fileBeingApplied = filePath;
            std::string::size_type idx = fileBeingApplied.rfind(".gz");
            if(std::string::npos != idx && (fileBeingApplied.length() - idx) == 3)
                fileBeingApplied.erase(idx, fileBeingApplied.length());

            std::string fileBeingAppliedBaseName = basename_r(fileBeingApplied.c_str());

            // 
            // step 0:
            // verify the file version
            if(!verify_file_version(fileBeingAppliedBaseName,change_metadata))
            {
                rv = false;
                break;
            }

            //
            // step 1:
            // verify the timestamp ordering
            // 
            // verify we are processingmfiles in correct order
            // of increasing timestamps
            bool in_order = verify_increasing_timestamp(fileBeingAppliedBaseName,change_metadata);

            if(!in_order)
            {
                if((!m_allowOutOfOrderTS || !m_allowOutOfOrderSeq))
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "Differentials will not be applied on %s due to files recieved out of order\n",
                                m_volumename.c_str());
                    CDPUtil::QuitRequested(180);
                    rv = false;
                    break;
                }

                if(CDPUtil::QuitRequested())
                {
                    rv = false;
                    break;
                }
            }

            if(m_checkDI)
            {
                m_checksums.clear();
                m_checksums.filename = filePath;
            }

            cdp_drtdv2s_t nonoverlapping_changes;

            if(!construct_nonoverlapping_drtds(change_metadata -> DrtdsBegin(), 
                                               change_metadata -> DrtdsEnd(), 
                                               nonoverlapping_changes))
            {
                rv = false;
                break;
            }


            PerfTime vsnapupdate_start_time;
            getPerfTime(vsnapupdate_start_time);

            updatevsnaps_v1(volumename,"",nonoverlapping_changes.begin(), nonoverlapping_changes.end());

            PerfTime vsnapupdate_end_time;
            getPerfTime(vsnapupdate_end_time);
            m_vsnapupdate_time = getPerfTimeDiff(vsnapupdate_start_time,vsnapupdate_end_time);


            PerfTime volwrite_start_time;
            getPerfTime(volwrite_start_time);
            if(!updatevolume(volumename,filePath,diff_data,
                             nonoverlapping_changes.begin(), nonoverlapping_changes.end()))
            {
                rv = false;
                break;
            }
            PerfTime volwrite_end_time;
            getPerfTime(volwrite_end_time);
            m_volwrite_time = getPerfTimeDiff(volwrite_start_time,volwrite_end_time);


            if(m_checkDI && m_verifyDI)
            {
                if(!calculate_checksums_from_volume(volumename, filePath, 
                                                    nonoverlapping_changes.begin(),
                                                    nonoverlapping_changes.end()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s Unable to calculate checksums for volume: %s Data File: %s\n",
                                FUNCTION_NAME, volumename.c_str(), filePath.c_str());
                    rv = false;
                    break;
                }
            }


            if(rv && m_checkDI)
            {
                if(!WriteChecksums(volumename, filePath))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s Unable to write checksums for volume: %s Data File: %s\n",
                                FUNCTION_NAME, volumename.c_str(), filePath.c_str());
                    rv = false;
                    break;
                }
            }


        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME : construct_nonoverlapping_drtds
 *
 * DESCRIPTION : 
 *   keeps only the non overlapping i/o retaining overlap from latest ones
 *               
 * INPUT PARAMETERS : 
 *   drtdsIter, drtdsEnd - const iterator to DRTDs in metadata
 *                    
 * OUTPUT PARAMETERS : 
 *   drtds_to_apply - vector which contains the non-overlap DRTDs to apply 
 *                    on target volume
 *
 * ALGORITHM :
 * 
 *    1. For each change in the list of changes
 *    2. Add the change to output list based on the non-overlapping condition
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::construct_nonoverlapping_drtds(const cdp_drtdv2s_constiter_t & drtdsIter, 
                                                 const cdp_drtdv2s_constiter_t & drtdsEnd, 
                                                 cdp_drtdv2s_t & drtds_to_apply)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);


    try
    {
        //__asm int 3;
        cdp_drtdv2_t r_nonoverlap;
        cdp_drtdv2_t l_nonoverlap;
        std::queue<cdp_drtdv2_t> drtds_to_filter;

        cdp_drtdv2s_constiter_t drtds_remaining_iter = drtdsIter;
        drtds_to_apply.clear();


        while( drtds_remaining_iter != drtdsEnd )
        {
            cdp_drtdv2_t drtd((*drtds_remaining_iter).get_length(),
                              (*drtds_remaining_iter).get_volume_offset(),
                              (*drtds_remaining_iter).get_file_offset(),
                              (*drtds_remaining_iter).get_seqdelta(),
                              (*drtds_remaining_iter).get_timedelta(),
                              (*drtds_remaining_iter).get_fileid());


            drtds_to_filter.push(drtd);
            ++drtds_remaining_iter;

            if(drtds_remaining_iter == drtdsEnd)
                break;

            // in resync, all ios are non overlapping
            if(!isInDiffSync())
            {
                continue;
            }

            unsigned int count = drtds_to_filter.size();
            while(count > 0)
            {
                cdp_drtdv2_t drtd_to_filter = drtds_to_filter.front();
                drtds_to_filter.pop();
                --count;

                if(disjoint(*drtds_remaining_iter, drtd_to_filter))
                {
                    drtds_to_filter.push(drtd_to_filter);
                    continue;
                }

                if(contains(*drtds_remaining_iter, drtd_to_filter))
                    continue;

                if(r_overlap(*drtds_remaining_iter, drtd_to_filter,r_nonoverlap))
                {
                    drtds_to_filter.push(r_nonoverlap);
                    continue;
                }

                if(l_overlap(*drtds_remaining_iter, drtd_to_filter, l_nonoverlap))
                {
                    drtds_to_filter.push(l_nonoverlap);
                    continue;
                }

                if(iscontained(*drtds_remaining_iter, drtd_to_filter, l_nonoverlap, r_nonoverlap))
                {
                    if(0 != l_nonoverlap.get_length())
                    {
                        drtds_to_filter.push(l_nonoverlap);
					}
					else
                    {
                        DebugPrintf(SV_LOG_ERROR," Note: this is not an error: l_nonoverlap length = zero.\n");
                    }
                    if(0 != r_nonoverlap.get_length())
                    {
                        drtds_to_filter.push(r_nonoverlap);
					}
					else
                    {
                        DebugPrintf(SV_LOG_ERROR,"Note: this is not an error: r_nonoverlap length = zero.\n");
                    }
                    continue;
                }
            }
        }

        SV_UINT length_processed = 0;
        SV_UINT length_pending = 0;

        while(!drtds_to_filter.empty())
        {
            cdp_drtdv2_t drtd = drtds_to_filter.front();
            if(drtd.get_length() < m_max_rw_size)
            {
                drtds_to_apply.push_back(drtd);
			}
			else
            {
                length_processed = 0;
                length_pending = drtd.get_length();
                while(length_pending > 0)
                {
                    cdp_drtdv2_t sub_drtd(min(m_max_rw_size, length_pending),
                                          drtd.get_volume_offset() +length_processed,
                                          drtd.get_file_offset() + length_processed,
                                          drtd.get_seqdelta(),
                                          drtd.get_timedelta(),
                                          drtd.get_fileid());

                    length_processed += sub_drtd.get_length();
                    length_pending -= sub_drtd.get_length();
                    drtds_to_apply.push_back(sub_drtd);
                }
            }

            drtds_to_filter.pop();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::disjoint(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y)
{
    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    return ( (y_endoffset < x_startoffset) || ( y_startoffset > x_endoffset) );
}

// x contains y
bool CDPV2Writer::contains(const cdp_drtdv2_t & original_drtd,
                           const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                           cdp_cow_overlap_t & overlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset >= y_endoffset) )
    {
        overlap.relative_voloffset = y_startoffset - original_drtd.get_volume_offset();
        overlap.length = y.get_length();
        overlap.fileoffset = x.get_file_offset() + (y_startoffset - x_startoffset);
        return true;
    }

    return false;
}

// x contains y
bool CDPV2Writer::contains(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset >= y_endoffset) )
    {
        return true;
    }

    return false;
}

// x overlaps y from right, get non overlapping portion from y
bool CDPV2Writer::r_overlap(const cdp_drtdv2_t & original_drtd,
                            const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                            cdp_drtdv2_t & r_nonoverlap,
                            cdp_cow_overlap_t & overlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset < y_endoffset) )
    {
        SV_UINT r_overlap_len = (x_endoffset - y_startoffset + 1);
        cdp_drtdv2_t nonoverlap(y_endoffset - x_endoffset,
                                x_endoffset + 1,
                                y.get_file_offset() + r_overlap_len,
                                y.get_seqdelta(),
                                y.get_timedelta(),
                                y.get_fileid());
        r_nonoverlap = nonoverlap;

        overlap.relative_voloffset = y_startoffset - original_drtd.get_volume_offset();
        overlap.length = r_overlap_len;
        overlap.fileoffset = x.get_file_offset()+ (y_startoffset - x_startoffset);

        return true;
    }

    return false;
}


// x overlaps y from right, get non overlapping portion from y
bool CDPV2Writer::r_overlap(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                            cdp_drtdv2_t & r_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset < y_endoffset) )
    {
        SV_UINT r_overlap_len = (x_endoffset - y_startoffset + 1);
        cdp_drtdv2_t nonoverlap(y_endoffset - x_endoffset,
                                x_endoffset + 1,
                                y.get_file_offset() + r_overlap_len,
                                y.get_seqdelta(),
                                y.get_timedelta(),
                                y.get_fileid());
        r_nonoverlap = nonoverlap;

        return true;
    }

    return false;
}

// x overlaps y from left,get non overlap portion from y
bool CDPV2Writer::l_overlap(const cdp_drtdv2_t & original_drtd,
                            const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                            cdp_drtdv2_t & l_nonoverlap,
                            cdp_cow_overlap_t &overlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (y_startoffset < x_startoffset) && (x_endoffset >= y_endoffset) )
    {
        cdp_drtdv2_t nonoverlap(x_startoffset - y_startoffset,
                                y_startoffset,
                                y.get_file_offset(),
                                y.get_seqdelta(),
                                y.get_timedelta(),
                                y.get_fileid());
        l_nonoverlap = nonoverlap;

        overlap.relative_voloffset = x_startoffset - original_drtd.get_volume_offset();
        overlap.length= y_endoffset - x_startoffset + 1;
        overlap.fileoffset = x.get_file_offset();
        return true;
    }

    return false;
}

// x overlaps y from left,get non overlap portion from y
bool CDPV2Writer::l_overlap(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                            cdp_drtdv2_t & l_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (y_startoffset < x_startoffset) && (x_endoffset >= y_endoffset) )
    {
        cdp_drtdv2_t nonoverlap(x_startoffset - y_startoffset,
                                y_startoffset,
                                y.get_file_offset(),
                                y.get_seqdelta(),
                                y.get_timedelta(),
                                y.get_fileid());
        l_nonoverlap = nonoverlap;

        return true;
    }

    return false;
}

// x is contained by y
bool CDPV2Writer::iscontained(const cdp_drtdv2_t & original_drtd,
                              const cdp_drtdv2_t & x, const cdp_drtdv2_t & y, 
                              cdp_drtdv2_t & l_nonoverlap,
                              cdp_drtdv2_t & r_nonoverlap,
                              cdp_cow_overlap_t &overlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset >= y_startoffset) && (x_endoffset <= y_endoffset) )
    {
        SV_UINT overlap_len = x.get_length();
        cdp_drtdv2_t nonoverlap1(x_startoffset - y_startoffset,
                                 y_startoffset,
                                 y.get_file_offset(),
                                 y.get_seqdelta(),
                                 y.get_timedelta(),
                                 y.get_fileid());
        l_nonoverlap = nonoverlap1;


        cdp_drtdv2_t nonoverlap2(y_endoffset - x_endoffset,
                                 x_endoffset + 1,
                                 y.get_file_offset() + overlap_len + nonoverlap1.get_length(),
                                 y.get_seqdelta(),
                                 y.get_timedelta(),
                                 y.get_fileid());
        r_nonoverlap = nonoverlap2;

        overlap.relative_voloffset = x_startoffset - original_drtd.get_volume_offset();
        overlap.length = x.get_length();
        overlap.fileoffset = x.get_file_offset();

        return true;
    }

    return false;
}

// x is contained by y
bool CDPV2Writer::iscontained(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y, 
                              cdp_drtdv2_t & l_nonoverlap,
                              cdp_drtdv2_t & r_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x.get_volume_offset();
    SV_ULONGLONG y_startoffset = y.get_volume_offset();
    SV_ULONGLONG x_endoffset = (x.get_volume_offset() + x.get_length() -1);
    SV_ULONGLONG y_endoffset = (y.get_volume_offset() + y.get_length() -1);

    if( (x_startoffset >= y_startoffset) && (x_endoffset <= y_endoffset) )
    {
        SV_UINT overlap_len = x.get_length();
        cdp_drtdv2_t nonoverlap1(x_startoffset - y_startoffset,
                                 y_startoffset,
                                 y.get_file_offset(),
                                 y.get_seqdelta(),
                                 y.get_timedelta(),
                                 y.get_fileid());
        l_nonoverlap = nonoverlap1;


        cdp_drtdv2_t nonoverlap2(y_endoffset - x_endoffset,
                                 x_endoffset + 1,
                                 y.get_file_offset() + overlap_len + nonoverlap1.get_length(),
                                 y.get_seqdelta(),
                                 y.get_timedelta(),
                                 y.get_fileid());
        r_nonoverlap = nonoverlap2;

        return true;
    }

    return false;
}

// =======================================================
//  
//  Volume update routines
//
//  updatevolume
//     updatevolume_asynch_inmem_difffile
//        write_async_drtds_to_volume
//            initiate_next_async_write
//              handle_write_file
//        flush_io
// 							   
//    updatevolume_synch_inmem_difffile
//        write_sync_drtds_to_volume
//           write_sync_drtd_to_volume
// 
//    updatevolume_asynch_ondisk_difffile
//        readDRTDFromBio
// 
//    updatevolume_synch_ondisk_difffile 
//
// =======================================================

bool CDPV2Writer::updatevolume(const std::string & volumename,
                               const std::string& filePath,
                               const char *const diff_data, 
                               const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                               const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s volumename:%s diff:%s.\n",
		FUNCTION_NAME, volumename.c_str(), filePath.c_str());

    do
    {
        assert(0 == m_pendingios);
        if(0 != m_pendingios)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption 0 == m_pendingios failed. m_pendingios: %d\n",
                        FUNCTION_NAME, m_pendingios);
            rv = false;
            break;
        }

        assert(false == m_error);
        if(false != m_error)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption false == m_error failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        //assert(0 == m_memconsumed_for_io);
        //if(0 != m_memconsumed_for_io)
        //{
        //	DebugPrintf(SV_LOG_ERROR, 
        //		"FUNCTION:%s assumption 0 == m_memconsumed_for_io failed. m_memconsumed_for_io: %d\n",
        //		FUNCTION_NAME, m_memconsumed_for_io);
        //	rv = false;
        //	break;
        //}

        assert(m_buffers_allocated_for_io.empty());
        if(!m_buffers_allocated_for_io.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_buffers_allocated_for_io.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_asyncwrite_handles.empty());
        if(!m_asyncwrite_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_asyncwrite_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_syncwrite_handles.empty());
        if(!m_syncwrite_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_syncwrite_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_asyncread_handles.empty());
        if(!m_asyncread_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_asyncread_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_syncread_handles.empty());
        if(!m_syncread_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_syncread_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        // PR# 21495
        // Workaround: If we see asynchronous i/o failing
        // we will use blocking io just for next invocation
        // m_UseBlockingIoForCurrentInvocation is set to true
        // in such a scenario.
        // This workaround is going to work only in case of 
        // writing to virtual volume sparse files
        //

        if(m_isFileInMemory)
        {
            if(m_useAsyncIOs && !m_UseBlockingIoForCurrentInvocation)
            {
                rv = updatevolume_asynch_inmem_difffile(volumename, diff_data,
                                                        nonoverlapping_batch_start,nonoverlapping_batch_end);
            }
            else
            {
                rv = updatevolume_synch_inmem_difffile(volumename, diff_data,
                                                       nonoverlapping_batch_start,nonoverlapping_batch_end);
                m_UseBlockingIoForCurrentInvocation = false;
            }
        }
        else
        {
            if(m_useAsyncIOs && !m_UseBlockingIoForCurrentInvocation)
            {
                rv = updatevolume_asynch_ondisk_difffile(volumename, filePath,
                                                         nonoverlapping_batch_start,nonoverlapping_batch_end);
            }
            else
            {
                rv = updatevolume_synch_ondisk_difffile(volumename, filePath,
                                                        nonoverlapping_batch_start,nonoverlapping_batch_end);
                m_UseBlockingIoForCurrentInvocation = false;
            }
        }

    }while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s volumename:%s diff:%s.\n",
		FUNCTION_NAME, volumename.c_str(), filePath.c_str());
    return rv;
}



/*
 * FUNCTION NAME :  CDPV2Writer::updatevolume_asynch_inmem_difffile
 *
 * DESCRIPTION : Writes the differential data to the target volume
 *               asynchronously from inmemory diff file
 *
 * INPUT PARAMETERS : volumename - target volume name
 *                    diff_data - diff data to be written to volume
 *                    metadata - contains change information that need to be
 *                               written from diff_data to volume
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 *
 *     1.  Update vsnaps
 *     2.  Open the target volume for unbuffered async IOs
 *     3.  Copy the changed data to a sector aligned buffer
 *     4.  for each change in this difetential, repeat the steps 5 & 6
 *     5.  Find the set of non overlapping changes 
 *     6.  Write these changes asynchronously to the target volume
 *     7.  Close the target volume
 *
 * return value : true if volume updation is successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevolume_asynch_inmem_difffile(const std::string & volumename,
                                                     const char *const diff_data, 
                                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{
    bool rv = true;


    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);


    try
    {
        do
        {

            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;

            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }

            SharedAlignedBuffer pvolumedata;
            char * pbuffer = 0;

            bool memallocated = false;
            SV_UINT bytescopied = 0;
            SV_UINT totalbytes = 0;
            SV_UINT mem_required = 0;

            m_memconsumed_for_io = 0;
            m_memtobefreed_on_io_completion = true;

            if(!get_mem_required(nonoverlapping_batch_start,
                                 nonoverlapping_batch_end, 
                                 totalbytes))
            {
                rv = false;
                break;
            }

            if(totalbytes <= m_maxMemoryForDrtds)
            {
                // allocate the sector aligned buffer of required length
                pvolumedata.Reset(totalbytes, m_sector_size);
                m_memconsumed_for_io = totalbytes;
                m_memtobefreed_on_io_completion = false;
                memallocated = true;
            }



            for( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter)
            {
                cdp_drtdv2_t drtd = *drtdsIter;

                // we will take a seperate code path for multi sparse file scenario
                if(m_ismultisparsefile)
                {
					int size = pvolumedata.Size() - bytescopied;
                    if(!write_async_inmemdrtd_multisparse_volume(diff_data, drtd, 
						(memallocated) ? (pvolumedata.Get() + bytescopied) : 0, (memallocated) ? (size) : 0)
                       )
                    {
                        rv = false;
                        break;
                    }

                    // if we are passing null buffer, memory allocation and release 
                    // will be done within write_async_drtd_multisparse_volume routine
                    // data is read to the buffer inside the routine
                    //
                    bytescopied += drtd.get_length();

                    // rest of code is for physical volume and single virtual volume case
                    continue;
                } 


                if(m_isvirtualvolume)
                {

#ifdef SV_WINDOWS

                    // for single sparse virtual volumes 
                    // writes are now happening directly to sparse file instead of going through
                    // volpack driver
                    // accordingly, even punching of hole in case of all data being zeroes
                    // is moved to userspace

                    if((!m_volpackdriver_available) && m_punch_hole_supported && m_sparse_enabled &&
                       all_zeroes((diff_data + drtd.get_file_offset()),drtd.get_length()))
                    {
                        if(!punch_hole_sparsefile(m_volguid,
                                                  drtd.get_volume_offset(),
                                                  drtd.get_length()))
                        {
                            if(m_punch_hole_supported)
                            {
                                rv = false;
                                break;
                            }
                        }
                        else
                        {
                            if(m_checkDI)
                            {
                                DiffChecksum checksum;
                                checksum.length = drtd.get_length();
                                checksum.offset = drtd.get_volume_offset();
                                ComputeHash(diff_data + drtd.get_file_offset(), drtd.get_length(), checksum.digest);
                                m_checksums.checksums.push_back(checksum);
                            }
	
                            // continue with next drtd, this drtd is all zeroes
                            // and hole is punched
                            bytescopied += drtd.get_length();
                            continue;
                        }
                    }

#endif // punch hole support for virtual volume in windows
                } 


                // rest of this code is for 
                //  1) Physical volume
                //  2) windows: single sparse virtual volume - non zero data
                //  3) Unix: single sparse virtual volume - both zero and non-zero data
                // 
                //  note: windows single sparse virtual volume zero data 
                //        and multi sparse virtual volume both zero and non-zero data
                //        is covered earlier above
                //

                if(!memallocated)
                {
                    mem_required = drtd.get_length();
                } else
                {
                    mem_required = 0;
                }

                if(!canIssueNextIo(mem_required))
                {
                    rv = false;
                    break;
                }

				int pbuffer_size; // To find buffer length
                if(!memallocated)
                {
                    // allocate the sector aligned buffer of required length
                    SharedAlignedBuffer palignedbuffer(drtd.get_length(), m_sector_size);

                    //
                    // insert this SharedAlignedBuffer into a map
                    // so that it's use count is increased
                    // it will be freed when the write to volume 
                    // is done by handle_write_file routine
                    // 
                    SV_UINT bytespending = drtd.get_length();
                    SV_ULONGLONG start_offset = drtd.get_volume_offset();
                    while(bytespending > 0)
                    {
                        SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                        add_memory_allocation(m_volhandle,start_offset,bytes_to_queue,palignedbuffer);
                        start_offset += bytes_to_queue;
                        bytespending -= bytes_to_queue;
                    }
                    pbuffer = palignedbuffer.Get();
					pbuffer_size = palignedbuffer.Size();
				} else
                {
                    pbuffer = pvolumedata.Get() + bytescopied;
					pbuffer_size = pvolumedata.Size() - bytescopied;
			    }

				inm_memcpy_s(pbuffer, pbuffer_size, (diff_data + drtd.get_file_offset()), drtd.get_length());
                bytescopied += drtd.get_length();


#ifdef SV_WINDOWS
                // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
                // in general this should not happen as we can make sure that sync hash
                // compare block size is always greater then FST_SCTR_OFFSET_511
                if (m_hideBeforeApplyingSyncData && (0 == drtd.get_volume_offset()) && (drtd.get_length() >= FST_SCTR_OFFSET_511))
                {
                    HideBeforeApplyingSyncData(pbuffer,pbuffer_size);
                }
#endif

                if(m_checkDI)
                {
                    DiffChecksum checksum;
                    checksum.length = drtd.get_length();
                    checksum.offset = drtd.get_volume_offset();
                    ComputeHash(pbuffer, drtd.get_length(), checksum.digest);
                    m_checksums.checksums.push_back(checksum);
                }

                if(!initiate_next_async_write(m_wfptr, volumename, pbuffer, drtd.get_length(),
                                              drtd.get_volume_offset()))
                {
                    rv = false;
                    break;
                }
            }

            if(rv)
            {
                // perform this check only if we have issued 
                // all the ios
                assert(totalbytes == bytescopied);
                if(totalbytes != bytescopied)
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "FUNCTION:%s bytes written/copied %d to volume %s does not match the input %d.\n",
                                FUNCTION_NAME, bytescopied, volumename.c_str(), totalbytes);
                    rv = false;
                    break;
                }
            }

            // wait for all ios that have been issued to complete
            if(!wait_for_xfr_completion())
            {
                rv = false;
                break;
            }

        } while (0);


        if(!m_ismultisparsefile)
        {
#ifndef SV_WINDOWS
            if(!m_useUnBufferedIo)
            {
                if(!flush_io(m_volhandle, volumename))
                {
                    m_error = true;
                    rv = false;
                }
            }
            issue_pagecache_advise(m_volhandle, nonoverlapping_batch_start, nonoverlapping_batch_end, INM_POSIX_FADV_DONTNEED);
#endif

        }
        else
        {
            if(!close_asyncwrite_handles())
                rv =false;  
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

/*
 * FUNCTION NAME : get_mem_required
 *
 * DESCRIPTION : 
 *   Gets the memory required for given drtds
 *               
 * INPUT PARAMETERS : 
 *   nonoverlapping_drtds - vector of drtds
 *
 *                    
 * OUTPUT PARAMETERS : 
 *   mem_required - memory required for copying the drtd data
 *
 * ALGORITHM :
 * 
 *    1. For each change in the list of changes
 *    2. Add the length to memory required
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::get_mem_required(const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                   const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                                   SV_UINT & mem_required)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        mem_required = 0;
        cdp_drtdv2s_constiter_t drtds_tmpiter = nonoverlapping_batch_start;

        for( ; drtds_tmpiter != nonoverlapping_batch_end ; ++drtds_tmpiter)
        {
            mem_required += drtds_tmpiter ->get_length();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



/*
 * FUNCTION NAME :  CDPV2Writer::initiate_next_async_write
 *
 * DESCRIPTION : Initiate an asynchronous write for the DRTD
 *
 *
 * INPUT PARAMETERS : vol_handle - handle to the file(target volume) to which
 *                                 we write the data asynchronously
 *                    diff_data - pointer to the aligned buffer which contains
 *                                data to be written
 *                    drtdPtr - pointer to a cdp_drtdv2_t for which write has
 *                              to be initiated
 * 
 * OUTPUT PARAMETERS : byteswritten - no. of bytes written to volume
 *
 * ALGORITHM :
 * 
 *    1. Initiate a new asynchronous write 
 *    2. if the async write is successfully initiated increment pending IOs
 *
 * return value : true if successfully initiated the write and false otherwise
 *
 */
bool CDPV2Writer::initiate_next_async_write(ACE_Asynch_Write_File_Ptr vol_handle, 
                                            const std::string & filename,
                                            const char * buffer, 
                                            const SV_UINT &length,
                                            const SV_OFFSET_TYPE& offset)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, filename.c_str(), offset, length);

    try
    {
        do
        {

            SV_ULONG bytespending = length;
            SV_ULONGLONG writeoffset = offset;
            SV_ULONG bytesWritten = 0;


            while(bytespending > 0)
            {


                SV_ULONG bytes_to_queue = min((SV_ULONG)m_max_rw_size, (SV_ULONG)(bytespending));

                ACE_Message_Block *mb = new ACE_Message_Block (
                    (char *) (buffer + bytesWritten), 
                    bytes_to_queue);

                if(!mb)
                {
                    stringstream l_stderr;
                    l_stderr   << "memory allocation(message block) failed "
                               << "while initiating write on " << filename << ".\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    m_error = true;
                    rv = false;
                    break;
                }

                mb ->wr_ptr(bytes_to_queue);
                incrementPendingIos();

                if(vol_handle -> write(*mb,bytes_to_queue, 
                                       (unsigned int)(writeoffset), 
                                       (unsigned int)(writeoffset >> 32)) == -1)
                {
                    int errorcode = ACE_OS::last_error();
                    std::stringstream l_stderr;
                    l_stderr << "Asynch write request for " << filename
                             << "at offset " << writeoffset << " failed. error no: "
                             << errorcode << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

                    WriteFailureAlertAtOffset a(filename, writeoffset, bytes_to_queue, errorcode);
                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFERENTIALSYNC, a);

                    decrementPendingIos();
                    m_error = true;
                    rv = false;
                    break;
                }

                bytesWritten += bytes_to_queue;
                writeoffset += bytes_to_queue;
                bytespending -= bytes_to_queue;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, filename.c_str(), offset, length);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::handle_write_file
 *
 * DESCRIPTION : Callback function which will be called once when single asynch
 *               IO is completed
 *
 * INPUT PARAMETERS : result - contains the status of the requested Asynch IO
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  Release the message block allocated while initiating the async write
 *     2.  Decrement pending IOs
 *
 * return value : none
 *
 */
void CDPV2Writer::handle_write_file (const ACE_Asynch_Write_File::Result &result)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            // release message block.
            result.message_block ().release ();

            if (result.success ())
            {
                unsigned long long offset = 
                    ((unsigned long long)(result.offset_high()) << 32) + (unsigned long long)(result.offset());

                if (result.bytes_transferred () != result.bytes_to_write())
                {
                    m_error = true;
                    stringstream l_stderr;
                    l_stderr << "async write at offset "
                             << offset
                             << " failed with error " 
                             <<  result.error()  
                             << "bytes transferred =" << result.bytes_transferred ()
                             << "bytes requested =" << result.bytes_to_write()
                             << "volume name = " << m_volumename
                             << "\n";
                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    break;
                }

                if(!release_memory_allocation(result.handle(),offset,result.bytes_transferred()))
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "FUNCTION:%s  encountered failure when releasing allocated memory.\n",
                                FUNCTION_NAME);
                    m_error = true;
                    break;
                }

			}
			else
            {
                m_error = true;
                stringstream l_stderr;
                l_stderr << "async write to volume " << m_volumename << " at "
                         << result.offset() << " " << result.offset_high()
                         << " failed with error " 
                         <<  result.error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                // PR# 21495
                // Workaround: If we see asynchronous i/o failing
                // we will use blocking io just for next invocation
                // This workaround is going to work only in case of 
                // writing to virtual volume sparse files
                //
                if(m_ismultisparsefile)
                    m_UseBlockingIoForCurrentInvocation = true;
                break;
            }

        } while (0);
    }
    catch ( ContextualException& ce )
    {
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    decrementPendingIos();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool CDPV2Writer::flush_io(ACE_HANDLE io_handle, const std::string & filename)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s filename:%s\n",
		FUNCTION_NAME,filename.c_str());

#ifndef SV_WINDOWS
    if(!m_useUnBufferedIo)
    {
#endif

        if(ACE_OS::fsync(io_handle) == -1)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "Flush buffers failed for %s with error %d.\n",
                        filename.c_str(),
                        ACE_OS::last_error());
            rv = false;
        }
#ifndef SV_WINDOWS
    }
#endif
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s filename:%s\n",
		FUNCTION_NAME,filename.c_str());
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::updatevolume_synch_inmem_difffile
 *
 * DESCRIPTION : Writes the differential data to the target volume
 *               synchronously from inmemory diff file
 *
 * INPUT PARAMETERS : volumename - target volume name
 *                    diff_data - diff data to be written to volume
 *                    metadata - contains change information that need to be
 *                               written from diff_data to volume
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  Update vsnaps
 *     2.  Open the target volume for unbuffered synchronous IOs
 *     3.  Copy the changed data to a sector aligned buffer
 *     4.  for each change in this difetential, repeat the step5
 *     5.  Write these changes synchronously to the target volume
 *     6.  Close the target volume
 *
 * return value : true if volume updation is successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevolume_synch_inmem_difffile(const std::string & volumename, 
                                                    const char *const diff_data,
                                                    const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                                    const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;

            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }

            while(drtdsIter != nonoverlapping_batch_end)
            {

                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());

                // get drtds to write based on memory available
                cdp_drtdv2s_constiter_t drtds_to_write_begin;
                cdp_drtdv2s_constiter_t drtds_to_write_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                nonoverlapping_batch_end, 
                                                maxUsableMemory, 
                                                drtds_to_write_begin,
                                                drtds_to_write_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }

                //allocate the sector aligned buffer of required length
                SharedAlignedBuffer pvolumedata(memRequired, m_sector_size);
				int size = pvolumedata.Size(); // To find buffer length
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                // copy drtds to the sector aligned buffer(needed for writing with no buffering)              
                SV_ULONGLONG bytescopied = 0;
                cdp_drtdv2s_constiter_t drtdsTempIter = drtds_to_write_begin;
                for( ; drtdsTempIter != drtds_to_write_end ; ++drtdsTempIter)
                {
                    cdp_drtdv2_t drtd = *drtdsTempIter;
					inm_memcpy_s((char*)(pvolumedata.Get() + bytescopied), (size - bytescopied),
                           (diff_data + drtd.get_file_offset()), 
                           drtd.get_length());
#ifdef SV_WINDOWS
                    // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
                    // in general this should not happen as we can make sure that sync hash
                    // compare block size is always greater then FST_SCTR_OFFSET_511
                    if (m_hideBeforeApplyingSyncData && (0 == drtd.get_volume_offset()) && (drtd.get_length() >= FST_SCTR_OFFSET_511))
                    {
						HideBeforeApplyingSyncData((char *)(pvolumedata.Get() + bytescopied), (size - bytescopied));
                    }
#endif
                    if(m_checkDI)
                    {
                        DiffChecksum checksum;
                        checksum.length = drtd.get_length();
                        checksum.offset = drtd.get_volume_offset();
                        ComputeHash(pvolumedata.Get() + bytescopied, drtd.get_length(), checksum.digest);
                        m_checksums.checksums.push_back(checksum);
                    }
                    bytescopied += drtd.get_length();
                }

                if(!m_ismultisparsefile)
                {
                    //write the drtds from aligned buffer to volume
                    if(!write_sync_drtds_to_volume(pvolumedata.Get(),drtds_to_write_begin, drtds_to_write_end))
                    {
                        rv = false;
                        break;
                    }
                }
                else
                {
                    //write the drtds from aligned buffer to volume
					if (!write_sync_inmemdrtds_to_multisparse_volume(pvolumedata.Get(), drtds_to_write_begin, drtds_to_write_end, pvolumedata.Size()))
                    {
                        rv = false;
                        break;
                    }
                }

                drtdsIter  = drtds_to_write_end;

            }

        } while (0);

        if(!m_ismultisparsefile)
        {

            //
            // flush on target volume 
            // is not applicable on windows
            // as it is in hidden state
            //
#ifndef SV_WINDOWS
            if(!m_useUnBufferedIo)
            {
                if(!flush_io(m_volhandle, volumename))
                    rv  = false;
            }
            issue_pagecache_advise(m_volhandle, nonoverlapping_batch_start, nonoverlapping_batch_end, INM_POSIX_FADV_DONTNEED);

#endif
        }
        else
        {
            if(!close_syncwrite_handles())
                rv = false;
        }
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

/*
 * FUNCTION NAME : get_drtds_limited_by_memory
 *
 * DESCRIPTION : 
 *   Gets the list of DRTDs that can fit in available memory
 *               
 * INPUT PARAMETERS : 
 *   drtdsIter, drtdsEnd - const iterator to DRTDs in metadata
 *   maxUsableMemory - max available memory
 *                    
 * OUTPUT PARAMETERS : 
 *   out_drtds - vector which contains the DRTDs to apply
 *   memRequired - memory required for copying the drtd data
 *
 * ALGORITHM :
 * 
 *    1. For each change in the list of changes
 *    2. Add the change to output list if memory is available
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::get_drtds_limited_by_memory(const cdp_drtdv2s_constiter_t & drtdsIter, 
                                              const cdp_drtdv2s_constiter_t & drtdsEnd, 
                                              const SV_ULONGLONG & maxUsableMemory, 
                                              cdp_drtdv2s_constiter_t & out_drtds_start,
                                              cdp_drtdv2s_constiter_t & out_drtds_end, 
                                              SV_ULONGLONG &memRequired)
{

    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        memRequired = 0;
        cdp_drtdv2s_constiter_t drtdstmpIter = drtdsIter;
        out_drtds_start = drtdstmpIter;

        for( ; drtdstmpIter != drtdsEnd ; ++drtdstmpIter)
        {
            cdp_drtdv2_t drtd = *drtdstmpIter;

            if(memRequired + drtd.get_length() > maxUsableMemory)
            {
                break;
            }          

            memRequired += drtd.get_length();
        }
        out_drtds_end = drtdstmpIter;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}




bool CDPV2Writer::write_sync_drtds_to_volume(char * buffer,
                                             const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                                             const cdp_drtdv2s_constiter_t & drtds_to_write_end)
{
    bool rv = true;

    cdp_drtdv2s_constiter_t drtdsIter = drtds_to_write_begin;

    SV_ULONGLONG bytesWritten = 0;
    for( ; drtdsIter != drtds_to_write_end; ++drtdsIter)
    {
        cdp_drtdv2_t drtd = *drtdsIter;
        if(!write_sync_drtd_to_volume(buffer + bytesWritten, drtd))
        {
            rv = false;
            break;
        }
        bytesWritten += drtd.get_length();
    }

    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::write_sync_drtd_to_volume
 *
 * DESCRIPTION : Writes a single DRTD to the target volume
 *
 * INPUT PARAMETERS :
 *
 *     vol_handle    -  target volume handle to which DRTDs should be written
 *     drtd       -  DRTDV2 
 * 
 * OUTPUT PARAMETERS : buffer  - buffer containing the DRTD
 * 
 * ALGORITHM :
 * 
 * return value : true if write successfull and false otherwise
 *
 */
bool CDPV2Writer::write_sync_drtd_to_volume(char *buffer, const cdp_drtdv2_t &drtd)
{
    bool rv = true;

    SV_OFFSET_TYPE volumeOffset = drtd.get_volume_offset();
    SV_ULONGLONG drtdRemainingLength = drtd.get_length();
    do
    {
        if(ACE_OS::llseek(m_volhandle,volumeOffset,SEEK_SET) < 0)
        {
            stringstream l_stderr;
            l_stderr   << "Seek to offset " << volumeOffset
                       << " failed for volume " << m_volumename
                       << ". error code: " << ACE_OS::last_error() << std::endl;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        while(drtdRemainingLength > 0)
        {
            SV_ULONGLONG bytesToWrite = min((SV_ULONGLONG) m_max_rw_size, drtdRemainingLength);
            SV_ULONGLONG bytesWritten = 0;

            if((bytesWritten = ACE_OS::write(m_volhandle,buffer, bytesToWrite)) != bytesToWrite)
            {
                stringstream l_stderr;
                l_stderr   << "Write at offset " << volumeOffset
                           << " failed for volume " << m_volumename
                           << ". error code: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            drtdRemainingLength -= bytesWritten;
            volumeOffset += bytesWritten;
            buffer += bytesWritten;
        }
    }while(false);
    return rv;
}




/*
 * FUNCTION NAME :  CDPV2Writer::updatevolume_asynch_ondisk_difffile
 *
 * DESCRIPTION : Writes the differential data to the target volume
 *               asynchronously from ondisk diff file
 *
 * INPUT PARAMETERS : volumename - target volume name
 *                    filePath   - DRTDs will be read from this file
 *                    metadata - contains change information that need to be
 *                               written from diff_data to volume
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 *
 *     1.  Update vsnaps
 *     2.  Open the target volume for unbuffered async IOs
 *     3.  Copy the changed data to a sector aligned buffer
 *     4.  for each change in this difetential, repeat the steps 5 & 6
 *     5.  Find the set of non overlapping changes 
 *     6.  Write these changes asynchronously to the target volume
 *     7.  Close the target volume
 *
 * return value : true if volume updation is successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevolume_asynch_ondisk_difffile(const std::string & volumename, 
                                                      const std::string& filePath, 
                                                      const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                                      const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;
    BasicIo diffFileBIo;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {


        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }



            diffFileBIo.Open(filePath,	BasicIo::BioReadExisting | BasicIo::BioShareRead |
                             BasicIo::BioShareWrite | BasicIo::BioShareDelete);
            if(!diffFileBIo.Good())
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            volumename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            SharedAlignedBuffer pvolumedata;
            char * pbuffer = 0;

            bool memallocated = false;
            SV_UINT bytescopied = 0;
            SV_UINT totalbytes = 0;
            SV_UINT mem_required = 0;

            m_memconsumed_for_io = 0;
            m_memtobefreed_on_io_completion = true;

            if(!get_mem_required(nonoverlapping_batch_start,
                                 nonoverlapping_batch_end, totalbytes))
            {
                rv = false;
                break;
            }


            if(totalbytes <= m_maxMemoryForDrtds)
            {
                // allocate the sector aligned buffer of required length
                pvolumedata.Reset(totalbytes, m_sector_size);
                m_memconsumed_for_io = totalbytes;
                m_memtobefreed_on_io_completion = false;
                memallocated = true;
            }

            for( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter)
            {
                cdp_drtdv2_t drtd = *drtdsIter;

                // we will take a seperate code path for multi sparse file scenario
                if(m_ismultisparsefile)
                {
					int size = pvolumedata.Size() - bytescopied;
                    if(!write_async_ondiskdrtd_multisparse_volume(diffFileBIo, drtd, 
						(memallocated) ? (pvolumedata.Get() + bytescopied) : 0, (memallocated) ? (size) : 0)
                       )
                    {
                        rv = false;
                        break;
                    }

                    // if we are passing null buffer, memory allocation and release 
                    // will be done within write_async_drtd_multisparse_volume routine
                    // data is read to the buffer inside the routine
                    //
                    bytescopied += drtd.get_length();

                    // rest of code is for physical volume and single virtual volume case
                    continue;
                } 

                if(!memallocated)
                {
                    mem_required = drtd.get_length();
                } else
                {
                    mem_required = 0;
                }

                if(!canIssueNextIo(mem_required))
                {
                    rv = false;
                    break;
                }

				//TO find pbuffer size
				int pbufferSize = 0;
                if(!memallocated)
                {
                    // allocate the sector aligned buffer of required length
                    SharedAlignedBuffer palignedbuffer(drtd.get_length(), m_sector_size);


                    //
                    // insert this SharedAlignedBuffer into a map
                    // so that it's use count is increased
                    // it will be freed when the write to volume 
                    // is done by handle_write_file routine
                    // 
                    SV_UINT bytespending = drtd.get_length();
                    SV_ULONGLONG start_offset = drtd.get_volume_offset();
                    while(bytespending > 0)
                    {
                        SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                        add_memory_allocation(m_volhandle,start_offset,bytes_to_queue,palignedbuffer);
                        start_offset += bytes_to_queue;
                        bytespending -= bytes_to_queue;
                    }
                    pbuffer = palignedbuffer.Get();
					pbufferSize = palignedbuffer.Size();

                } else
                {
                    pbuffer = pvolumedata.Get() + bytescopied;
					pbufferSize = pvolumedata.Size() - bytescopied;
                }

                if(!readDRTDFromBio(diffFileBIo, pbuffer, drtd.get_file_offset(), drtd.get_length()))
                { 
                    rv = false;
                    break;
                }
                bytescopied += drtd.get_length();


                if(m_isvirtualvolume)
                {

#ifdef SV_WINDOWS

                    // for single sparse virtual volumes 
                    // writes are now happening directly to sparse file instead of going through
                    // volpack driver
                    // accordingly, even punching of hole in case of all data being zeroes
                    // is moved to userspace

                    if((!m_volpackdriver_available) && m_punch_hole_supported && m_sparse_enabled &&
                       all_zeroes(pbuffer,drtd.get_length()))
                    {
                        if(!punch_hole_sparsefile(m_volguid,
                                                  drtd.get_volume_offset(),
                                                  drtd.get_length()))
                        {
                            if(m_punch_hole_supported)
                            {
                                rv = false;
                                break;
                            }
                        }
                        else
                        {
                            // memory is not allocated for whole chunk, but allocated per drtd
                            if(!memallocated)
                            {
                                SV_UINT bytespending = drtd.get_length();
                                SV_ULONGLONG start_offset = drtd.get_volume_offset();
                                while(bytespending > 0)
                                {
                                    SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                                    if(!release_memory_allocation(m_volhandle,start_offset,bytes_to_queue))
                                    {
                                        DebugPrintf(SV_LOG_ERROR, 
                                                    "FUNCTION:%s  encountered failure when releasing allocated memory.\n",
                                                    FUNCTION_NAME);
                                        rv  = false;
                                        break;
                                    }
                                    start_offset += bytes_to_queue;
                                    bytespending -= bytes_to_queue;
                                }
                                if(!rv)
                                {                                
                                    rv  = false;
                                    break;
                                }
                            }
	
                            if(m_checkDI)
                            {
                                DiffChecksum checksum;
                                checksum.length = drtd.get_length();
                                checksum.offset = drtd.get_volume_offset();
                                ComputeHash(pbuffer, drtd.get_length(), checksum.digest);
                                m_checksums.checksums.push_back(checksum);
                            }
	
                            // continue with next drtd, this drtd is all zeroes
                            // and hole is punched
                            continue;
                        }
                    }

#endif // punch hole support for virtual volume in windows
                } 


                // rest of this code is for 
                //  1) Physical volume
                //  2) windows: single sparse virtual volume - non zero data
                //  3) Unix: single sparse virtual volume - both zero and non-zero data
                // 
                //  note: windows single sparse virtual volume zero data 
                //        and multi sparse virtual volume both zero and non-zero data
                //        is covered earlier above
                //


#ifdef SV_WINDOWS
                // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
                // in general this should not happen as we can make sure that sync hash
                // compare block size is always greater then FST_SCTR_OFFSET_511
                if (m_hideBeforeApplyingSyncData && (0 == drtd.get_volume_offset()) && (drtd.get_length() >= FST_SCTR_OFFSET_511))
                {
					HideBeforeApplyingSyncData(pbuffer, pbufferSize);
                }
#endif

                if(m_checkDI)
                {
                    DiffChecksum checksum;
                    checksum.length = drtd.get_length();
                    checksum.offset = drtd.get_volume_offset();
                    ComputeHash(pbuffer, drtd.get_length(), checksum.digest);
                    m_checksums.checksums.push_back(checksum);
                }

                if(!initiate_next_async_write(m_wfptr, volumename, pbuffer, drtd.get_length(),
                                              drtd.get_volume_offset()))
                {
                    rv = false;
                    break;
                }
            }

            if(rv)
            {
                // perform this check only if we have issued 
                // all the ios
                assert(totalbytes == bytescopied);
                if(totalbytes != bytescopied)
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "FUNCTION:%s bytes written/copied %d to volume %s does not match the input %d.\n",
                                FUNCTION_NAME, bytescopied, volumename.c_str(), totalbytes);
                    rv = false;
                    break;
                }
            }

            // wait for all ios to complete
            if(!wait_for_xfr_completion())
            {
                rv = false;
                break;
            }

        } while (0);

        diffFileBIo.Close();
        if(!m_ismultisparsefile)
        {
            if(ACE_INVALID_HANDLE != m_volhandle)
            {
#ifndef SV_WINDOWS
                if(!m_useUnBufferedIo)
                {
                    if(!flush_io(m_volhandle, volumename))
                    {
                        m_error = true;
                        rv = false;
                    }
                }

                issue_pagecache_advise(m_volhandle, nonoverlapping_batch_start, nonoverlapping_batch_end, INM_POSIX_FADV_DONTNEED);
#endif
            }
        }
        else
        {
            if(!close_asyncwrite_handles())
                rv =false;  
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


bool CDPV2Writer::readDRTDFromBio(BasicIo &bIo,
                                  char *buffer,
                                  const SV_OFFSET_TYPE & drtdOffset,
                                  const SV_UINT & drtdLength)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        SV_OFFSET_TYPE offset = drtdOffset;
        SV_UINT remainingLength = drtdLength;
        if(bIo.Seek(drtdOffset, BasicIo::BioBeg) != drtdOffset)
        {
            stringstream l_stderr;
            l_stderr   << "Seek to offset " << drtdOffset
                       << " failed for  " << bIo.GetName()
                       << ". error code: " << ACE_OS::last_error() << std::endl;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        while(remainingLength > 0)
        {
            SV_UINT bytesToRead = min(m_max_rw_size, remainingLength);
            SV_UINT bytesRead = 0;

            if( (bytesRead = bIo.Read(buffer, bytesToRead)) != bytesToRead)
            {
                stringstream l_stderr;
                l_stderr   << "Read at offset " << drtdOffset
                           << " failed for  " << bIo.GetName()
                           << ". error code: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            remainingLength -= bytesRead;
            offset += bytesRead;
            buffer += bytesRead;
        }
    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::updatevolume_synch_ondisk_difffile
 *
 * DESCRIPTION : Writes the differential data to the target volume
 *               synchronously by reading form ondisk diff file.
 *
 * INPUT PARAMETERS : 
 *     volumename - target volume name. DRTDs are written to this volume
 *     filePath   - DRTDs will be read from this file
 *                    metadata - contains change information that need to be
 *                               written from diff_data to volume
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 *
 *     1.  Update vsnaps
 *     2.  Open on disk differential file for reading
 *     3.  Open the target volume for writing
 *     4.  for each change in this difetential, repeat the steps 5&6
 *     5.  Read diff data from on disk differential file
 *     5.  Write these changes synchronously to the target volume
 *     6.  Close the diff file & target volume
 *
 * return value : true if volume updation is successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevolume_synch_ondisk_difffile(const std::string& volumename,
                                                     const std::string& filePath,
                                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        BasicIo diffFileBIo;

        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }



            diffFileBIo.Open(filePath,	BasicIo::BioReadExisting | BasicIo::BioShareRead |
                             BasicIo::BioShareWrite | BasicIo::BioShareDelete);
            if(!diffFileBIo.Good())
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filePath.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }


            for( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter)
            {
                cdp_drtdv2_t drtd = *drtdsIter;
                SharedAlignedBuffer pdiffdata(drtd.get_length(), m_sector_size);

                if(!readDRTDFromBio(diffFileBIo, pdiffdata.Get(), 
                                    drtd.get_file_offset(), drtd.get_length()))
                {
                    rv = false;
                    break;
                }

                // we will take a seperate code path for multi sparse file scenario
                if(m_ismultisparsefile)
                {
					if (!write_sync_drtd_to_multisparse_volume(drtd, pdiffdata.Get(), pdiffdata.Size()))
                    {
                        rv = false;
                        break;
                    }

                    // rest of code is for physical volume and single virtual volume case
                    continue;
                } 

                if(m_isvirtualvolume)
                {

#ifdef SV_WINDOWS

                    // for single sparse virtual volumes 
                    // writes are now happening directly to sparse file instead of going through
                    // volpack driver
                    // accordingly, even punching of hole in case of all data being zeroes
                    // is moved to userspace

                    if((!m_volpackdriver_available) && m_punch_hole_supported && m_sparse_enabled &&
                       all_zeroes(pdiffdata.Get(),drtd.get_length()))
                    {
                        if(!punch_hole_sparsefile(m_volguid,
                                                  drtd.get_volume_offset(),
                                                  drtd.get_length()))
                        {
                            if(m_punch_hole_supported)
                            {
                                rv = false;
                                break;
                            }
                        }
                        else
                        {
                            if(m_checkDI)
                            {
                                DiffChecksum checksum;
                                checksum.length = drtd.get_length();
                                checksum.offset = drtd.get_volume_offset();
                                ComputeHash(pdiffdata.Get(), drtd.get_length(), checksum.digest);
                                m_checksums.checksums.push_back(checksum);
                            }
	
                            // continue with next drtd, this drtd is all zeroes
                            // and hole is punched
                            continue;
                        }
                    }

#endif // punch hole support for virtual volume in windows
                } 

                // rest of this code is for 
                //  1) Physical volume
                //  2) windows: single sparse virtual volume - non zero data
                //  3) Unix: single sparse virtual volume - both zero and non-zero data
                // 
                //  note: windows single sparse virtual volume zero data 
                //        and multi sparse virtual volume both zero and non-zero data
                //        is covered earlier above
                //


#ifdef SV_WINDOWS
                // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
                // in general this should not happen as we can make sure that sync hash
                // compare block size is always greater then FST_SCTR_OFFSET_511
                if (m_hideBeforeApplyingSyncData && (0 == drtd.get_volume_offset()) && (drtd.get_length() >= FST_SCTR_OFFSET_511))
                {
					HideBeforeApplyingSyncData((char *)(pdiffdata.Get()), pdiffdata.Size());
                }
#endif


                if(m_checkDI)
                {
                    DiffChecksum checksum;
                    checksum.length = drtd.get_length();
                    checksum.offset = drtd.get_volume_offset();
                    ComputeHash(pdiffdata.Get(), drtd.get_length(), checksum.digest);
                    m_checksums.checksums.push_back(checksum);
                }

                if(!write_sync_drtd_to_volume(pdiffdata.Get(), drtd))
                {
                    rv = false;
                    break;
                }
            }

        } while (0);

        if(!m_ismultisparsefile)
        {
#ifndef SV_WINDOWS
            if(!m_useUnBufferedIo)
            {
                if(!flush_io(m_volhandle, volumename))
                {
                    m_error = true;
                    rv = false;
                }
            }
            issue_pagecache_advise(m_volhandle, nonoverlapping_batch_start, nonoverlapping_batch_end, INM_POSIX_FADV_DONTNEED);
#endif
        }
        else
        {
            if(!close_syncwrite_handles())
                rv = false;
        }

        diffFileBIo.Close();
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


// =====================================================
// 
//   Vsnap map updates:
//    updatevsnaps  
//
// =====================================================

/*
 * FUNCTION NAME :  CDPV2Writer::updatevsnaps_v1
 *
 * DESCRIPTION : As the diffs are applied, virtual volume driver is informed to
 *               update the vsnap map files
 *
 * INPUT PARAMETERS : volumename - target volume name from which the vsnaps are created
 *                    cdp_datafile - retention data file name
 *                    metadata - the differential data applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *    1.  Open virtual volume driver
 *    2.  for each change in current differential,
 *        issue a map update request to the driver
 *    3.  Close the virtual volume driver
 *
 * return value : true if vsnap updation successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevsnaps_v1(const std::string & volumename, 
                                  const std::string & cdp_data_filepath,
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;
    std::string vsnap_parentvol = volumename;
    std::string cdp_filename = "";
    FormatVolumeName(vsnap_parentvol);
    bool cow = false;
    int idx = 0;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(!m_vsnapdriver_available)
            {
                rv = true;
                break;
            }

            if(!m_volpackdriver_available && m_isvirtualvolume)
            {
                vsnap_parentvol = m_volguid;
            }
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }

            if(!m_cdpenabled)
            {
                cow = true;
			}
			else
            {
                cdp_filename = basename_r(cdp_data_filepath.c_str());
                cow = false;
            }


            if(strnicmp(vsnap_parentvol.c_str(), "\\\\.\\", 4) == 0)
                idx = 4;
#if SV_WINDOWS
            vsnap_parentvol[idx] = toupper(vsnap_parentvol[idx]);
#endif

            for ( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter )
            {
                cdp_drtdv2_t drtd = *drtdsIter;


                if(IssueMapUpdateIoctlToVVDriver(m_vsnapctrldev, 
                                                 (const char *)(vsnap_parentvol.c_str() + idx),
                                                 cdp_filename.c_str(),
                                                 drtd.get_length(),
                                                 drtd.get_volume_offset(),
                                                 drtd.get_file_offset(),
                                                 cow) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "vsnap map update for %s failed.\n",
                                volumename.c_str());
                    rv = false;
                    break;
                }
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}



// ==========================================================
//   
//  DI checks
//   calculate_checksums_from_volume
//    issue_pagecache_advise
//    ComputeHash
//   WriteChecksums
// 
// ==========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::calculate_checksums_from_volume
 *
 * DESCRIPTION : reads the checksums from the volume for nonoverlapping_drtds
 *
 * INPUT PARAMETERS : volumename - volume from which checksums will be read
 *					 filename - filename for which checksums will be read
 *					 nonoverlapping_drtds - change information for which the checksums will be read
 *			
 *                    
 * OUTPUT PARAMETERS : None
 *
 * NOTES : m_checksums will contain the offset, length, checksum from diff file till this point.
 *		  For all the checksums stored in m_checksums we calculate the checksum read from volume
 * 
 * return value :true - if success
 *				false - otherwise
 */
bool CDPV2Writer::calculate_checksums_from_volume( std::string const & volumename, 
                                                   std::string const & filename, 
                                                   const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                                   const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;  

    std::string vol_guid = volumename;
    BasicIo volumeBIo;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s and %s\n",FUNCTION_NAME, volumename.c_str(), filename.c_str());

    try
    {
        do
        {

            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }

            if(!m_ismultisparsefile )
            {
                BasicIo::BioOpenMode volume_mode =
                    (BasicIo::BioRWExisitng | 
                     BasicIo::BioShareRead | 
                     BasicIo::BioShareWrite | 
                     BasicIo::BioShareDelete);

                if(m_useUnBufferedIo)
                {
                    setbiodirect(volume_mode);
                }

                FormatVolumeNameToGuid(vol_guid);
                ConfigureVxAgent & vxAgent = m_Configurator->getVxAgent();
                volumeBIo.EnableRetry(CDPUtil::QuitRequested,
                                      vxAgent.getVolumeRetries(),
                                      vxAgent.getVolumeRetryDelay());

                volumeBIo.Open(vol_guid, volume_mode);

                if(!volumeBIo.Good())
                {
                    DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                                volumename.c_str(), ACE_OS::last_error());
                    rv = false;
                    break;
                }

                (void)issue_pagecache_advise(volumeBIo.Handle(), nonoverlapping_batch_start,nonoverlapping_batch_end);

            }

            std::vector<DiffChecksum>::iterator cksumIter = m_checksums.checksums.begin();
            std::vector<DiffChecksum>::iterator cksumEnd = m_checksums.checksums.end();

            //	skip past the checksums that have been processed
            //	
            //	e.g For the offsets - 10,20,10
            //
            //	1st iteration - m_checksums will contain			- 10,20
            //					nonoverlapping_drtds will contain	- 10,20
            //
            //	2nd iteration - m_checksums will contain			- 10,20,10
            //					nonoverlapping_drtds will contain	- 10
            //	
            //	as the checksums(read from volume) for 10,20 is already processed and stored in m_checksums
            //	we skip past these two

            cksumIter += m_checksums.volume_cksums_count;
            for( ; cksumIter != cksumEnd ; ++cksumIter)
            {
                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pvoldata(cksumIter ->length, m_sector_size);
                if(!m_ismultisparsefile )
                {
                    if(!readDRTDFromBio(volumeBIo, pvoldata.Get(),cksumIter ->offset, cksumIter ->length))
                    {
                        rv = false;
                        break;
                    }
                }
                else
                {
                    if(!read_sync_drtd_from_multisparsefile(pvoldata.Get(),cksumIter ->offset, cksumIter ->length))
                    {
                        rv = false;
                        break;
                    }
                }

                ComputeHash(pvoldata.Get(), cksumIter ->length, cksumIter->volumedigest);
                assert(!(memcmp(cksumIter->digest, cksumIter->volumedigest, INM_MD5TEXTSIGLEN/2)));
                if(memcmp(cksumIter->digest, cksumIter->volumedigest, INM_MD5TEXTSIGLEN/2))
                {
                    stringstream ostr;
                    ostr << "issue with volume apply\n"
                         << " offset: "
                         << cksumIter ->offset
                         << "\n length: "
                         << cksumIter ->length
                         << "\n incoming checksum: ";

                    for(int i = 0; i < INM_MD5TEXTSIGLEN/2; ++i)
                        ostr << std::hex << (int)(cksumIter->digest[i]);

                    ostr << "\n computed checkum: ";
                    for(int i = 0; i < INM_MD5TEXTSIGLEN/2; ++i)
                        ostr << std::hex << (int)(cksumIter->volumedigest[i]);

                    ostr << "\n";
                    DebugPrintf(SV_LOG_ERROR, "%s\n", ostr.str().c_str());

                    rv = false;
                    break;
                }
                m_checksums.volume_cksums_count +=1;
            }

        } while (0);

        volumeBIo.Close();
    }

    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s and %s\n",FUNCTION_NAME, volumename.c_str(), filename.c_str());
    return rv;
}

bool CDPV2Writer::issue_pagecache_advise(ACE_HANDLE & vol_handle,
                                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                                         int advise)
{

    bool rv = true;  



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {

        cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;

        for( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;

            if(posix_fadvise_wrapper(vol_handle,drtd.get_volume_offset(),
                                     drtd.get_length(), advise))
            {
                DebugPrintf(SV_LOG_ERROR, "posix_fadvise failed for %s\n",
                            m_volumename.c_str());
                rv = false;
                break;
            }
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);


    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::ComputeHash
 *
 * DESCRIPTION : computes the checksum of the input buffer
 *
 * INPUT PARAMETERS : buffer - data for which checksum needs to be computed
 *					 length - length of the input buffer
 *			
 *                    
 * OUTPUT PARAMETERS : digest - MD5 checksum of the input buffer
 *
 * 
 * return value :None
 */
void CDPV2Writer::ComputeHash(char const* buffer, SV_LONGLONG length, unsigned char digest[INM_MD5TEXTSIGLEN/2])
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)buffer, length);
    INM_MD5Final(digest, &ctx);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME :  CDPV2Writer::WriteChecksums
 *
 * DESCRIPTION : writes the checksums to {target Volume Name}_{replication state}.txt
 *
 * INPUT PARAMETERS : volumename - volume for which checksums will be logged
 *					 filename - filename for which checksums will be logged
 *			
 *                    
 * OUTPUT PARAMETERS : None
 *
 * 
 * return value :true - if success
 *				false - otherwise
 */
bool CDPV2Writer::WriteChecksums(std::string const & volumename, std::string const & filename)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s and %s\n", FUNCTION_NAME, volumename.c_str(), filename.c_str());

    bool rv = true;

    try
    {
        do 
        {

            ACE_Guard<ACE_Recursive_Thread_Mutex> guard(cksum_lock);

            if(!guard.locked())
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "\n%s encountered error (%d) (%s) while trying to acquire lock ",
                            FUNCTION_NAME,ACE_OS::last_error(),
                            ACE_OS::strerror(ACE_OS::last_error()));
                rv = false;
                break;
            }

            std::string volume = volumename;

            replace_nonsupported_chars(volume);


            std::string checksumFileName = m_targetCheckSumDir;
            checksumFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            checksumFileName += volume;
            checksumFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;

            if(SVMakeSureDirectoryPathExists(checksumFileName.c_str()).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, checksumFileName.c_str());
                rv = false;
                break;
            }

            checksumFileName += isInDiffSync() ? "diffsync.txt" : "resync.txt";

            // PR#10815: Long Path support
            std::ofstream out(getLongPathName(checksumFileName.c_str()).c_str(), std::ios::app);


            if(!out) 
            {
                DebugPrintf(SV_LOG_ERROR,
                            "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                            FUNCTION_NAME, checksumFileName.c_str(), LINE_NO,FILE_NAME);
                rv = false;
                break;
            }

            out << m_checksums.filename << std::endl;

            if(!out)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed writing diff filename %s to %s.\n",
                            m_checksums.filename.c_str(),
                            checksumFileName.c_str());
                rv = false;
                break;
            }

            //diffsync file name
            //{offset,length,checksum of the data received, checksum of data read from target
            //volume after writing} 

            std::vector<DiffChecksum>::const_iterator cksumIter = m_checksums.checksums.begin();
            std::vector<DiffChecksum>::const_iterator cksumEnd = m_checksums.checksums.end();
            for( ; cksumIter != cksumEnd; ++cksumIter)
            {
                DiffChecksum cksum = *cksumIter;

                out << std::dec << cksum.offset << "," << cksum.length << ",";   

                for(int i = 0; i < INM_MD5TEXTSIGLEN/2; ++i)
                    out << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (int)cksum.digest[i];

                if(m_checksums.volume_cksums_count)
                {
                    out << ",";

                    for(int i = 0; i < INM_MD5TEXTSIGLEN/2; ++i)
                        out << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (int)cksum.volumedigest[i];
                }
                out << std::endl;
            }

        } while (0);

        m_checksums.clear();


    }
    catch ( ContextualException & ce )
    {
        DebugPrintf(SV_LOG_DEBUG, "Caught exception %s in %s\n", ce.what(), FUNCTION_NAME);
        rv = false;
    }
    catch ( std::exception & e )
    {
        DebugPrintf(SV_LOG_DEBUG, "Caught exception %s in %s\n", e.what(), FUNCTION_NAME);
        rv = false;
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_DEBUG, "Caught unknown exception in %s\n", FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s and %s\n", FUNCTION_NAME, volumename.c_str(), filename.c_str());
    return rv;
}

// ==================================================
// utility routines for performance measurment
//  getPerfTime
//  getPerfTimeDiff
//  WritePerfCounters
// ==================================================

/*
 * FUNCTION NAME :  CDPV2Writer::getPerfTime
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *					 
 *			
 *                    
 * OUTPUT PARAMETERS : 
 *
 * 
 * return value :
 *				
 */

void CDPV2Writer::getPerfTime(struct PerfTime& perfTime)
{
#ifdef WIN32
    QueryPerformanceCounter(&(perfTime.perfCount));
#else
    perfTime.perfCount = ACE_OS::gettimeofday();
#endif
}

/*
 * FUNCTION NAME :  CDPV2Writer::getPerfTimeDiff
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS :
 *					 
 *			
 *                    
 * OUTPUT PARAMETERS : 
 *
 * 
 * return value :true - if success
 *				false - otherwise
 */
double CDPV2Writer::getPerfTimeDiff(struct PerfTime& perfStartTime, struct PerfTime& perfEndTime)
{
#ifdef WIN32
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    return ((perfEndTime.perfCount.QuadPart - perfStartTime.perfCount.QuadPart) / (double)perfFreq.QuadPart);
#else
    ACE_Time_Value timeDiff = perfEndTime.perfCount - perfStartTime.perfCount;
    return (timeDiff.sec() + timeDiff.usec()/(double)1000000);
#endif
}

/*
 * FUNCTION NAME :  CDPV2Writer::WritePerfCounters
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *					 
 *			
 *                    
 * OUTPUT PARAMETERS : 
 *
 * 
 * return value :true - if success
 *				false - otherwise
 */
bool CDPV2Writer::WriteProfilingData(std::string const & volumename)
{
    DebugPrintf(SV_LOG_DEBUG, 
		"ENTERED %s volume:%s\n", FUNCTION_NAME, volumename.c_str());

    bool rv = true;
    SV_UINT cdp_version = get_cdp_version(); 

    try
    {
        do 
        {

            if(!m_printPerf && 
               ((cdp_version != CDP_ENABLED_V3) || (!m_profile_source_iopattern && !m_profile_volread && !m_profile_volwrite && !m_profile_cdpwrite))
               )
            {
                rv = true;
                break;
            }

            ACE_Guard<ACE_Recursive_Thread_Mutex> guard(perf_lock);

            if(!guard.locked())
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "\n%s encountered error (%d) (%s) while trying to acquire lock ",
                            FUNCTION_NAME,ACE_OS::last_error(),
                            ACE_OS::strerror(ACE_OS::last_error()));
                rv = false;
                break;
            }

            std::string volume = volumename;
            replace_nonsupported_chars(volume);

            std::string perfFilePath = m_installPath;
            std::string srcstats, volreadstats, volwritestats, cdpwritestats;

            perfFilePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            perfFilePath += Perf_Folder;
            perfFilePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            perfFilePath += volume;
            perfFilePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;


            if(SVMakeSureDirectoryPathExists(perfFilePath.c_str()).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, perfFilePath.c_str());
                rv = false;
                break;
            }

            srcstats = perfFilePath + "srciostats.csv";
            volreadstats  = perfFilePath + "volreadstats.csv";
            volwritestats = perfFilePath + "volwritestats.csv";
            cdpwritestats = perfFilePath + "cdpwritestats.csv";

            perfFilePath += isInDiffSync() ? "diffsync.csv" : "resync.csv";

            if(m_printPerf && !write_perf_stats(perfFilePath))
                rv = false;

            if(cdp_version != CDP_ENABLED_V3)
            {
                rv = true;
                break;
            }

            if(m_profile_source_iopattern && !write_src_profile_stats(srcstats))
                rv = false;

            if(m_profile_volread && !write_volread_profile_stats(volreadstats))
                rv = false;

            if(m_profile_volwrite && !write_volwrite_profile_stats(volwritestats))
                rv = false;

            if(m_profile_cdpwrite && !write_cdpwrite_profile_stats(cdpwritestats))
                rv = false;

        } while (0);
    }
    catch ( ContextualException & ce )
    {
        DebugPrintf(SV_LOG_DEBUG, "Caught exception %s in %s\n", ce.what(), FUNCTION_NAME);
        rv = false;
    }
    catch ( std::exception & e )
    {
        DebugPrintf(SV_LOG_DEBUG, "Caught exception %s in %s\n", e.what(), FUNCTION_NAME);
        rv = false;
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_DEBUG, "Caught unknown exception in %s\n", FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s volume:%s\n", FUNCTION_NAME, volumename.c_str());
    return rv;
}





// ========================================================
//  update cdp cow data routines
//     updatecdp
//       updatecdpv1
//       updatecdpv3
// =========================================================

bool CDPV2Writer::updatecdp(const std::string &volumename,
                            const std::string & filePath,
                            char * diff_data,
                            DiffPtr change_metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string volumelock = volumename;
    SV_UINT cdp_version = get_cdp_version();

    try
    {
        do
        {

            FormatVolumeName(volumelock);
            CDPLock volumeCdpLock(volumelock);
            if(isInDiffSync())
            {
                if(!volumeCdpLock.acquire())
                {
                    DebugPrintf(SV_LOG_ERROR, "%s%s%s","dataprotection could not acquire lock on volume.",volumename.c_str(),"\nRecovery/snapshot are in progress on the volume.\n");
                    rv = false;
                    break;
                }
                if(isActionPendingOnVolume())
                {
                    DebugPrintf(SV_LOG_DEBUG, "The volume %s is a rollback volume/unhidden volume. So skip applying data.\n",volumename.c_str());
                    rv = false;
                    break;
                }

                if(cdp_version != CDP_DISABLED_VALUE)
                {

                    ACE_stat statbuf ={0};
                    if(sv_stat(getLongPathName(m_cdpprune_filepath.c_str()).c_str(),&statbuf) == 0)
                    {
                        close_cdpdata_asyncwrite_handles();
                        close_cdpdata_syncwrite_handles();
                        CDPDatabase db(m_dbname);
                        if(db.purge_retention(m_volumename))
                        {
                            ACE_OS::unlink(m_cdpprune_filepath.c_str());
                            RetentionDirFreedUpAlert a(m_cdpdir);
                            DebugPrintf(SV_LOG_DEBUG, "%s", a.GetMessage().c_str());
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to purge retention for volume %s, retention dir %s\n",m_volumename.c_str(),m_cdpdir.c_str());
                            rv = false;
                            break;
                        }
                    }
                }
            }


            if(!m_ismultisparsefile && !m_cache_volumehandle)
            {
                if(!open_volume_handle())
                {
                    rv = false;
                    break;
                }
            }

            // if configured to reserve retention space, create reserve space if not present.
            if(m_reserved_cdpspace && (cdp_version != CDP_DISABLED_VALUE))
            {
                rv = reserve_retention_space();
            }

            if(rv)
            {
				if (isAzureTarget())
                {
					rv = ApplyResyncFiletoAzureBlob(filePath);
				}
				else if (cdp_version == CDP_DISABLED_VALUE)
				{
                    rv = updatevolume_nocdp(volumename,filePath,diff_data,change_metadata);
				}
				else if (cdp_version == CDP_ENABLED_V1)
                {
                    rv = updatecdpv1(volumename,filePath,diff_data,change_metadata);
				}
				else if (cdp_version == CDP_ENABLED_V3)
                {
                    rv = updatecdpv3(volumename,filePath,diff_data,change_metadata);
				}
				else
                {
                    DebugPrintf(SV_LOG_ERROR, "updatecdp failed - recieved unsupported cdp options.\n");
                    rv = false;
                }
            }

            if(rv && m_profile_source_iopattern)
                profile_source_io(change_metadata);

        } while (0);

        if(!m_ismultisparsefile && !m_cache_volumehandle){
            if(!close_volume_handle())	{
                rv = false;
            }
        }

    }
    catch (AzureBadFileException & be)
    {
        std::stringstream excmsg;
        excmsg << FUNCTION_NAME << " encountered AzureBadFileException - " << be.what();
        excmsg << " - Sync file: " << filePath << " for target volume: " << m_volumename << " is corrupted cannot apply";
        throw CorruptResyncFileException(excmsg.str());
    }
    catch ( ContextualException& ce )
    {
        if(!m_ismultisparsefile && !m_cache_volumehandle)
            close_volume_handle();

        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        if(!m_ismultisparsefile && !m_cache_volumehandle)
            close_volume_handle();

        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        if(!m_ismultisparsefile && !m_cache_volumehandle)
            close_volume_handle();

        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::updatecdpv1
 *
 * DESCRIPTION : Updates the retention database and the retention data files
 *
 *
 * INPUT PARAMETERS : volumename - target volume name from which COW will be read
 *                    filePath - absolute path to diff file in cache dir
 *                    change_metadata - metadata of the diff file we are processing
 * 
 * OUTPUT PARAMETERS : cdp_metadata - metadata of differential in structure format
 *
 * ALGORITHM : 
 * 
 * cdp enabled:
 *            get inode entry
 *            get last applied information
 *            if [ last file applied == file being applied ]
 *                    verify inode entry return contains the last cdp data file
 *                    baseoffset to be got from lastapplied info
 *                    changesapplied to be got from lastapplied info
 *                    mark header already written
 *            else
 *                    baseoffset to be got from inode entry
 *                    header not written
 *
 *           fill cdp metadata
 *
 *            if (header to be written)
 *            update header bytes
 *            update lastapplied info
 *
 *            end: cdp enabled
 *
 *            while drtds pending
 *                  get next non overlapping set
 *                  if cdp enabled: 
 *                        update cow data for the set
 *                        update last applied info
 *                  update vsnaps
 *                  update volume 
 *
 * return value : true if updation successfull and false otherwise
 *
 */
bool CDPV2Writer::updatecdpv1(const std::string &volumename,
                              const std::string & filePath,
                              char * diff_data,
                              DiffPtr change_metadata)
{
    bool rv = true;

    // inode entry for the file to be applied
    CDPV1Inode inode;


    // last applied entry
    std::string fileapplied;
    SV_OFFSET_TYPE baseoffset =0;
    SV_UINT changesappliedinlastrun =0;
    SV_UINT changesapplied = 0;

    // file being applied
    std::string fileBeingApplied;
    std::string fileBeingAppliedBaseName;


    std::string cdp_data_filepath;
    bool cdp_hdr_alreadwritten = false;
    SV_ULONG cdp_hdr_len = 0;
    SV_UINT  skip_bytes = 0;
    SV_OFFSET_TYPE drtdStartOffset = 0;
    SV_ULONG cdp_data_len = 0;
    SV_UINT batchsize = 0;


    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
        do
        {



            PerfTime dbop1_start_time;
            getPerfTime(dbop1_start_time);

            fileBeingApplied = filePath;
            std::string::size_type idx = fileBeingApplied.rfind(".gz");
            if(std::string::npos != idx && (fileBeingApplied.length() - idx) == 3)
                fileBeingApplied.erase(idx, fileBeingApplied.length());
            fileBeingAppliedBaseName = basename_r(fileBeingApplied.c_str());

            // 
            // step 0:
            // verify the file version
            if(!verify_file_version(fileBeingAppliedBaseName,change_metadata))
            {
                rv = false;
                break;
            }

            //
            // step 1:
            // 
            // verify we are processing
            // files in correct order
            // of increasing timestamps
            bool in_order = true;

            in_order = verify_increasing_timestamp(fileBeingAppliedBaseName,change_metadata);

            if(!in_order)  
            {
                if((!m_allowOutOfOrderTS || !m_allowOutOfOrderSeq))
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "Differentials will not be applied on %s due to files recieved out of order\n",
                                m_volumename.c_str());
                    CDPUtil::QuitRequested(180);
                    rv = false;
                    break;
                }

                if(CDPUtil::QuitRequested())
                {
                    rv = false;
                    break;
                }
            }

            //
            //step 2:
            //get last retention transaction information
            //
            // Read the retention transaction file
            //  
            // We need not read transaction file per diff as we already have this
            // information in the inmemory structure(m_lastCDPtxn). We just read it
            // while instantiating the cdpv2writer for each invocation of
            // dataprotection. This way we get considerable amount of performance
            // improvement
            //

            //
            // is this diff partially applied
            // in previous invocation?
            bool partiallyApplied = false;

            if(isInDiffSync())
            {
                if(!isPartiallyApplied(fileBeingAppliedBaseName, m_lastCDPtxn, partiallyApplied))
                {
                    rv = false;
                    break;
                }
            }

            //
            // step 3:
            // if it is resync file or
            // if the timestamp is out of order wrt retention store 
            // adjust the timestamps
            // 
            if(!adjust_timestamp(change_metadata, m_lastCDPtxn))
            {
                rv = false;
                break;
            }

            CDPV1Database db(m_dbname);

            if(!db.get_cdp_inode(filePath, partiallyApplied, m_recoveryaccuracy, change_metadata, 
                                 m_preallocate, inode, isInDiffSync()))
            {
                rv = false;
                break;
            }

            if(!get_partial_transaction_information(fileapplied, 
                                                    changesappliedinlastrun))
            {
                rv = false;
                break;
            }

            baseoffset = inode.c_logicalsize;

            if (fileapplied == fileBeingAppliedBaseName)
            {
                cdp_hdr_alreadwritten = true;
            }
            else
            {
                cdp_hdr_alreadwritten = false;
            }

            PerfTime dbop1_end_time;
            getPerfTime(dbop1_end_time);
            m_dboperation_time += getPerfTimeDiff(dbop1_start_time, dbop1_end_time);

            PerfTime readmd_start_time;
            getPerfTime(readmd_start_time);

            DiffPtr cdp_metadata(new (nothrow) Differential);
            if(!cdp_metadata)
            {
                DebugPrintf(SV_LOG_ERROR, "memory allocation for %s failed.\n",
                            filePath.c_str());
                rv = false;
                break;
            }

            if(!fill_cdp_metadata_v1(change_metadata, cdp_metadata, baseoffset, 
                                     cdp_hdr_len, skip_bytes, drtdStartOffset, cdp_data_len))
            {
                rv = false;
                break;
            }

            if(!isInDiffSync())
            {
                cdp_metadata ->StartTime(inode.c_endtime);
                cdp_metadata ->StartTimeSequenceNumber(inode.c_endseq);
                cdp_metadata ->EndTime(inode.c_endtime);
                cdp_metadata ->EndTimeSequenceNumber(inode.c_endseq);
                cdp_metadata -> SetContinuationId(1);
            }

            cdp_data_filepath = (inode.c_datadir + ACE_DIRECTORY_SEPARATOR_CHAR_A + inode.c_datafile);

            if(m_cache_cdpfile_handles)
            {
                if(!m_cdpdata_syncwrite_handles.empty() && 
                   m_cdpdata_syncwrite_handles.find(cdp_data_filepath) == m_cdpdata_syncwrite_handles.end())
                {
                    close_cdpdata_syncwrite_handles();
                }
            }

            if(!cdp_hdr_alreadwritten)
            {

                if(!write_cdp_header_v1(cdp_data_filepath, 
					cdp_metadata,                        
					baseoffset,
					cdp_hdr_len,
					skip_bytes,
					drtdStartOffset))
                {
                    rv = false;
                    break;
                }

                if(!update_partial_transaction_information(fileBeingAppliedBaseName, 
                                                           changesapplied))
                {
                    rv = false;
                    break;
                }
            }

            PerfTime readmd_end_time;
            getPerfTime(readmd_end_time);
            m_metadata_time = getPerfTimeDiff(readmd_start_time, readmd_end_time);


            cdp_drtdv2s_constiter_t drtdsIter = change_metadata -> DrtdsBegin();
            cdp_drtdv2s_constiter_t cdpdrtdsIter = cdp_metadata -> DrtdsBegin();

            cdp_drtdv2s_constiter_t nonoverlapping_changes_begin;
            cdp_drtdv2s_constiter_t nonoverlapping_changes_end;
            cdp_drtdv2s_constiter_t nonoverlapping_cdp_drtds_begin;
            cdp_drtdv2s_constiter_t nonoverlapping_cdp_drtds_end;


            if(m_checkDI)
            {
                m_checksums.clear();
                m_checksums.filename = filePath;
            }

            while(drtdsIter != change_metadata -> DrtdsEnd())
            {
                /*PerfTime cow_start_time;
                  getPerfTime(cow_start_time);*/

                if(!get_next_nonoverlapping_batch(drtdsIter,
                                                  change_metadata -> DrtdsEnd(), 
                                                  nonoverlapping_changes_begin,
                                                  nonoverlapping_changes_end))
                {
                    rv = false;
                    break;
                }

                drtdsIter = nonoverlapping_changes_end;
                batchsize = nonoverlapping_changes_end - nonoverlapping_changes_begin;
                nonoverlapping_cdp_drtds_begin = cdpdrtdsIter;
                cdpdrtdsIter += batchsize;
                nonoverlapping_cdp_drtds_end = cdpdrtdsIter;		

                changesapplied += batchsize;
                if(changesapplied  > changesappliedinlastrun)
                {
                    CDPV1Database db(m_dbname);
                    if(m_useAsyncIOs)
                    {
                        if(!update_cow_data_asynch_v1(volumename, 
                                                      cdp_data_filepath,
                                                      nonoverlapping_cdp_drtds_begin,
                                                      nonoverlapping_cdp_drtds_end))
                        {
                            if(!isInDiffSync())
                            {
                                std::stringstream l_stderr;
                                l_stderr << "This message is only for information and does not "
                                    "require any user action. Truncating retention file "
                                         << cdp_data_filepath << " to rollback partial changes.\n";
                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                // PR#10815: Long Path support
                                if(ACE_OS::truncate(getLongPathName(cdp_data_filepath.c_str()).c_str(), 0) != 0)
                                {
                                    DebugPrintf(SV_LOG_DEBUG, "Failed to truncate %s. error no:%d\n",
                                                cdp_data_filepath.c_str(), ACE_OS::last_error());
                                }
                            }
                            rv = false;
                            break;
                        }
					}
					else
                    {
                        if(!update_cow_data_synch_v1(volumename, 
                                                     cdp_data_filepath,
                                                     nonoverlapping_cdp_drtds_begin,
                                                     nonoverlapping_cdp_drtds_end))
                        {
                            if(!isInDiffSync())
                            {
                                std::stringstream l_stderr;
                                l_stderr << "This message is only for information and does not "
                                    "require any user action. Truncating retention file "
                                         << cdp_data_filepath << " to rollback partial changes.\n";
                                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                                // PR#10815: Long Path support
                                if(ACE_OS::truncate(getLongPathName(cdp_data_filepath.c_str()).c_str(), 0) != 0)
                                {
                                    DebugPrintf(SV_LOG_DEBUG, "Failed to truncate %s. error no:%d\n",
                                                cdp_data_filepath.c_str(), ACE_OS::last_error());
                                }
                            }
                            rv = false;
                            break;
                        }
                    }

                    /*PerfTime cow_end_time;
                      getPerfTime(cow_end_time);
                      m_cowwrite_time += getPerfTimeDiff(cow_start_time, cow_end_time);*/


                    PerfTime vsnapop_start_time;
                    getPerfTime(vsnapop_start_time);

                    updatevsnaps_v1(volumename, cdp_data_filepath,nonoverlapping_cdp_drtds_begin,nonoverlapping_cdp_drtds_end);
                    PerfTime vsnapop_end_time;
                    getPerfTime(vsnapop_end_time);
                    m_vsnapupdate_time +=  getPerfTimeDiff(vsnapop_start_time, vsnapop_end_time);

                    if(!update_partial_transaction_information(fileBeingAppliedBaseName, 
                                                               changesapplied))
                    {
                        rv = false;
                        break;
                    }
                }


                PerfTime volwrite_start_time;
                getPerfTime(volwrite_start_time);
                if(!updatevolume(volumename,filePath,diff_data,
                                 nonoverlapping_changes_begin, nonoverlapping_changes_end))
                {
                    rv = false;
                    break;
                }

                PerfTime volwrite_end_time;
                getPerfTime(volwrite_end_time);
                m_volwrite_time += getPerfTimeDiff(volwrite_start_time, volwrite_end_time);

                if(m_checkDI && m_verifyDI)
                {
                    if(!calculate_checksums_from_volume(volumename, filePath, 
                                                        nonoverlapping_changes_begin, nonoverlapping_changes_end))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s Unable to calculate checksums for volume: %s Data File: %s\n",
                                    FUNCTION_NAME, volumename.c_str(), filePath.c_str());
                        rv = false;
                        break;
                    }
                }
            }

            if(rv && m_checkDI)
            {
                if(!WriteChecksums(volumename, filePath))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s Unable to write checksums for volume: %s Data File: %s\n",
                                FUNCTION_NAME, volumename.c_str(), filePath.c_str());
                    rv = false;
                    break;
                }
            }

            if(rv)
            {
                PerfTime dbop2_start_time;
                getPerfTime(dbop2_start_time);


                CDPV1Database db(m_dbname);
                if(!db.update_cdp(filePath,
                                  m_recoveryaccuracy,
                                  change_metadata,
                                  cdp_metadata,
                                  inode,
                                  isInDiffSync()))
                {
                    rv = false;
                    break;
                }

                if(!update_end_timestamp_information(cdp_metadata->EndTimeSequenceNumber(),
                                                     cdp_metadata->endtime(), cdp_metadata->ContinuationId()))
                {
                    rv = false;
                    break;
                }


                std::string none = "none";
                memset(&m_lastCDPtxn.partialTransaction, 0, sizeof(m_lastCDPtxn.partialTransaction));
                inm_memcpy_s(m_lastCDPtxn.partialTransaction.diffFileApplied, sizeof(m_lastCDPtxn.partialTransaction.diffFileApplied), none.c_str(), none.length() + 1);

                if(!CDPUtil::update_last_retentiontxn(m_cdpdir, m_lastCDPtxn))
                {
                    rv = false;
                    break;
                }

                PerfTime dbop2_end_time;
                getPerfTime(dbop2_end_time);
                m_dboperation_time += getPerfTimeDiff(dbop2_start_time, dbop2_end_time);
            }

        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}







// =========================================================
//   cdpv1 routines
//  
//  1) Metadata
//     fill_cdp_metadata_v1 (create metadata to memory)
//     fill_cdp_header_v1  
//     write_cdp_header_v1
//     write_cow_data
// =========================================================

/*
 * FUNCTION NAME :  CDPV2Writer::fill_cdp_metadata_v1
 *
 * DESCRIPTION : Make changes to metadata so that it will be consistent when
 *               written to data file
 *
 * INPUT PARAMETERS : change_metadata - metadata of the diff file we are processing
 *                    baseoffset - end offset of the cdp data file 
 * 
 * OUTPUT PARAMETERS : 
 * 
 *      cdp_metadata - contains the auxilary information which may be
 *                     written to cdp data file, but is in a structural format
 *      cdp_hdr_len  - lenght of the auxillary buffer. This includes Header & Skip Bytes.
 *      skip_bytes   - no. of bytes to skip for alignment
 *      drtdStartOffset - offset in the cdp data file where DRTDs will be written 
 *      cdp_datalen  - length of the data that will be written to the cdp data file.
 *
 * return value : true if changes to metadata are successfull and false otherwise
 *
 */
bool CDPV2Writer::fill_cdp_metadata_v1(DiffPtr change_metadata, 
                                       DiffPtr & cdp_metadata, 
                                       SV_ULONGLONG baseoffset, 
                                       SV_ULONG & cdp_hdr_len,
                                       SV_UINT & skip_bytes,
                                       SV_OFFSET_TYPE & drtdStartOffset,
                                       SV_ULONG & cdp_datalen)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SV_OFFSET_TYPE   dataStartOffset = 0;
    SV_OFFSET_TYPE   dataCurOffset = 0;

    try
    {
        do
        {

            cdp_hdr_len = 0;
            skip_bytes = 0;
            drtdStartOffset = 0;
            cdp_datalen = 0;

            cdp_metadata -> StartOffset(baseoffset);
            cdp_metadata -> Hdr(change_metadata ->getHdr());

            cdp_hdr_len += ( SVD_PREFIX_SIZE + SVD_HEADER2_SIZE);

            cdp_metadata -> StartTime(change_metadata -> getStartTime());

            cdp_hdr_len += ( SVD_PREFIX_SIZE + SVD_TIMESTAMP_V2_SIZE);


            UserTagsIterator udtsIter = change_metadata -> UserTagsBegin();
            UserTagsIterator udtsEnd  = change_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                cdp_metadata -> AddBookMark(pMarker);
                cdp_hdr_len += ( SVD_PREFIX_SIZE + pMarker ->RawLength());
            }


            cdp_drtdv2s_iter_t drtdsIter = change_metadata -> DrtdsBegin();
            cdp_drtdv2s_iter_t drtdsEnd  = change_metadata -> DrtdsEnd();

            if(drtdsIter != drtdsEnd)
            {
                SV_ULONGLONG lodc = change_metadata -> DrtdsLength();
                lodc += SVD_PREFIX_SIZE;
                lodc += ((change_metadata -> NumDrtds()) * (SVD_DIRTY_BLOCK_V2_SIZE));
                cdp_metadata -> LengthOfDataChanges(lodc);

                cdp_hdr_len += (SVD_PREFIX_SIZE + SVD_DATA_INFO_SIZE);
                drtdStartOffset = cdp_metadata -> StartOffset() + cdp_hdr_len;

                cdp_hdr_len += SVD_PREFIX_SIZE;
                cdp_hdr_len += ((change_metadata -> NumDrtds()) * (SVD_DIRTY_BLOCK_V2_SIZE));

			}
			else
            {
                cdp_metadata -> LengthOfDataChanges(0);
            }


            cdp_metadata -> EndTime(change_metadata -> getEndTime());
            cdp_hdr_len += ( SVD_PREFIX_SIZE + SVD_TIMESTAMP_V2_SIZE);
            drtdStartOffset +=  ( SVD_PREFIX_SIZE + SVD_TIMESTAMP_V2_SIZE);

            // skip bytes for sector alignment (will appear before tslc)
            cdp_hdr_len += (SVD_PREFIX_SIZE); 
            drtdStartOffset += (SVD_PREFIX_SIZE);
            skip_bytes = (m_sector_size - (cdp_hdr_len % m_sector_size));
            cdp_hdr_len += skip_bytes;
            drtdStartOffset += skip_bytes;

            cdp_datalen = change_metadata -> DrtdsLength();
            dataStartOffset = (cdp_metadata -> StartOffset() + cdp_hdr_len);
            dataCurOffset = dataStartOffset;

            for ( /* empty */ ; drtdsIter != drtdsEnd ; ++drtdsIter )
            {
                cdp_drtdv2_t change_drtd = *drtdsIter;

                cdp_drtdv2_t cdp_drtd(change_drtd.get_length(),
                                      change_drtd.get_volume_offset(),
                                      dataCurOffset,
                                      change_drtd.get_seqdelta(),
                                      change_drtd.get_timedelta(),
                                      change_drtd.get_fileid());


                cdp_metadata -> AddDRTD(cdp_drtd);
                dataCurOffset += cdp_drtd.get_length();
            }

            if(drtdsIter != drtdsEnd)
            {
                rv = false;
                break;
            }


            cdp_metadata -> EndOffset( baseoffset + cdp_hdr_len + cdp_datalen);
            cdp_metadata -> SetContinuationId (m_ctid);
            cdp_metadata -> type(change_metadata -> type());



        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPV2Writer::write_cdp_header_v1(std::string cdp_data_file, 
                                      DiffPtr & cdp_metadata,                        
                                      const SV_ULONGLONG &baseoffset,
                                      const SV_ULONG & cdp_hdr_len,
                                      const SV_UINT & skip_bytes,
                                      const SV_OFFSET_TYPE & drtdStartOffset)
{
    bool rv = true;   
    ACE_HANDLE retfile_handle = ACE_INVALID_HANDLE;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(!get_cdpdata_sync_writehandle(cdp_data_file,retfile_handle))
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no: %d\n",
                            cdp_data_file.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            if(ACE_OS::llseek(retfile_handle,baseoffset, SEEK_SET) < 0)
            {
                std::stringstream l_stderr;
                l_stderr << "Seek to offset: " << baseoffset << " failed for "
                         << cdp_data_file << ". error no: " << ACE_OS::last_error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }

            SharedAlignedBuffer cdp_header(cdp_hdr_len,m_sector_size);

            // fill cdp header
            if(!fill_cdp_header_v1(cdp_metadata, 
                                   cdp_header.Get(),
                                   baseoffset,
                                   cdp_hdr_len,
                                   skip_bytes,
                                   drtdStartOffset))
            {
                rv = false;
                break;
            }

            // write cow data to cdp data file
            if(!write_cow_data(cdp_data_file, retfile_handle, 
                               cdp_header.Get(), 
                               cdp_hdr_len))
            {
                rv = false;
                break;
            }

        } while (0);

        if(retfile_handle != ACE_INVALID_HANDLE)
        {
            if(!m_useUnBufferedIo)
            {
                if(!flush_cdpdata_syncwrite_handles())
                    rv = false;
            }
            if(!m_cache_cdpfile_handles)
            {
                close_cdpdata_syncwrite_handles();
            }
        }
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::fill_cdp_header_v1
 *
 * DESCRIPTION : Converts the cdp metadata which is in structural format to a plain
 *               metadata adding the corresponding prefixes for each field.
 *
 * INPUT PARAMETERS :
 * 
 *    cdp_metadata  -  metadata in structural format
 *    baseoffset    -  offset of the cdp data file where we start writing the
 *                     auxillary data
 *    cdp_hdr_len   -  length of the auxillary data
 *    skip_bytes    -  no. of bytes to skip for alignment
 *    drtdStartOffset - offset in the cdp data file where DRTDs will be written 
 * 
 * OUTPUT PARAMETERS : cdp_header - buffer to which the converted metadata
 *                                    will be stored
 *
 * NOTES : Format of cdp header (metadata)
 * 
 *   SVD FILE HEADER
 *   TSFC
 *   USER TAGS
 *   DATA TAG HEADER
 *   DATA INFO
 *   SKIP TAG
 *   SKIP bytes for alignment
 *   TSLC
 *   DDV3 TAG
 *   DRTD HEADER 1
 *   DRTD HEADER 2
 *   DRTD HEADER 3
 *   ...
 *   DRTD HEADER N
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::fill_cdp_header_v1(DiffPtr cdp_metadata,
                                     char * cdp_header,
                                     const SV_ULONGLONG & baseoffset,
                                     const SV_ULONG & cdp_hdr_len,
                                     const SV_UINT & skip_bytes,
                                     const SV_OFFSET_TYPE & drtdStartOffset)
{
    bool rv = true;   
    SVD_PREFIX pfx;
    SV_ULONG curpos = 0;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            // fill in svd headers
            FILL_PREFIX(pfx, SVD_TAG_HEADER_V2, 1, 0);
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            SVD_HEADER_V2 cdp_hdr = cdp_metadata -> getHdr();
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &cdp_hdr, SVD_HEADER2_SIZE);
            curpos += SVD_HEADER2_SIZE;

            // fill in tsfc
            FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2, 1, 0);
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            SVD_TIME_STAMP_V2 cdp_starttime = cdp_metadata -> getStartTime();
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &cdp_starttime, SVD_TIMESTAMP_V2_SIZE);
            curpos += SVD_TIMESTAMP_V2_SIZE;

            // fill in bookmarks
            UserTagsIterator udtsIter = cdp_metadata -> UserTagsBegin();
            UserTagsIterator udtsEnd  = cdp_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;

                FILL_PREFIX(pfx, SVD_TAG_USER, 1, pMarker ->RawLength());
				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
                curpos += SVD_PREFIX_SIZE;
				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), (const char*)pMarker->RawData(),
                       pMarker ->RawLength());
                curpos += pMarker ->RawLength();
            }


            cdp_drtdv2s_iter_t drtdsIter = cdp_metadata -> DrtdsBegin();
            cdp_drtdv2s_iter_t drtdsEnd  = cdp_metadata -> DrtdsEnd();

            if(drtdsIter != drtdsEnd)
            {
                // fill in length of data changes
                FILL_PREFIX(pfx, SVD_TAG_DATA_INFO, 1, 0);
				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
                curpos += SVD_PREFIX_SIZE;

                SVD_DATA_INFO dataInfo;
                dataInfo.drtdStartOffset = drtdStartOffset;
                dataInfo.drtdLength.QuadPart = cdp_metadata -> LengthOfDataChanges();
                dataInfo.diffEndOffset = cdp_metadata -> EndOffset();

				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), (const char*)(&dataInfo),
                       SVD_DATA_INFO_SIZE);
                curpos += SVD_DATA_INFO_SIZE;
            }

            FILL_PREFIX(pfx, SVD_TAG_SKIP, skip_bytes, 0);
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            curpos += skip_bytes;

            // fill in tslc
            FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2, 1, 0);
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            SVD_TIME_STAMP_V2 cdp_endtime = cdp_metadata -> getEndTime();
			inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &cdp_endtime, SVD_TIMESTAMP_V2_SIZE);
            curpos += SVD_TIMESTAMP_V2_SIZE;

            if(drtdsIter != drtdsEnd)
            {

                // fill in drtd count information
                FILL_PREFIX(pfx, SVD_TAG_DIRTY_BLOCK_DATA_V3, 
                            cdp_metadata -> NumDrtds() , 0);
				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), &pfx, SVD_PREFIX_SIZE);
                curpos += SVD_PREFIX_SIZE;
            }

            SVD_DIRTY_BLOCK_V2 drtdhdr;
            for ( /* empty */ ; drtdsIter != drtdsEnd ; ++drtdsIter )
            {
                cdp_drtdv2_t cdp_drtd = *drtdsIter;

                drtdhdr.ByteOffset = cdp_drtd.get_volume_offset();
                drtdhdr.Length = cdp_drtd.get_length();
                drtdhdr.uiSequenceNumberDelta = cdp_drtd.get_seqdelta();
                drtdhdr.uiTimeDelta = cdp_drtd.get_timedelta();

				inm_memcpy_s(cdp_header + curpos, (cdp_hdr_len - curpos), (const char*)&drtdhdr, SVD_DIRTY_BLOCK_V2_SIZE);
                curpos += SVD_DIRTY_BLOCK_V2_SIZE;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::write_cow_data
 *
 * DESCRIPTION : write the data from buffer to the cdp data file
 *
 * INPUT PARAMETERS : 
 *
 *     cdp_data_file - cdp data file to which the data is to be written
 *     cdp_data      - buffer containing the data which should be written to cdp data file
 *     baseoffset    - offset in the cdp data file from where data is to be written
 *     cdp_hdr_len   - length of the auxillary data
 *     cdp_data_len  - length of the cdp data buffer
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  Open the cdp data file and seek to the end.
 *     2.  Write the current differential to the data file.
 *     3.  Close file.
 *
 * return value : true if write successfull and false otherwise
 *
 */
bool CDPV2Writer::write_cow_data(const std::string & cdp_data_file, ACE_HANDLE & cdp_handle,
                                 const char * cdp_data,
                                 const SV_ULONG & cdp_buffer_len)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            SV_ULONG cbDataRemaining    = cdp_buffer_len;
            SV_ULONG cbTotalWritten = 0;
            SV_UINT cbWritten = 0;
            SV_UINT cbToWrite = 0;

            // step 1: write the data
            while ( cbDataRemaining > 0 ) 
            {
                cbToWrite = (SV_UINT) min( cbDataRemaining, (SV_ULONG) m_max_rw_size );
                if( (cbWritten = ACE_OS::write(cdp_handle,(cdp_data + cbTotalWritten), cbToWrite)) != cbToWrite )
                {
                    stringstream l_stderr;
                    l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                             << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                             << "Error during write operation\n"
                             << "Data File:" << cdp_data_file  << "\n"
                             << "Expected Write: " << cbToWrite << "\n"
                             << "Actual Write Bytes: " << cbWritten <<"\n"
                             << "Error Code: " << ACE_OS::last_error() << "\n"
                             << "Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n"
                             << USER_DEFAULT_ACTION_MSG << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    rv = false;
                    break;
                }

                cbTotalWritten += cbWritten;
                cbDataRemaining -= cbWritten;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


// =================================================
//  updatecdpv1 routines
//
//  routines to fetch non overlapping drtds:
//    get_next_nonoverlapping_batch
//    is_overlap
// =================================================

/*
 * FUNCTION NAME : get_next_nonoverlapping_batch
 *
 * DESCRIPTION : 
 *   Gets the next set of non-overlapping DRTDs
 *               
 * INPUT PARAMETERS : 
 *   drtdsIter, drtdsEnd - const iterator to DRTDs in metadata
 *                    
 * OUTPUT PARAMETERS : 
 *   drtds_to_apply - vector which contains the non-overlap DRTDs 
 *
 * ALGORITHM :
 * 
 *    1. For each change in the list of changes
 *    2. Add the change to output list based on the non-overlapping condition
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::get_next_nonoverlapping_batch(const cdp_drtdv2s_constiter_t & drtds_begin, 
                                                const cdp_drtdv2s_constiter_t & drtds_end, 
                                                cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                                cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        nonoverlapping_batch_start = drtds_begin;
        nonoverlapping_batch_end = drtds_begin;

        for( ; nonoverlapping_batch_end != drtds_end ; ++nonoverlapping_batch_end)
        {

            cdp_drtdv2_t drtd((*nonoverlapping_batch_end).get_length(),
                              (*nonoverlapping_batch_end).get_volume_offset(),
                              (*nonoverlapping_batch_end).get_file_offset(),
                              (*nonoverlapping_batch_end).get_seqdelta(),
                              (*nonoverlapping_batch_end).get_timedelta(),
                              (*nonoverlapping_batch_end).get_fileid());

            if(is_overlap(nonoverlapping_batch_start, nonoverlapping_batch_end, drtd))
            {
                break;
            }
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



/*
 * FUNCTION NAME :  CDPV2Writer::is_overlap
 *
 * DESCRIPTION : check if DRTD overlaps with any other DRTDs
 *
 * INPUT PARAMETERS : nonoverlap_drtds - vector containing no overlapping DRTDs
 *                    drtd - current DRTD which should be checked if it
 *                              overlaps with others.
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  For any DRTD in list of DRTDs overlaps with current DRTD return true
 *
 * return value : true if overlaps and false otherwise
 *
 */
bool CDPV2Writer::is_overlap(const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                             const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                             const cdp_drtdv2_t & drtd)
{
    // assume it is non overlapping to begin with
    bool rv = false;


    cdp_drtdv2s_constiter_t drtds_iter = nonoverlapping_batch_start;

    SV_ULONGLONG x_startoffset = drtd.get_volume_offset();
    SV_ULONGLONG x_endoffset = (drtd.get_volume_offset() + drtd.get_length() -1);

    for( ; drtds_iter != nonoverlapping_batch_end ; ++drtds_iter)
    {
        cdp_drtdv2_t y = *drtds_iter;
        SV_ULONGLONG y_startoffset = y.get_volume_offset() ;
        SV_ULONGLONG y_endoffset = (y.get_volume_offset()  + y.get_length() -1);

        if((x_endoffset < y_startoffset) || ( x_startoffset > y_endoffset))
        {
            // non overlapping
            continue;
		}
		else
        {
            rv = true;
            break;
        }
    }

    return rv;
}

// =====================================================
//  updatecdpv1 routines
//    
//   Asynchronous COW update:
//    update_cow_data_asynch_v1
//      get_drtds_limited_by_memory
//      issue_pagecache_advise
//      read_async_drtds_from_volume
//          initiate_next_async_read
//          handle_read_file
//      wait_for_xfr_completion
//      write_cow_date (declared earlier)
// =======================================================


bool CDPV2Writer::update_cow_data_asynch_v1(const std::string & volumename,
                                            const std::string & cdp_data_file,
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    /*
      1. open the cdp data file
      2. seek to base offset
      3. read the drtds from volume to shared aligned buffer
      4. write the buffer to cdp data file
      5. close the cdp data file
    */

    bool rv = true;
    ACE_HANDLE retfile_handle = ACE_INVALID_HANDLE;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {

        do
        {
            //__asm int 3;
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }



            PerfTime cowopenPerfStart;
            getPerfTime(cowopenPerfStart);

            if(!get_cdpdata_sync_writehandle(cdp_data_file,retfile_handle))
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no: %d\n",
                            cdp_data_file.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            if(ACE_OS::llseek(retfile_handle,(*drtdsIter).get_file_offset(), SEEK_SET) < 0)
            {
                std::stringstream l_stderr;
                l_stderr << "Seek to offset: " << (*drtdsIter).get_file_offset() << " failed for "
                         << cdp_data_file << ". error no: " << ACE_OS::last_error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            PerfTime cowopenPerfEnd;
            getPerfTime(cowopenPerfEnd);
            m_cowwrite_time += getPerfTimeDiff(cowopenPerfStart, cowopenPerfEnd);

            while(drtdsIter != nonoverlapping_batch_end)          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());

                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t drtds_to_read_begin;
                cdp_drtdv2s_constiter_t drtds_to_read_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                nonoverlapping_batch_end, 
                                                maxUsableMemory, 
                                                drtds_to_read_begin,
                                                drtds_to_read_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }


                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                // copy drtds to the sector aligned buffer(needed for writing with no buffering)
                PerfTime volreadPerfStart;
                getPerfTime(volreadPerfStart);

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, drtds_to_read_begin,drtds_to_read_end);
                }

                if(!read_async_drtds_from_volume(volumename,pcowdata.Get(), drtds_to_read_begin,drtds_to_read_end))
                {
                    rv = false;
                    break;
                }

                PerfTime volreadPerfEnd;
                getPerfTime(volreadPerfEnd);
                m_volread_time += getPerfTimeDiff(volreadPerfStart, volreadPerfEnd);

                PerfTime cowwritePerfStart;
                getPerfTime(cowwritePerfStart);

                // write cow data to cdp data file
                if(!write_cow_data(cdp_data_file, 
                                   retfile_handle,
                                   pcowdata.Get(), 
                                   memRequired))
                {
                    rv = false;
                    break;
                }


                PerfTime cowwritePerfEnd;
                getPerfTime(cowwritePerfEnd);
                m_cowwrite_time += getPerfTimeDiff(cowwritePerfStart, cowwritePerfEnd);

                drtdsIter = drtds_to_read_end;
            }

        } while (0);

        PerfTime cowclosePerfStart;
        getPerfTime(cowclosePerfStart);

        if(retfile_handle != ACE_INVALID_HANDLE)
        {
            if(!m_useUnBufferedIo)
            {
                if(!flush_cdpdata_syncwrite_handles())
                {
                    rv = false;
                }
            }
            if(!m_cache_cdpfile_handles)
            {
                close_cdpdata_syncwrite_handles();
            }
        }

        PerfTime cowclosePerfEnd;
        getPerfTime(cowclosePerfEnd);
        m_cowwrite_time += getPerfTimeDiff(cowclosePerfStart, cowclosePerfEnd);

        PerfTime volclosePerfStart;
        getPerfTime(volclosePerfStart);

        if(!close_asyncread_handles())
            rv =false;  

        PerfTime volclosePerfEnd;
        getPerfTime(volclosePerfEnd);
        m_volread_time += getPerfTimeDiff(volclosePerfStart, volclosePerfEnd);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}
/*
 * FUNCTION NAME :  CDPV2Writer::read_async_drtds_from_volume
 *
 * DESCRIPTION : Initiate asyncronous reads of non overlapping DRTDs and wait for
 *               the reads to complete
 *
 * INPUT PARAMETERS : 
 * 
 *    vol_handle - handle of the file(target volume) from which we read the
 *                 data asynchronously
 *    cow_drtds  - vector of non-overlapping DRTDs for which asynchronous read
 *                 is to be initiated
 *     
 * OUTPUT PARAMETERS : 
 *     cdp_data   - pointer to the aligned buffer to which data
 *                  is to be read
 *
 * ALGORITHM :
 * 
 *     1.  For each change in the given set of non overlapping changes, repeat step2
 *     2.  Initiate an async read from the target volume
 *     3.  Wait for the initiated reads to complete
 *
 * return value : true if read successfull and false otherwise
 *
 */
bool CDPV2Writer::read_async_drtds_from_volume(const std::string &volumename, 
                                               char * cdp_data,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_end)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_ULONG bytesRead = 0;
        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;

        for( ; drtdsIter != drtds_to_read_end ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;

            if(m_ismultisparsefile)
            {
                if(!read_async_drtd_from_multisparsefile((char *) (cdp_data + bytesRead), 
                                                         drtd.get_volume_offset(),
                                                         drtd.get_length()))
                {
                    rv = false;
                    break;
                }

			}
			else
            {
                if(!canIssueNextIo(0))
                {
                    rv = false;
                    break;
                }

                if(!initiate_next_async_read(*m_rfptr.get(), 
                                             (char *) (cdp_data + bytesRead), 
                                             drtd.get_length(),
                                             drtd.get_volume_offset()))
                {
                    rv = false;
                    break;
                }
            }

            bytesRead += drtd.get_length();
        }

        if(!wait_for_xfr_completion())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::initiate_next_async_read
 *
 * DESCRIPTION : Initiate an asynchronous read for the DRTD
 *
 * INPUT PARAMETERS : 
 *
 *      vol_handle - handle of the file(target volume) from which we read the data
 *                   asynchronously
 *      cdp_data   - pointer to the aligned buffer to which data is to be read
 *      drtdPtr    - pointer to cdp_drtdv2_t for which asynch read has to be initiated
 * 
 * OUTPUT PARAMETERS : 
 *
 *      
 *                   
 * 
 * ALGORITHM :
 * 
 *    1. Initiate a new read request 
 *    2. Increment pending IOs
 *
 * return value : true if read initiated successfull and false otherwise
 *
 */
bool CDPV2Writer::initiate_next_async_read(ACE_Asynch_Read_File & vol_handle, 
                                           char * cdp_data, 
                                           const SV_UINT &length,
                                           const SV_OFFSET_TYPE& offset)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, m_volumename.c_str(), offset, length);

    try
    {

        do
        {
            SV_ULONG bytespending = length;
            SV_ULONGLONG readoffset = offset;
            SV_ULONG bytesRead = 0;

            while(bytespending > 0)
            {
                SV_ULONG bytes_to_queue = min((SV_ULONG)m_max_rw_size, (SV_ULONG)(bytespending));
                ACE_Message_Block *mb = new ACE_Message_Block (cdp_data + bytesRead, bytes_to_queue);
                if(!mb)
                {
                    stringstream l_stderr;
                    l_stderr   << "memory allocation(message block) failed "
                               << "while initiating read from volume " << m_volumename << ".\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    m_error = true;
                    rv = false;
                    break;
                }

                incrementPendingIos();
                if(vol_handle.read(*mb,bytes_to_queue, 
                                   (unsigned int)(readoffset), 
                                   (unsigned int)(readoffset >> 32)) == -1)
                {
                    int errorcode = ACE_OS::last_error();
                    std::stringstream l_stderr;
                    l_stderr << "Asynch read request from volume " << m_volumename
                             << "at offset " << readoffset << " length : " << bytes_to_queue << " failed. error no: "
                             << errorcode << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    decrementPendingIos();
                    m_error = true;
                    rv = false;
                    break;
                }
                bytesRead += bytes_to_queue;
                readoffset += bytes_to_queue;
                bytespending -= bytes_to_queue;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, m_volumename.c_str(), offset, length);
    return rv;
}



/*
 * FUNCTION NAME :  CDPV2Writer::handle_read_file
 *
 * DESCRIPTION : Callback function which will be called once each asynch IO
 *               is completed
 *
 * INPUT PARAMETERS : result - contains the status of the requested Asynch IO
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *     1.  Release the message block allocated while initiating the async read
 *     2.  Decrement pending IOs
 *
 * return value : none
 *
 */
void CDPV2Writer::handle_read_file (const ACE_Asynch_Read_File::Result &result)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            // release message block.
            result.message_block ().release ();

            unsigned long long voloffset = 
                ((unsigned long long)(result.offset_high()) << 32) + (unsigned long long)(result.offset());

            if (result.success ())
            {

                if (result.bytes_transferred () != result.bytes_to_read())
                {
                    m_error = true;
                    stringstream l_stderr;
                    l_stderr << "async read from volume " << m_volumename << " at "
                             << voloffset
                             << " failed with error " 
                             <<  result.error()  
                             << "bytes transferred =" << result.bytes_transferred ()
                             << "bytes requested =" << result.bytes_to_read()
                             << "\n";
                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    break;
                }

			}
			else
            {
                m_error = true;
                stringstream l_stderr;
                l_stderr << "async read from volume " << m_volumename << " at "
                         << voloffset
                         << " failed with error " 
                         <<  result.error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                break;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        m_error = true;      
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        m_error = true;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    decrementPendingIos();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME :  CDPV2Writer::wait_for_xfr_completion
 *
 * DESCRIPTION : wait for the all asyncronous IOs to complete.
 *
 * INPUT PARAMETERS : none
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::wait_for_xfr_completion()
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        iocount_mutex.acquire();

        while(m_pendingios > 0)
            iocount_cv.wait();

        iocount_mutex.release();

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    if(error())
        rv = false;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::wait_for_xfr_completion
 *
 * DESCRIPTION : wait for the requested no. of asyncronous IOs to complete.
 *
 * INPUT PARAMETERS : io count to wait
 *                    
 * 
 * OUTPUT PARAMETERS : none
 *
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::wait_for_xfr_completion(const SV_UINT & iocount)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        iocount_mutex.acquire();

        unsigned long pendingios = m_pendingios;

        while(pendingios > m_pendingios + iocount)
            iocount_cv.wait();

        iocount_mutex.release();

        if(error())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// ====================================================
//  updatecdpv1 routines
//    
//  Synchronous COW update
//     update_cow_data_synch_v1
//        read_sync_drtds_from_volume
//            readDRTDFromBio
//         write_cow_data (declared earlier)
//
// =====================================================   



bool CDPV2Writer::update_cow_data_synch_v1(const std::string & volumename,
                                           const std::string & cdp_data_file,
                                           const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                           const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{
    /*
      1. open the cdp data file
      2. seek to base offset
      3. read the drtds from volume to shared aligned buffer
      4. write the buffer to cdp data file
      5. close the cdp data file
    */

    bool rv = true;  

    ACE_HANDLE retfile_handle = ACE_INVALID_HANDLE;



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }


            PerfTime cowopenPerfStart;
            getPerfTime(cowopenPerfStart);

            if(!get_cdpdata_sync_writehandle(cdp_data_file,retfile_handle))
            {
                rv = false;
                DebugPrintf(SV_LOG_ERROR,"Failed to obtain handle for %s\n",cdp_data_file.c_str());
                break;
            }

            if(ACE_OS::llseek(retfile_handle, (*drtdsIter).get_file_offset(), SEEK_SET) < 0)
            {
                std::stringstream l_stderr;
                l_stderr << "Seek to offset: " << (*drtdsIter).get_file_offset() << " failed for "
                         << cdp_data_file << ". error no: " << ACE_OS::last_error() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            PerfTime cowopenPerfEnd;
            getPerfTime(cowopenPerfEnd);
            m_cowwrite_time += getPerfTimeDiff(cowopenPerfStart, cowopenPerfEnd);

            while(drtdsIter != nonoverlapping_batch_end)          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());


                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t cow_drtds_begin;
                cdp_drtdv2s_constiter_t cow_drtds_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                nonoverlapping_batch_end, 
                                                maxUsableMemory, 
                                                cow_drtds_begin,
                                                cow_drtds_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }


                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;


                // copy drtds to the sector aligned buffer(needed for writing with no buffering)
                PerfTime volreadPerfStart;
                getPerfTime(volreadPerfStart);

                // copy drtds to the sector aligned buffer(needed for writing with no buffering)
                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, cow_drtds_begin,cow_drtds_end);
                }

                if(!read_sync_drtds_from_volume(volumename,pcowdata.Get(), cow_drtds_begin,cow_drtds_end ))
                {
                    rv = false;
                    break;
                }

                PerfTime volreadPerfEnd;
                getPerfTime(volreadPerfEnd);

                m_volread_time += getPerfTimeDiff(volreadPerfStart, volreadPerfEnd);

                PerfTime cowwritePerfStart;
                getPerfTime(cowwritePerfStart);

                // write cow data to cdp data file
                if(!write_cow_data(cdp_data_file,
                                   retfile_handle,
                                   pcowdata.Get(), 
                                   memRequired))
                {
                    rv = false;
                    break;
                }

                PerfTime cowwritePerfEnd;
                getPerfTime(cowwritePerfEnd);
                m_cowwrite_time += getPerfTimeDiff(cowwritePerfStart, cowwritePerfEnd);

                drtdsIter = cow_drtds_end;
            }

        } while (0);

        PerfTime cowclosePerfStart;
        getPerfTime(cowclosePerfStart);

        //Performance Improvement - 
        if(retfile_handle != ACE_INVALID_HANDLE)
        {
            if(!m_useUnBufferedIo)
            {
                if(!flush_cdpdata_syncwrite_handles())
                    rv = false;
            }
            if(!m_cache_cdpfile_handles)
            {
                close_cdpdata_syncwrite_handles();
            }
        }
        PerfTime cowclosePerfEnd;
        getPerfTime(cowclosePerfEnd);
        m_cowwrite_time += getPerfTimeDiff(cowclosePerfStart, cowclosePerfEnd);
        PerfTime volclosePerfStart;
        getPerfTime(volclosePerfStart);

        if(!close_syncread_handles())
            rv = false;

        PerfTime volclosePerfEnd;
        getPerfTime(volclosePerfEnd);
        m_volread_time += getPerfTimeDiff(volclosePerfStart, volclosePerfEnd);
    }

    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}




bool CDPV2Writer::read_sync_drtds_from_volume(const std::string & volumename,
                                              char * buffer,
                                              const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                              const cdp_drtdv2s_constiter_t & drtds_to_read_end)
{
    bool rv = true;

    cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;


    SV_ULONGLONG bytesRead = 0;
    for( ; drtdsIter != drtds_to_read_end; ++drtdsIter)
    {
        cdp_drtdv2_t drtd = *drtdsIter;

        if(m_ismultisparsefile)
        {
            if(!read_sync_drtd_from_multisparsefile((char *) (buffer + bytesRead), 
                                                    drtd.get_volume_offset(),
                                                    drtd.get_length()))
            {
                rv = false;
                break;
            }

		}
		else
        {

            if(!read_sync_drtd_from_volume(m_volhandle,
                                           (char *) (buffer + bytesRead),
                                           drtd.get_volume_offset(),
                                           drtd.get_length()))
            {
                rv = false;
                break;
            }
        }
        bytesRead += drtd.get_length();
    }

    return rv;
}









// ================================================================
//  resource management routines
// ================================================================

void CDPV2Writer::add_memory_allocation(ACE_HANDLE h, 
                                        const SV_OFFSET_TYPE &offset,
                                        const SV_UINT & mem_consumed, 
                                        SharedAlignedBuffer ptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    iocount_mutex.acquire();
    cdp_memorymap_key_t key(h,offset);
    m_buffers_allocated_for_io.insert(std::make_pair(key, ptr));
    m_memconsumed_for_io += mem_consumed;
    iocount_mutex.release();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
}

bool CDPV2Writer::release_memory_allocation(ACE_HANDLE h, 
                                            const SV_OFFSET_TYPE &offset,
                                            const SV_UINT & mem_to_release)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s offset:" LLSPEC " mem_to_release:%d\n",
		FUNCTION_NAME, offset, mem_to_release);

    iocount_mutex.acquire();

    if(m_memtobefreed_on_io_completion)
    {

        cdp_memorymap_key_t key(h,offset);
        cdp_memoryallocs_t::const_iterator mem_iter =
            m_buffers_allocated_for_io.find(key);

        if(mem_iter == m_buffers_allocated_for_io.end())
        {
            m_error = true;
            stringstream l_stderr;
            l_stderr << "internal logic error in memory management "
                     << " volume: " << m_volumename 
                     << " offset: " << offset
                     << " error: trying to free already freed memory.\n";
            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
		}
		else
        {
            m_memconsumed_for_io -= mem_to_release;
            m_buffers_allocated_for_io.erase(key);
        }
	}
	else
    {
        m_memconsumed_for_io -= mem_to_release;
    }

    iocount_mutex.release();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s offset:" LLSPEC " mem_to_release:%d\n",
		FUNCTION_NAME, offset, mem_to_release);
    return rv;
}



bool CDPV2Writer::canIssueNextIo(const SV_UINT & mem_required)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s mem_required:%d\n",FUNCTION_NAME, mem_required);

    try
    {

        do
        {
            unsigned long pendingios = m_pendingios;
            if(error())
            {
                rv = false;
                break;
            }

            if(!m_memconsumed_for_io)
            {
                rv = true;
                break;
            }

            if((m_pendingios < m_maxpendingios)
               &&(!mem_required ||  (m_memconsumed_for_io + mem_required <= m_maxMemoryForDrtds)))
            {
                rv = true;
                break;
			}
			else
            {
                iocount_mutex.acquire();
                while( !error() 
                       && ( ((m_memconsumed_for_io > 0) && (m_memconsumed_for_io + mem_required > m_maxMemoryForDrtds))
                            || (m_pendingios >= m_maxpendingios)))
                {
                    iocount_cv.wait();
                }

                iocount_mutex.release();

                if(error())
                {
                    rv = false;
                    break;
                }
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s mem_required:%d\n",FUNCTION_NAME,mem_required);
    return rv;
}

void CDPV2Writer::incrementPendingIos()
{
    iocount_mutex.acquire();
    ++m_pendingios;
    iocount_mutex.release();
}

void CDPV2Writer::decrementPendingIos()
{
    iocount_mutex.acquire();
    --m_pendingios;
    if( m_pendingios <= m_minpendingios)
        iocount_cv.signal();
    iocount_mutex.release();

}

void CDPV2Writer::incrementMemoryUsage(const SV_UINT & mem_consumed)
{
    iocount_mutex.acquire();
    m_memconsumed_for_io  += mem_consumed;
    iocount_mutex.release();
}

void CDPV2Writer::decrementMemoryUsage(const SV_UINT & mem_released)
{
    iocount_mutex.acquire();
    m_memconsumed_for_io  -= mem_released;
    iocount_mutex.release();
}

// ================================================================
//  sparse retention
// ================================================================


bool CDPV2Writer::updatecdpv3(const std::string & volumename,
                              const std::string & filePath, 
                              char * diff_data,
                              DiffPtr change_metadata)
{
    bool rv = true;

    // file being applied
    std::string fileBeingApplied;
    std::string fileBeingAppliedBaseName;


    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        do
        {


            //__asm int 3;
            //
            // if a diff file contains changing spanning
            // for say 3 hours
            // we apply in 3 parts
            // 1st part all changes belonging to the first hour
            // 2nd part all changes belonging to second hour
            // 3rd part all changes belonging to third hour
            //
            // part_no is the current part 
            // we are applying
            //
            // current_part_hrstart is the start
            // time in hours for current part
            //
            // current_part_hrstart is the end
            // time in hours for current part
            // 

            SV_UINT part_no = 1;
            SV_ULONGLONG current_part_hrstart = 0; 
            SV_ULONGLONG current_part_hrend = 0; 


            // 
            // verify we are processing
            // files in correct order
            // of increasing timestamps
            bool in_order = true;

            //
            // metadata file name,
            // file name is based on the hour
            // 
            std::string metadata_filename;
            std::string metadata_filepath;
            SV_UINT mdfilesize = 0;

            //
            // metadata for current part
            //
            cdpv3metadata_t cow_metadata;
            cdpv3metadata_t vol_metadata;


            //
            // has current part already been applied
            // in previous invocation?
            bool current_part_already_applied = false;



            fileBeingApplied = filePath;
            std::string::size_type idx = fileBeingApplied.rfind(".gz");
            if(std::string::npos != idx && (fileBeingApplied.length() - idx) == 3)
                fileBeingApplied.erase(idx, fileBeingApplied.length());
            fileBeingAppliedBaseName = basename_r(fileBeingApplied.c_str());

            // 
            // step 0:
            // verify the file version
            if(!verify_file_version(fileBeingAppliedBaseName,change_metadata))
            {
                rv = false;
                break;
            }

            //
            // step 1:
            // verify the timestamp ordering



            in_order = verify_increasing_timestamp(fileBeingAppliedBaseName,change_metadata);

            if(!in_order) 
            {
                if((!m_allowOutOfOrderTS || !m_allowOutOfOrderSeq))
                {
                    DebugPrintf(SV_LOG_ERROR, 
                                "Differentials will not be applied on %s due to files recieved out of order\n",
                                m_volumename.c_str());
                    CDPUtil::QuitRequested(180);
                    rv = false;
                    break;
                }

                if(CDPUtil::QuitRequested())
                {
                    rv = false;
                    break;
                }
            }



            // step 2: 
            // Read the transaction file
            //
            //
            // Transaction File
            // Contains:
            // 
            //  (I) Header information
            //       version, revision etc
            //  (II) transaction information related to apply and
            //       rollback
            //  (III) latest timestamp and sequence in the retention
            //        db (used in case of out of order apply)
            //
            // (II) contains:
            // 
            //  1) Differential File which has been partially applied
            //  2) Part No, we are currently processing
            //     Part no starts from 1
            //  3) Metadata File Name
            //    you need this in case you already applied the current
            //    part to read back the metadata and set corresponding
            //    bits in bitmap
            //  
            // We need not read this file per diff as we already have this
            // information in the inmemory structure(m_txn). We just read it
            // while instantiating the cdpv2writer for each invocation of
            // dataprotection. This way we get considerable amount of performance
            // improvement
            //
            std::string txnfilename = cdpv3txnfile_t::getfilepath(m_cdpdir);
            cdpv3txnfile_t txnfile(txnfilename);

            std::stringstream txnstrm;
            txnstrm << "transaction:\n"
                    << m_txn.m_lastupdate.m_diffname << "\n"
                    << m_txn.m_lastupdate.m_mdfilename << "\n"
                    << m_txn.m_lastupdate.m_partno << "\n"
                    << m_txn.m_lastupdate.m_mdsize << "\n";
            DebugPrintf(SV_LOG_DEBUG, "%s\n", txnstrm.str().c_str());


            //
            // step 3:
            // if it is resync file or
            // if the timestamp is out of order wrt retention store 
            // adjust the timestamps
            // 


            if(!adjust_timestamp(change_metadata,m_txn))
            {
                rv = false;
                break;
            }

            //
            // step 4:
            // start applying in parts
            //

            // 
            // loop till we have processed the complete differential
            // for e.g say
            // diff contains changes from 10:30 to 12:48
            // you would loop for 3 times
            // first loop:
            //  currrent_part_hrstart = 10:00
            //  current_part_hrend = 11:00
            // second loop:
            //  current_part_hrstart = 11:00
            //  current_part_hrend = 12:00
            // third loop:
            //  current_part_hrstart = 12:00
            //  current_part_hrend = 1:00
            //

            if(!CDPUtil::ToTimestampinHrs(change_metadata -> starttime(), current_part_hrstart))
            {
                rv = false;
                break;
            }

            current_part_hrend = current_part_hrstart + HUNDREDS_OF_NANOSEC_IN_HOUR;


            while (current_part_hrstart <= change_metadata -> endtime())
            {				

                //
                // reset changes processed in current part
                //

                current_part_already_applied = false;
                cow_metadata.clear();
                vol_metadata.clear();
                m_cowlocations.clear();
                mdfilesize = 0;

                //
                // Fill the metadata 
                //
                // from transaction record
                //  is the diff file name matching?
                //  yes
                //    if part no == current part no?
                //      current_part_already_applied = true
                //      assert(metadata file name matching)
                //      read metadata from metadata file
                //      flush the bitmap
                //      skip current part for cow
                //    if part no > current part no
                //      current_part_already_applied = true
                //      skip current part for cow
                //    if part  no < current part no
                //      current_part_already_applied = false
                //      rollback partial write 
                //      and ensure metadata file size
                //      matches the size in transaction file
                //  no
                //     current_part_already_written = false

                PerfTime md_start_time;
                PerfTime md_end_time;

                PerfTime bitmap_start_time;
                PerfTime bitmap_end_time;

                PerfTime md_compute_start_time;
                PerfTime md_compute_end_time;

                metadata_filepath = cdpv3metadatafile_t::getfilepath(current_part_hrstart, m_cdpdir);				
                if(metadata_filepath.empty())
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "FUNCTION %s recieved empty metadata path for timestamp " ULLSPEC " \n",
                                FUNCTION_NAME, current_part_hrstart);
                    rv = false;
                    break;
                }
                metadata_filename = basename_r(metadata_filepath.c_str());

                if(m_cache_cdpfile_handles) //Close all previously opened handles.
                {
                    if((m_cdpdata_handles_hr != current_part_hrstart) || (change_metadata -> ContainsMarker()))
                    {
                        if(!m_cdpdata_asyncwrite_handles.empty())
                        {
                            close_cdpdata_asyncwrite_handles();
                        }

                        if(!m_cdpdata_syncwrite_handles.empty())
                        {
                            close_cdpdata_syncwrite_handles();
                        }
                    }
                    if(m_cdpdata_asyncwrite_handles.size() >= m_max_filehandles_tocache)
                    {
                        close_cdpdata_asyncwrite_handles();
                    }
                    if(m_cdpdata_syncwrite_handles.size() >= m_max_filehandles_tocache)
                    {
                        close_cdpdata_syncwrite_handles();
                    }
                    m_cdpdata_handles_hr = current_part_hrstart;
                }


                if(!isInDiffSync())
                {
                    if(m_resync_txn_mgr  && m_resync_txn_mgr -> is_entry_present(fileBeingAppliedBaseName))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "%s is already applied to retention.\n", fileBeingAppliedBaseName.c_str());
                        current_part_already_applied = true;
                    }
                    else
                    {
                        current_part_already_applied = false;
                    }
				}
				else
                {
                    if(!strcmp(m_txn.m_lastupdate.m_diffname,fileBeingAppliedBaseName.c_str()))
                    {
                        if(m_txn.m_lastupdate.m_partno == part_no)
                        {
                            current_part_already_applied = true;
                            if(!strcmp(metadata_filename.c_str(),m_txn.m_lastupdate.m_mdfilename))
                            {

                                getPerfTime(md_start_time);

                                if(!read_metadata_from_mdfile_v3(metadata_filepath,cow_metadata))
                                {
                                    DebugPrintf(SV_LOG_ERROR,
                                                "FUNCTION %s failed in read_metadata_from_mdfile_v3 %s" ULLSPEC " \n",
                                                FUNCTION_NAME, m_txn.m_lastupdate.m_mdfilename, current_part_hrstart);
                                    rv = false;
                                    break;
                                }

#if 0
                                dump_metadata_v3(volumename,filePath,change_metadata);
                                dump_metadata_v3(volumename,cow_metadata);
                                dump_cowlocations(volumename,cow_metadata);
#endif

                                getPerfTime(md_end_time);
                                m_metadata_time +=  getPerfTimeDiff(md_start_time, md_end_time);

                                getPerfTime(bitmap_start_time);
                                if(!set_ts_inbitmap_v3(cow_metadata,m_txn))
                                {
                                    rv = false;
                                    break;
                                }

                                if(!m_cdpbitmap.flush())
                                {
                                    rv = false;
                                    break;
                                }

                                getPerfTime(bitmap_end_time);
                                m_bitmapwrite_time += getPerfTimeDiff(bitmap_start_time, bitmap_end_time);
                            }

						}
						else if (m_txn.m_lastupdate.m_partno > part_no)
                        {
                            current_part_already_applied = true;
						}
						else
                        {
                            getPerfTime(md_start_time);

                            current_part_already_applied = false;
                            if(!rollback_partial_mdwrite_v3(metadata_filepath, m_txn.m_lastupdate.m_mdsize))
                            {
                                rv = false;
                                break;
                            }

                            getPerfTime(md_end_time);
                            m_metadata_time +=  getPerfTimeDiff(md_start_time, md_end_time);

                        }
                    } // filename in txn matches with diff file
                    else
                    {
                        current_part_already_applied = false;
                    }
                }


                if(!current_part_already_applied)
                {
                    getPerfTime(md_start_time);

                    //
                    // update the transaction file with current metadata
                    // file size (used in case of partial write to rollback)
                    //
                    if(!get_metadatafile_size_v3(metadata_filepath, mdfilesize))
                    {
                        rv = false;
                        break;
                    }

                    inm_strcpy_s(m_txn.m_lastupdate.m_diffname, ARRAYSIZE(m_txn.m_lastupdate.m_diffname), fileBeingAppliedBaseName.c_str());
                    inm_strcpy_s(m_txn.m_lastupdate.m_mdfilename, ARRAYSIZE(m_txn.m_lastupdate.m_mdfilename), metadata_filename.c_str());
                    m_txn.m_lastupdate.m_mdsize = mdfilesize;
                    m_txn.m_lastupdate.m_partno = part_no -1;

                    if(current_part_hrstart == m_txn.m_bkmks.m_hr)
                    {
                        m_txn.m_bkmks.m_bkmkcount += change_metadata -> NumUserTags();
					}
					else
                    {
                        m_txn.m_bkmks.m_hr = current_part_hrstart;
                        if(part_no ==1)
                            m_txn.m_bkmks.m_bkmkcount = change_metadata -> NumUserTags();
                        else
                            m_txn.m_bkmks.m_bkmkcount = 0;
                    }


                    if(isInDiffSync())
                    {
                        if(!txnfile.write(m_txn, m_sector_size, m_useUnBufferedIo))
                        {
                            rv = false;
                            break;
                        }
                    }

                    getPerfTime(md_end_time);
                    m_metadata_time += getPerfTimeDiff(md_start_time, md_end_time);

                    //
                    // construct the cow metadata
                    //

                    getPerfTime(md_compute_start_time);

                    if(!get_cowmetadata_for_timerange_v3(current_part_hrstart, 
                                                         current_part_hrend,
                                                         change_metadata,
                                                         m_txn,
                                                         cow_metadata))
                    {
                        rv = false;
                        break;
                    }

#if 0
                    dump_metadata_v3(volumename,filePath,change_metadata);
                    dump_metadata_v3(volumename,cow_metadata);
                    dump_cowlocations(volumename,cow_metadata);
#endif

                    getPerfTime(md_compute_end_time);
                    m_cownonoverlap_computation_time += getPerfTimeDiff(md_compute_start_time, md_compute_end_time);


                    if(m_profile_volread)
                        profile_volume_read();

                    if(m_profile_cdpwrite)
                        profile_cdp_write(cow_metadata);

                    //
                    // write to the cow data files
                    //

                    if(!update_cow_v3(volumename,
                                      filePath, 
                                      diff_data,
                                      cow_metadata))
                    {
                        rv = false;
                        break;
                    }

                    //
                    // update the db
                    //

                    PerfTime dbupdate_start_time;
                    getPerfTime(dbupdate_start_time);

                    if(!update_dbrecord_v3(cow_metadata,m_txn))
                    {
                        rv = false;
                        break;
                    }

                    PerfTime dbupdate_end_time;
                    getPerfTime(dbupdate_end_time);
                    m_dboperation_time += getPerfTimeDiff(dbupdate_start_time, dbupdate_end_time);


                    // write the metadata and then update 
                    // transaction file

                    getPerfTime(md_start_time);

                    if(!write_metadata_v3(metadata_filepath,cow_metadata))
                    {
                        rv = false;
                        break;
                    }


                    if(isInDiffSync())
                    {

                        if(!get_metadatafile_size_v3(metadata_filepath, mdfilesize))
                        {
                            rv = false;
                            break;
                        }

                        inm_strcpy_s(m_txn.m_lastupdate.m_diffname, ARRAYSIZE(m_txn.m_lastupdate.m_diffname), fileBeingAppliedBaseName.c_str());
                        inm_strcpy_s(m_txn.m_lastupdate.m_mdfilename, ARRAYSIZE(m_txn.m_lastupdate.m_mdfilename), metadata_filename.c_str());
                        m_txn.m_lastupdate.m_mdsize = mdfilesize;
                        m_txn.m_lastupdate.m_partno = part_no;
                        if(!txnfile.write(m_txn, m_sector_size, m_useUnBufferedIo))
                        {
                            rv = false;
                            break;
                        }
                    }

                    getPerfTime(md_end_time);
                    m_metadata_time += getPerfTimeDiff(md_start_time, md_end_time);

                    getPerfTime(bitmap_start_time);

                    if(!m_cdpbitmap.flush())
                    {
                        rv = false;
                        break;
                    }
                    getPerfTime(bitmap_end_time);
                    m_bitmapwrite_time += getPerfTimeDiff(bitmap_start_time, bitmap_end_time);

                    if(!isInDiffSync() && m_resync_txn_mgr)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "adding %s entry to resync_txn file.\n", fileBeingAppliedBaseName.c_str());
                        m_resync_txn_mgr -> add_entry(fileBeingAppliedBaseName);
                    }
                }


                PerfTime volcompute_start_time;
                getPerfTime(volcompute_start_time);

                if(!get_volmetadata_for_timerange_v3(current_part_hrstart, 
                                                     current_part_hrend,
                                                     change_metadata,
                                                     vol_metadata))
                {
                    rv = false;
                    break;
                }

#if 0
                dump_metadata_v3(volumename,vol_metadata);
#endif

                PerfTime volcompute_end_time;
                getPerfTime(volcompute_end_time);
                m_volnonoverlap_computation_time += getPerfTimeDiff(volcompute_start_time,volcompute_end_time);

                PerfTime volwrite_start_time;
                getPerfTime(volwrite_start_time);

                if(m_profile_volwrite)
                    profile_volume_write(vol_metadata);


                if(!updatevolume(volumename,filePath,diff_data,
                                 vol_metadata.m_drtds.begin(),
                                 vol_metadata.m_drtds.end()))
                {
                    rv = false;
                    break;
                }

                PerfTime volwrite_end_time;
                getPerfTime(volwrite_end_time);
                m_volwrite_time += getPerfTimeDiff(volwrite_start_time,volwrite_end_time);

                current_part_hrstart = current_part_hrend;
                current_part_hrend += HUNDREDS_OF_NANOSEC_IN_HOUR;
                ++part_no;
            }

            if(rv)
            {
                m_txn.m_ts.m_ts = change_metadata -> endtime();
                m_txn.m_ts.m_seq = change_metadata ->EndTimeSequenceNumber();
                if(!txnfile.write(m_txn,m_sector_size, m_useUnBufferedIo))
                {
                    rv = false;
                    break;
                }
            }

        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// ==================================================
//
// metadata routines
//  read_metadata_from_mdfile_v3
//  rollback_partial_mdwrite_v3
//  get_metadatafile_size_v3
//  get_cowmetadata_for_timerange_v3
//    get_cow_fileid_writeoffset_v3
//      get_cow_filename_v3
//      get_cowdatafile_size_v3
//    construct_cow_location_v3
//  write_metadata_v3
// ===================================================


bool CDPV2Writer::read_metadata_from_mdfile_v3(const std::string &metadata_filepath,
                                               cdpv3metadata_t &cow_metadata)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s metadata_filepath: %s\n",
		FUNCTION_NAME, metadata_filepath.c_str());
    do
    {
        cdpv3metadatafile_t mdfile(metadata_filepath);
        if(mdfile.read_metadata_desc(cow_metadata) != SVS_OK)
        {
            rv = false;
            break;
        }

    }while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s metadata_filepath: %s\n",
		FUNCTION_NAME, metadata_filepath.c_str());
    return rv;
}

bool CDPV2Writer::rollback_partial_mdwrite_v3(const std::string &metadata_filepath,
                                              const SV_UINT & mdfilesize)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s metadata_filepath: %s eof size: %d\n",
		FUNCTION_NAME, metadata_filepath.c_str(), mdfilesize);

    ACE_stat statbuf;
    if(0 != sv_stat(getLongPathName(metadata_filepath.c_str()).c_str(), &statbuf))
    {
        DebugPrintf(SV_LOG_DEBUG, "FUNCTION: %s unable to truncate %s to size %d. file does not exist\n",
                    FUNCTION_NAME, metadata_filepath.c_str(), mdfilesize);
        rv = true;
	}
	else if (0 != ACE_OS::truncate(getLongPathName(metadata_filepath.c_str()).c_str(), mdfilesize))
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to truncate %s to size %d. error: %d\n",
                    FUNCTION_NAME, metadata_filepath.c_str(), mdfilesize, ACE_OS::last_error());
        rv = false;
	}
	else
    {
        DebugPrintf(SV_LOG_DEBUG, "truncated %s to size %d.\n",
                    metadata_filepath.c_str(), mdfilesize);
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s metadata_filepath: %s eof size: %d\n",
		FUNCTION_NAME, metadata_filepath.c_str(), mdfilesize);
    return rv;
}

bool CDPV2Writer::get_metadatafile_size_v3(const std::string & metadata_filepath,
                                           SV_UINT & mdfilesize)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s metadata_filepath:%s\n",
		FUNCTION_NAME, metadata_filepath.c_str());

    do
    {

        ACE_HANDLE handle = ACE_INVALID_HANDLE;
        ACE_LOFF_T endoffset = 0;

        if(!get_cdpdata_sync_writehandle(metadata_filepath, handle))
        {
            rv = false;
            break;
        }


        endoffset = ACE_OS::llseek(handle,0, SEEK_END);
        if(endoffset < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s seek to end of file %s failed. error: %d\n",
                        FUNCTION_NAME, metadata_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        mdfilesize = endoffset;

    } while (0);


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s metadata_filepath:%s\n",
		FUNCTION_NAME, metadata_filepath.c_str());
    return rv;
}

bool CDPV2Writer::get_cowmetadata_for_timerange_v3(const SV_ULONGLONG &current_part_hrstart, 
                                                   const SV_ULONGLONG &current_part_hrend,
                                                   DiffPtr change_metadata,
                                                   const cdpv3transaction_t &txn,
                                                   cdpv3metadata_t &cow_metadata)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, 
		"ENTERED %s timerange [" ULLSPEC " - " ULLSPEC "]\n",
		FUNCTION_NAME, current_part_hrstart, current_part_hrend);

    do
    {
        cow_metadata.clear();

        //
        // changes that have to be skipped
        // as they belong to previous parts
        // 
        SV_UINT changes_skipped = 0;
        SV_UINT drtd_start = 0;
        SV_UINT curr_drtd = 0;
        SV_UINT cowdrtd_no = 0;
        cdp_cow_location_t cow_locations;


        //
        // difference between current part tsfc/seq and
        // differential tsfc/seq
        //
        SV_UINT seq_difference = 0;
        SV_UINT time_difference = 0;

        // 
        // local variables
        //
        SV_OFFSET_TYPE aligned_offset = -1;
        SV_UINT aligned_length = 0;
        SV_UINT seq_delta = 0;
        SV_UINT time_delta = 0;
        SV_OFFSET_TYPE  read_voloffset = -1;
        SV_UINT         read_vollength = 0;
        SV_OFFSET_TYPE  write_offset = -1;
        SV_UINT         write_fileid = 0;
        SV_UINT delta = 0;
        SV_UINT pending_length = 0;
        SV_UINT block_alignment = m_cdpbitmap.get_block_size();

        SV_OFFSET_TYPE next_voloffset = -1;
        SV_UINT nextwrite_fileid = 0;
        SV_OFFSET_TYPE nextwrite_offset = -1;

        cdp_drtdv2s_iter_t drtds_iter = change_metadata -> DrtdsBegin();
        cdp_drtdv2s_iter_t drtds_end  = change_metadata -> DrtdsEnd();

        //
        // skip changes which are from previous parts
        //
        for( ; drtds_iter != drtds_end ; ++drtds_iter)
        {
            if((change_metadata -> starttime() + drtds_iter ->get_timedelta())
               < current_part_hrstart)
            {
                ++changes_skipped;
			}
			else
            {
                break;
            }
        }


        // determine the tsfc, tsfcseq
        if(0 == changes_skipped)
        {
            if(change_metadata -> starttime() < current_part_hrstart)
            {
                cow_metadata.m_tsfc = change_metadata -> endtime();
                cow_metadata.m_tsfcseq = change_metadata -> EndTimeSequenceNumber();
			}
			else
            {
                cow_metadata.m_tsfc = change_metadata -> starttime();
                cow_metadata.m_tsfcseq = change_metadata -> StartTimeSequenceNumber();
            }

		}
		else if (drtds_iter == drtds_end)
        {
            cow_metadata.m_tsfc = change_metadata -> endtime();
            cow_metadata.m_tsfcseq = change_metadata -> EndTimeSequenceNumber();
		}
		else
        {
            cow_metadata.m_tsfc = change_metadata -> starttime() + drtds_iter ->get_timedelta();
            cow_metadata.m_tsfcseq = change_metadata -> StartTimeSequenceNumber() + drtds_iter ->get_seqdelta();
        }

        seq_difference = cow_metadata.m_tsfcseq - change_metadata -> StartTimeSequenceNumber();
        time_difference = cow_metadata.m_tsfc - change_metadata -> starttime();

        //
        // add bookmarks if it is the first part
        //
        if(0 == changes_skipped)
        {
            UserTagsIterator udtsIter = change_metadata -> UserTagsBegin();
            UserTagsIterator udtsEnd  = change_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                cow_metadata.m_users.push_back(pMarker);
            }
        }

        drtd_start = changes_skipped;
        curr_drtd = drtd_start;

        //Performance Improvement : Async read requests for bitmap files.
        cdp_change_offset_ts_list_t readoffsets_ts;
        cdp_drtdv2s_iter_t tempitr = drtds_iter;

        SV_TIME svtime;

        if(!CDPUtil::ToSVTime(current_part_hrstart,svtime))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s invalid timestamp: " ULLSPEC "\n",
                        FUNCTION_NAME, current_part_hrstart);
            rv = false;
            break;
        }

        cdp_timestamp_t old_iotime;
        cdp_timestamp_t new_iotime;

        memset(&old_iotime, 0, sizeof(cdp_timestamp_t));

        new_iotime.year = svtime.wYear - CDP_BASE_YEAR;
        new_iotime.month = svtime.wMonth;
        new_iotime.days = svtime.wDay;
        new_iotime.hours = svtime.wHour;
        new_iotime.events = txn.m_bkmks.m_bkmkcount;



        for ( /* empty */ ; tempitr != drtds_end ; ++tempitr )
        {
            cdp_drtdv2_t change_drtd = *tempitr;

            if(change_metadata -> starttime() + change_drtd.get_timedelta() >= current_part_hrend)
                break;

            aligned_offset = change_drtd.get_volume_offset();
            aligned_length = change_drtd.get_length();

            delta = aligned_offset % block_alignment;
            if(delta)
            {
                aligned_offset -= delta;
                aligned_length += delta;
            }

            delta = aligned_length % block_alignment;
            if(delta)
            {
                aligned_length += (block_alignment - delta);
            }

            pending_length = aligned_length;
            read_voloffset = aligned_offset;

            // PR#11756
            // if the aligned length is exceeding the volume capacity
            // cap it.
            if(read_voloffset + pending_length > m_srccapacity)
            {
                pending_length = m_srccapacity - read_voloffset;
            }

            read_vollength = min(block_alignment, pending_length);

            //Insert the volume offset to read list.
            cdp_offset_change_ts_t offset_change_ts;
            offset_change_ts.repeating = false; //Initialize to false. If the offset is repeating, it will be set to true to avoid multiple reads.
            offset_change_ts.offset = read_voloffset;
            offset_change_ts.setts = new_iotime;
            offset_change_ts.getts = old_iotime;

            readoffsets_ts.push_back(offset_change_ts);

            if(pending_length < block_alignment)
            {
                pending_length = 0;
			}
			else
            {
                pending_length -= block_alignment;
            }

            next_voloffset = (read_voloffset + read_vollength);
            while(pending_length > 0)
            {
                //Add to next_voloffset to read list.
                cdp_offset_change_ts_t offset_change_ts;
                offset_change_ts.repeating = false;
                offset_change_ts.offset = next_voloffset;
                offset_change_ts.setts = new_iotime;
                offset_change_ts.getts = old_iotime;

                readoffsets_ts.push_back(offset_change_ts);

                next_voloffset += min(pending_length,block_alignment);

                if(pending_length < block_alignment)
                {
                    pending_length = 0;
				}
				else
                {
                    pending_length -= block_alignment;
                }

            }
        }

        PerfTime bitmapread_start_time;
        getPerfTime(bitmapread_start_time);
        //Retrieve the ts for the changed offsets.
        if(!m_cdpbitmap.get_and_set_ts(readoffsets_ts))
        {
            DebugPrintf(SV_LOG_DEBUG,"%s Failed to get and set ts for changed offsets\n",FUNCTION_NAME);
            rv = false;
            break;
        }
        PerfTime bitmapread_end_time;
        getPerfTime(bitmapread_end_time);
        m_bitmapread_time += getPerfTimeDiff(bitmapread_start_time,bitmapread_end_time);


        for ( /* empty */ ; drtds_iter != drtds_end ; ++drtds_iter )
        {
            cdp_drtdv2_t change_drtd = *drtds_iter;

            if(change_metadata -> starttime() + change_drtd.get_timedelta() >= current_part_hrend)
                break;

            aligned_offset = change_drtd.get_volume_offset();
            aligned_length = change_drtd.get_length();

            delta = aligned_offset % block_alignment;
            if(delta)
            {
                aligned_offset -= delta;
                aligned_length += delta;
            }

            delta = aligned_length % block_alignment;
            if(delta)
            {
                aligned_length += (block_alignment - delta);
            }

            seq_delta = change_drtd.get_seqdelta() - seq_difference;
            time_delta = change_drtd.get_timedelta() - time_difference;

            pending_length = aligned_length;
            read_voloffset = aligned_offset;

            // PR#11756
            // if the aligned length is exceeding the volume capacity
            // cap it.
            if(read_voloffset + pending_length > m_srccapacity)
            {
                pending_length = m_srccapacity - read_voloffset;
            }

            read_vollength = min(block_alignment, pending_length);

            cdp_offset_change_ts_t offset_change_ts = readoffsets_ts.front();
            readoffsets_ts.pop_front();

            if(offset_change_ts.offset != read_voloffset)
            {
                DebugPrintf(SV_LOG_DEBUG,"Volume offset mismatch wit the list of offsets read\n");
                rv = false;
                break;
            }
            if(!get_cow_fileid_writeoffset_v3(read_voloffset, 
                                              read_vollength,
                                              cow_metadata,
                                              write_fileid,
                                              write_offset,
                                              offset_change_ts.getts,
                                              offset_change_ts.setts))
            {
                rv = false;
                break;
            }

            if(pending_length < block_alignment)
            {
                pending_length = 0;
			}
			else
            {
                pending_length -= block_alignment;
            }

            next_voloffset = (read_voloffset + read_vollength);
            nextwrite_fileid = 0;
            nextwrite_offset = 0;

            while(pending_length > 0)
            {

                cdp_offset_change_ts_t offset_change_ts = readoffsets_ts.front();
                readoffsets_ts.pop_front();

                if(offset_change_ts.offset != next_voloffset)
                {
                    DebugPrintf(SV_LOG_DEBUG,"Volume offset mismatch wit the list of offsets read\n");
                    rv = false;
                    break;
                }
                if(!get_cow_fileid_writeoffset_v3(next_voloffset, 
                                                  min(pending_length, block_alignment),
                                                  cow_metadata,
                                                  nextwrite_fileid,
                                                  nextwrite_offset,
                                                  offset_change_ts.getts,
                                                  offset_change_ts.setts))
                {
                    rv = false;
                    break;
                }

                if (nextwrite_fileid == write_fileid)
                {
                    read_vollength += min(pending_length,block_alignment);
                    next_voloffset += min(pending_length,block_alignment);
				}
				else
                {

                    cdp_drtdv2_t cow_drtd(read_vollength,
                                          read_voloffset,
                                          write_offset,
                                          seq_delta,
                                          time_delta,
                                          write_fileid);

                    cow_metadata.m_drtds.push_back(cow_drtd);

                    if(!construct_cow_location_v3(change_metadata, 
                                                  drtd_start, curr_drtd -1,
                                                  cow_drtd, cow_locations))
                    {
                        rv = false;
                        break;
                    }

                    m_cowlocations.insert(std::make_pair(cowdrtd_no,cow_locations));
                    ++cowdrtd_no;

                    read_voloffset = next_voloffset;
                    read_vollength = min(pending_length, block_alignment);
                    write_fileid = nextwrite_fileid;
                    write_offset = nextwrite_offset;
                    next_voloffset = (read_voloffset + read_vollength);
                }

                if(pending_length < block_alignment)
                {
                    pending_length = 0;
				}
				else
                {
                    pending_length -= block_alignment;
                }

            }


            if(!rv)
                break;

            cdp_drtdv2_t cow_drtd(read_vollength,
                                  read_voloffset,
                                  write_offset,
                                  seq_delta,
                                  time_delta,
                                  write_fileid);

            cow_metadata.m_drtds.push_back(cow_drtd);
            if(!construct_cow_location_v3(change_metadata, 
                                          drtd_start, curr_drtd -1,
                                          cow_drtd, cow_locations))
            {
                rv = false;
                break;
            }

            m_cowlocations.insert(std::make_pair(cowdrtd_no,cow_locations));
            ++cowdrtd_no;

            ++curr_drtd;
        }

        if(!rv)
            break;

        if(drtds_iter == drtds_end)
        {
            if(change_metadata -> endtime() >= current_part_hrend)
            {
                cow_metadata.m_tslc = cow_metadata.m_tsfc;
                cow_metadata.m_tslcseq = cow_metadata.m_tsfcseq;
			}
			else
            {
                cow_metadata.m_tslc = change_metadata -> endtime();
                cow_metadata.m_tslcseq = change_metadata -> EndTimeSequenceNumber();
            }

		}
		else
        {
            cow_metadata.m_tslc = cow_metadata.m_tsfc + time_delta;
            cow_metadata.m_tslcseq = cow_metadata.m_tsfcseq + seq_delta;
        }

        cow_metadata.m_type = 0;
        if(change_metadata -> type() == RESYNC_MODE)
        {
            cow_metadata.m_type = 0;
		}
		else if (change_metadata->type() == OUTOFORDER_TS)
        {
            cow_metadata.m_type = 0; 
		}
		else
        {
            cow_metadata.m_type |= DIFFTYPE_DIFFSYNC_BIT_SET;

            if(change_metadata -> type() == NONSPLIT_IO)
            {
                cow_metadata.m_type |= DIFFTYPE_NONSPLITIO_BIT_SET;
            }

            if(change_metadata -> version() >= SVD_PERIOVERSION)
            {
                cow_metadata.m_type |= DIFFTYPE_PERIO_BIT_SET;
            }
        }


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, 
		"EXITED %s timerange [" ULLSPEC " - " ULLSPEC "]\n",
		FUNCTION_NAME, current_part_hrstart, current_part_hrend);
    return rv;
}

bool CDPV2Writer::get_cow_fileid_writeoffset_v3(const SV_OFFSET_TYPE &read_voloffset, 
                                                const SV_UINT &read_vollength,
                                                cdpv3metadata_t  & cow_metadata,
                                                SV_UINT & write_fileid,
                                                SV_OFFSET_TYPE &write_offset,
                                                cdp_timestamp_t & old_iotime,
                                                cdp_timestamp_t & new_iotime)
{
    bool rv = true;
    std::string cow_filename;
    std::string cow_filepath;
    SV_ULONGLONG cow_filesize;
    SV_UINT sequenceNumber = 0;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s vol.offset: " ULLSPEC "\n",
		FUNCTION_NAME, read_voloffset);
    do
    {
        cow_filename = get_cow_filename_v3(read_voloffset,	old_iotime,	new_iotime);
        cow_filepath = m_cdpdir;
        cow_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        cow_filepath += cow_filename;

        std::vector<std::string>::iterator file_iter = 
            find(cow_metadata.m_filenames.begin(), cow_metadata.m_filenames.end(),
                 cow_filename);

        while(file_iter == cow_metadata.m_filenames.end())
        {

            if(!get_cowdatafile_size_v3(cow_filepath, cow_filesize))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to get size for file %s\n",
                            FUNCTION_NAME, cow_filepath.c_str());
                rv = false;
                break;
            }

            if(!isInDiffSync() || (0 == m_maxcdpv3FileSize) || cow_filesize < m_maxcdpv3FileSize)
            {
                rv = true;
                break;
            }

            ++sequenceNumber;
            cow_filename = getNextCowFileName(old_iotime,new_iotime,sequenceNumber);
            cow_filepath = m_cdpdir;
            cow_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            cow_filepath += cow_filename;


            file_iter = find(cow_metadata.m_filenames.begin(), cow_metadata.m_filenames.end(),
                             cow_filename);

        }

        if(!rv)
            break;

        if(file_iter == cow_metadata.m_filenames.end())
        {
            cow_metadata.m_filenames.push_back(cow_filename);
            cow_metadata.m_baseoffsets.push_back(cow_filesize);
            cow_metadata.m_byteswritten.push_back(read_vollength);
            write_fileid = cow_metadata.m_filenames.size() - 1;
            write_offset = cow_metadata.m_baseoffsets[write_fileid];
		}
		else
        {
            write_fileid = file_iter - cow_metadata.m_filenames.begin();
            write_offset = cow_metadata.m_baseoffsets[write_fileid] + cow_metadata.m_byteswritten[write_fileid];
            cow_metadata.m_byteswritten[write_fileid] += read_vollength;
        }

    } while (0);


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s vol.offset: " ULLSPEC "\n",
		FUNCTION_NAME, read_voloffset);
    return rv;
}


bool CDPV2Writer::get_cow_fileid_writeoffset_v3(const SV_OFFSET_TYPE &read_voloffset, 
                                                const SV_UINT &read_vollength,
                                                const SV_ULONGLONG & newts,
                                                const cdpv3transaction_t &txn,
                                                cdpv3metadata_t  & cow_metadata,
                                                SV_UINT & write_fileid,
                                                SV_OFFSET_TYPE &write_offset)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s vol.offset: " ULLSPEC "\n",
		FUNCTION_NAME, read_voloffset);
    do
    {
        //
        // new io time stamp for the 
        // block starting at read_voloffset
        //

        SV_TIME svtime;
        SV_ULONGLONG newts_in_hrs = 0;
        cdp_timestamp_t new_iotime;
        cdp_timestamp_t old_iotime;
        std::string cow_filename;
        std::string cow_filepath;
        SV_ULONGLONG cow_filesize;
        SV_UINT sequenceNumber = 0;

        if(!CDPUtil::ToSVTime(newts,svtime))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s offset: " ULLSPEC " invalid timestamp: " ULLSPEC "\n",
                        FUNCTION_NAME, read_voloffset, newts);
            rv = false;
            break;
        }

        if(!CDPUtil::ToTimestampinHrs(newts, newts_in_hrs))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s offset: " ULLSPEC " invalid timestamp: " ULLSPEC "\n",
                        FUNCTION_NAME, read_voloffset, newts);
            rv = false;
            break;
        }

        assert(newts_in_hrs == txn.m_bkmks.m_hr);
        if(newts_in_hrs != txn.m_bkmks.m_hr)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s timestamp mismatch txn time: " ULLSPEC " drtd time: " ULLSPEC "\n",
                        FUNCTION_NAME, txn.m_bkmks.m_hr, newts_in_hrs);
            rv = false;
            break;
        }

        memset(&old_iotime, 0, sizeof(cdp_timestamp_t));

        new_iotime.year = svtime.wYear - CDP_BASE_YEAR;
        new_iotime.month = svtime.wMonth;
        new_iotime.days = svtime.wDay;
        new_iotime.hours = svtime.wHour;
        new_iotime.events = txn.m_bkmks.m_bkmkcount;

        PerfTime bitmapread_start_time;
        getPerfTime(bitmapread_start_time);
        if(!m_cdpbitmap.get_and_set_ts(read_voloffset,new_iotime,old_iotime))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s bitmap operation failed for offset: " ULLSPEC " time: " ULLSPEC "\n",
                        FUNCTION_NAME, read_voloffset, newts);
            rv = false;
            break;
        }

        PerfTime bitmapread_end_time;
        getPerfTime(bitmapread_end_time);
        m_bitmapread_time += getPerfTimeDiff(bitmapread_start_time,bitmapread_end_time);


        cow_filename = get_cow_filename_v3(read_voloffset,	old_iotime,	new_iotime);
        cow_filepath = m_cdpdir;
        cow_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        cow_filepath += cow_filename;

        std::vector<std::string>::iterator file_iter = 
            find(cow_metadata.m_filenames.begin(), cow_metadata.m_filenames.end(),
                 cow_filename);

        while(file_iter == cow_metadata.m_filenames.end())
        {

            if(!get_cowdatafile_size_v3(cow_filepath, cow_filesize))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to get size for file %s\n",
                            FUNCTION_NAME, cow_filepath.c_str());
                rv = false;
                break;
            }

            if(!isInDiffSync() || (0 == m_maxcdpv3FileSize) || cow_filesize < m_maxcdpv3FileSize)
            {
                rv = true;
                break;
            }

            ++sequenceNumber;
            cow_filename = getNextCowFileName(old_iotime,new_iotime,sequenceNumber);
            cow_filepath = m_cdpdir;
            cow_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            cow_filepath += cow_filename;

            file_iter = find(cow_metadata.m_filenames.begin(), cow_metadata.m_filenames.end(),
                             cow_filename);
        }

        if(!rv)
            break;

        if(file_iter == cow_metadata.m_filenames.end())
        {
            cow_metadata.m_filenames.push_back(cow_filename);
            cow_metadata.m_baseoffsets.push_back(cow_filesize);
            cow_metadata.m_byteswritten.push_back(read_vollength);
            write_fileid = cow_metadata.m_filenames.size() - 1;
            write_offset = cow_metadata.m_baseoffsets[write_fileid];
		}
		else
        {
            write_fileid = file_iter - cow_metadata.m_filenames.begin();
            write_offset = cow_metadata.m_baseoffsets[write_fileid] + cow_metadata.m_byteswritten[write_fileid];
            cow_metadata.m_byteswritten[write_fileid] += read_vollength;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s vol.offset: " ULLSPEC "\n",
		FUNCTION_NAME, read_voloffset);
    return rv;
}

std::string CDPV2Writer::get_cow_filename_v3(const SV_OFFSET_TYPE & read_voloffset,
                                             const cdp_timestamp_t & old_iotime,
                                             const cdp_timestamp_t & new_iotime)
{
    // constructs the v3 data file name whose format is as below
    // cdpv30_<end time><end bookmark>_<file type(1 - overlap/ 0 - first write)>_
    //        <start time><start bookmark>_<sequence no>.dat
    // example: cdpv30_010422080010_1_010422080010_1234.dat
    // See bug# 11826 for more details

    bool overlap = (old_iotime == new_iotime);
    SV_UINT seq = 0;
    if(isInDiffSync())
    {
        seq = 0;
    } else
    {
        seq = ( read_voloffset / m_resync_chunk_size + 1);
    }	

    char filename[64];

	inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%02d%02d%02d%02d%04d_%d_%02d%02d%02d%02d%04d_%d%s",
            CDPV3_DATA_PREFIX.c_str(),new_iotime.year,new_iotime.month,new_iotime.days,new_iotime.hours,new_iotime.events,overlap,
            old_iotime.year,old_iotime.month,old_iotime.days,old_iotime.hours,old_iotime.events,seq,CDPV3_DATA_SUFFIX.c_str());

    return string(filename);
}

bool CDPV2Writer::get_cowdatafile_size_v3(const std::string & cow_filepath,
                                          SV_ULONGLONG & filesize)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s cow_filepath:%s\n",
		FUNCTION_NAME, cow_filepath.c_str());
    do
    {

        ACE_HANDLE handle = ACE_INVALID_HANDLE;
        ACE_Asynch_Write_File_Ptr wf_ptr;
        ACE_LOFF_T endoffset = 0;

        if(m_useAsyncIOs)
        {
            if(!get_cdpdata_async_writehandle(cow_filepath,handle,wf_ptr))
            {
                rv = false;
                break;
            }
		}
		else
        {
            if(!get_cdpdata_sync_writehandle(cow_filepath, handle))
            {
                rv = false;
                break;
            }
        }


        endoffset = ACE_OS::llseek(handle,0, SEEK_END);
        if(endoffset < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s seek to end of file %s failed. error: %d\n",
                        FUNCTION_NAME, cow_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        filesize = endoffset;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s cow_filepath:%s\n",
		FUNCTION_NAME, cow_filepath.c_str());
    return rv;
}


/*
 * FUNCTION NAME : construct_cow_location_v3
 *
 * DESCRIPTION : 
 *   
 *               
 * INPUT PARAMETERS : 
 *   
 *                    
 * OUTPUT PARAMETERS : 
 *   
 *
 * ALGORITHM :
 * 
 *    
 *    
 * return value : true if successfull and false otherwise
 *
 */
bool CDPV2Writer::construct_cow_location_v3(DiffPtr change_metadata, 
                                            SV_INT drtd_start,
                                            SV_INT drtd_end,
                                            const cdp_drtdv2_t & cow_drtd,
                                            cdp_cow_location_t & cow_locations)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);


    try
    {
        //__asm int 3;
        cdp_drtdv2_t drtd_to_match;
        cdp_drtdv2_t r_nonoverlap;
        cdp_drtdv2_t l_nonoverlap;
        cdp_cow_overlap_t overlap;
        std::queue<cdp_drtdv2_t> drtds_to_filter;

        SV_INT drtdno = drtd_end;
        cow_locations.overlaps.clear();
        cow_locations.nonoverlaps.clear();
        drtds_to_filter.push(cow_drtd);



        for( ; drtdno >= drtd_start; --drtdno)
        {

            unsigned int count = drtds_to_filter.size();
            if(0 == count)
                break;

            drtd_to_match = change_metadata -> drtd(drtdno);

            while(count > 0)
            {
                cdp_drtdv2_t drtd_to_filter = drtds_to_filter.front();
                drtds_to_filter.pop();
                --count;


                if(disjoint(drtd_to_match, drtd_to_filter))
                {
                    drtds_to_filter.push(drtd_to_filter);
                    continue;
                }

                if(contains(cow_drtd,drtd_to_match, drtd_to_filter,overlap))
                {
                    cow_locations.overlaps.push_back(overlap);
                    continue;
                }


                if(r_overlap(cow_drtd,drtd_to_match, drtd_to_filter,r_nonoverlap,overlap))
                {
                    drtds_to_filter.push(r_nonoverlap);
                    cow_locations.overlaps.push_back(overlap);
                    continue;
                }

                if(l_overlap(cow_drtd,drtd_to_match, drtd_to_filter, l_nonoverlap,overlap))
                {
                    drtds_to_filter.push(l_nonoverlap);
                    cow_locations.overlaps.push_back(overlap);
                    continue;
                }

                if(iscontained(cow_drtd,drtd_to_match, drtd_to_filter, l_nonoverlap, r_nonoverlap,overlap))
                {
                    if(0 != l_nonoverlap.get_length())
                    {
                        drtds_to_filter.push(l_nonoverlap);
					}
					else
                    {
                        DebugPrintf(SV_LOG_ERROR," Note: this is not an error: l_nonoverlap length = zero.\n");
                    }
                    if(0 != r_nonoverlap.get_length())
                    {
                        drtds_to_filter.push(r_nonoverlap);
					}
					else
                    {
                        DebugPrintf(SV_LOG_ERROR,"Note: this is not an error: r_nonoverlap length = zero.\n");
                    }

                    cow_locations.overlaps.push_back(overlap);
                    continue;
                }
            }
        }

        while(!drtds_to_filter.empty())
        {
            cdp_drtdv2_t drtd = drtds_to_filter.front();
            cdp_cow_nonoverlap_t nonoverlap;
            nonoverlap.relative_voloffset = drtd.get_volume_offset() - cow_drtd.get_volume_offset();
            nonoverlap.length = drtd.get_length();

            cow_locations.nonoverlaps.push_back(nonoverlap);
            drtds_to_filter.pop();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::write_metadata_v3(const std::string &metadata_filepath,
                                    const cdpv3metadata_t & cow_metadata)
{
    bool rv = true;
    ACE_HANDLE handle = ACE_INVALID_HANDLE;
    SV_OFFSET_TYPE baseoffset = -1;
    bool rollback = false;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s metadata_filepath:%s\n",
		FUNCTION_NAME, metadata_filepath.c_str());

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(mdfile_lock);

    do
    {
        //
        // local variables
        //
        SVD_PREFIX pfx;
        SV_UINT buflen = 0;
        SV_UINT skip_bytes = 0;
        SV_UINT delta = 0;
        SV_UINT i = 0;

        char * buffer = 0;
        SV_ULONG curpos = 0;


        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) (%s) while trying to acquire lock ",
                        FUNCTION_NAME,ACE_OS::last_error(),
                        ACE_OS::strerror(ACE_OS::last_error()));
            rv = false;
            break;
        }


        /*

          1. get the metadata file name
          2. open the metadata file name (if unbuffered i/o use file flag no buffering)
          3. seek to the end
          4. get the length of the metadata buffer
          5. allocate buffer
          6. fill buffer
          7. write buffer
          8. if buffering, flush  i/os
          9. close file

        */

        if(!get_cdpdata_sync_writehandle(metadata_filepath,handle))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s failed to open %s. error=%d\n",
                        FUNCTION_NAME, metadata_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        if(ACE_INVALID_HANDLE == handle)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s failed to open %s. error=%d\n",
                        FUNCTION_NAME, metadata_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        baseoffset = ACE_OS::llseek(handle, 0, SEEK_END);
        if(baseoffset < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s seek end of file failed on %s. error=%d\n",
                        FUNCTION_NAME, metadata_filepath.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        // get length of metadata buffer
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_TIMESTAMP_V2_SIZE;
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_TIMESTAMP_V2_SIZE;
        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_OTHR_INFO_SIZE;

        if(!cow_metadata.m_filenames.empty())
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += (cow_metadata.m_filenames.size() * SVD_FOST_INFO_SIZE);
        }

        if(!cow_metadata.m_drtds.empty())
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += (cow_metadata.m_drtds.size() * SVD_CDPDRTD_SIZE);
        }

        for( i = 0 ; i < cow_metadata.m_users.size(); ++i)
        {
            buflen += SVD_PREFIX_SIZE;
            buflen += cow_metadata.m_users[i] -> RawLength();
        }

        buflen += SVD_PREFIX_SIZE; // skip bytes

        buflen += SVD_PREFIX_SIZE;
        buflen += SVD_SOST_SIZE;

        delta = (buflen % m_sector_size);
        if(delta)
            skip_bytes = m_sector_size - delta;

        buflen += skip_bytes;

        SharedAlignedBuffer alignedbuf(buflen, m_sector_size);
        buffer = alignedbuf.Get();
		int buffer_size = alignedbuf.Size(); // To find buffer length

        // fill the tfv2
        SVD_TIME_STAMP_V2 tfv2;
        memset(&tfv2,0, SVD_TIMESTAMP_V2_SIZE);
        tfv2.TimeInHundNanoSecondsFromJan1601 = cow_metadata.m_tsfc;
        tfv2.SequenceNumber = cow_metadata.m_tsfcseq;

        FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2, 1, 0);
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &tfv2, SVD_TIMESTAMP_V2_SIZE);
        curpos += SVD_TIMESTAMP_V2_SIZE;

        // fill the tlv2
        SVD_TIME_STAMP_V2 tlv2;
        memset(&tlv2,0, SVD_TIMESTAMP_V2_SIZE);
        tlv2.TimeInHundNanoSecondsFromJan1601 = cow_metadata.m_tslc;
        tlv2.SequenceNumber = cow_metadata.m_tslcseq;

        FILL_PREFIX(pfx, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2, 1, 0);
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &tlv2, SVD_TIMESTAMP_V2_SIZE);
        curpos += SVD_TIMESTAMP_V2_SIZE;

        // fill othr info
        FILL_PREFIX(pfx, SVD_TAG_OTHR, 1, 0);
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &(cow_metadata.m_type), SVD_OTHR_INFO_SIZE);
        curpos += SVD_OTHR_INFO_SIZE;

        // fill cdp data files
        if(!cow_metadata.m_filenames.empty())
        {
            FILL_PREFIX(pfx, SVD_TAG_FOST, cow_metadata.m_filenames.size(), 0);
			inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            for( i = 0; i < cow_metadata.m_filenames.size(); ++i)
            {
                SVD_FOST_INFO fost;
                memset(fost.filename, '\0', SVD_FOST_FNAME_SIZE);
                inm_strncpy_s(fost.filename, ARRAYSIZE(fost.filename), cow_metadata.m_filenames[i].c_str(),SVD_FOST_FNAME_SIZE);
                fost.startoffset = cow_metadata.m_baseoffsets[i];
				inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &fost, SVD_FOST_INFO_SIZE);
                curpos += SVD_FOST_INFO_SIZE;
            }
        }

        // fill drtds
        if(!cow_metadata.m_drtds.empty())
        {
            FILL_PREFIX(pfx, SVD_TAG_CDPD, cow_metadata.m_drtds.size(), 0);
			inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
            for( i = 0; i < cow_metadata.m_drtds.size(); ++i)
            {
                SVD_CDPDRTD drtd;
                drtd.voloffset = cow_metadata.m_drtds[i].get_volume_offset();
                drtd.length = cow_metadata.m_drtds[i].get_length();
                drtd.fileid = cow_metadata.m_drtds[i].get_fileid();
                drtd.uiSequenceNumberDelta = cow_metadata.m_drtds[i].get_seqdelta();
                drtd.uiTimeDelta = cow_metadata.m_drtds[i].get_timedelta();
				inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &drtd, SVD_CDPDRTD_SIZE);
                curpos += SVD_CDPDRTD_SIZE;
            }
        }

        // fill bookmarks
        for( i = 0 ; i < cow_metadata.m_users.size(); ++i)
        {
            FILL_PREFIX(pfx, SVD_TAG_USER, 1,  cow_metadata.m_users[i] -> RawLength());
			inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
            curpos += SVD_PREFIX_SIZE;
			inm_memcpy_s(buffer + curpos, (buffer_size - curpos), cow_metadata.m_users[i]->RawData(),
                   cow_metadata.m_users[i] -> RawLength());
            curpos += cow_metadata.m_users[i] -> RawLength();
        }

        // fill skip bytes
        FILL_PREFIX(pfx, SVD_TAG_SKIP, skip_bytes, 0);
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
        curpos += skip_bytes;

        // fill sost information
        FILL_PREFIX(pfx, SVD_TAG_SOST, 1, 0);
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &pfx, SVD_PREFIX_SIZE);
        curpos += SVD_PREFIX_SIZE;
		inm_memcpy_s(buffer + curpos, (buffer_size - curpos), &baseoffset, SVD_SOST_SIZE);
        curpos += SVD_SOST_SIZE;


        // write metadata
        unsigned int byteswrote = ACE_OS::write(handle, buffer, buflen);
        if(byteswrote != buflen)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s write to file %s failed. byteswrote: %d, expected:%d. error=%d\n",
                        FUNCTION_NAME, metadata_filepath.c_str(), byteswrote, buflen, ACE_OS::last_error());

            rollback = true;
            rv = false;
            break;
        }

        if(!flush_io(handle, metadata_filepath))
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s flush on %s failed.\n",
                        FUNCTION_NAME, metadata_filepath.c_str());

            rollback = true;
            rv = false;
            break;
        }

    } while (0);

    if(ACE_INVALID_HANDLE != handle)
    {
        if(!m_cache_cdpfile_handles)
        {
            close_cdpdata_syncwrite_handles();
        }
    }

    if(rollback)
    {
        DebugPrintf(SV_LOG_DEBUG, "FUNCTION: %S rolling back partial write to %s\n",
                    FUNCTION_NAME,metadata_filepath.c_str());

        rollback_partial_mdwrite_v3(metadata_filepath, baseoffset);
    }

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s metadata_filepath:%s\n",
		FUNCTION_NAME, metadata_filepath.c_str());
    return rv;
}

void CDPV2Writer::dump_metadata_v3(const std::string & volumename,const std::string & filename,
                                   DiffPtr change_metadata)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    do
    {
        stringstream dmpstr;
        int i =0;
        dmpstr << "METADATA:" << '\n'
               << change_metadata -> starttime() << ' '
               << change_metadata -> StartTimeSequenceNumber() << ' '
               << change_metadata -> endtime() << ' '
               << change_metadata -> EndTimeSequenceNumber() << '\n';

        dmpstr << filename << '\n';

        UserTagsIterator uiter = change_metadata -> UserTagsBegin();
        UserTagsIterator uend = change_metadata -> UserTagsEnd();

        for ( /* empty */ ; uiter != uend; ++uiter)	
        {
            SvMarkerPtr usertag = *uiter;
            std::string bookmark = usertag -> Tag().get();
            dmpstr << "bookmark: " << bookmark << '\n';
        }

        for ( i = 0;  i < change_metadata -> NumDrtds() ; ++i)
        {
            dmpstr << "drtd:" << ' '
                   << change_metadata -> drtd(i).get_volume_offset() << ' '
                   << change_metadata -> drtd(i).get_length() << ' '
                   << change_metadata -> drtd(i).get_file_offset() << ' '
                   << change_metadata -> drtd(i).get_fileid() << ' '
                   << change_metadata -> drtd(i).get_timedelta() << ' '
                   << change_metadata -> drtd(i).get_seqdelta() << '\n';
        }

        DebugPrintf(SV_LOG_DEBUG, "%s\n", dmpstr.str().c_str());

#if 0
        std::string volume = volumename;
        replace_nonsupported_chars(volume);


        std::string debuginfoPath = m_installPath;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += "debug";
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += volume;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;


        if(SVMakeSureDirectoryPathExists(debuginfoPath.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, debuginfoPath.c_str());
            rv = false;
            break;
        }

        debuginfoPath += isInDiffSync() ? "diffsync.txt" : "resync.txt";

        // PR#10815: Long Path support
        std::ofstream out(getLongPathName(debuginfoPath.c_str()).c_str(), std::ios::app);


        if(!out) 
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                        FUNCTION_NAME, debuginfoPath.c_str(), LINE_NO,FILE_NAME);
            rv = false;
            break;
        }

        out << dmpstr.str() << "\n";
#endif


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void CDPV2Writer::dump_metadata_v3(const std::string & volumename, const cdpv3metadata_t & md)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    do
    {
        stringstream dmpstr;
        int i =0;
        dmpstr << "METADATA:" << '\n'
               << md.m_tsfc << ' '
               << md.m_tsfcseq << ' '
               << md.m_tslc << ' '
               << md.m_tslcseq << '\n';

        for ( i = 0; i < md.m_filenames.size() ; ++i )
        {
            dmpstr << md.m_filenames[i] << ' '
                   << md.m_baseoffsets[i] << ' '
                   << md.m_byteswritten[i] << '\n';
        }

        for ( i = 0;  i < md.m_drtds.size() ; ++i)
        {
            dmpstr << "drtd:" << ' '
                   << md.m_drtds[i].get_volume_offset() << ' '
                   << md.m_drtds[i].get_length() << ' '
                   << md.m_drtds[i].get_file_offset() << ' '
                   << md.m_drtds[i].get_fileid() << ' '
                   << md.m_drtds[i].get_timedelta() << ' '
                   << md.m_drtds[i].get_seqdelta() << '\n';
        }

        DebugPrintf(SV_LOG_DEBUG, "%s\n", dmpstr.str().c_str());

#if 0
        std::string volume = volumename;
        replace_nonsupported_chars(volume);


        std::string debuginfoPath = m_installPath;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += "debug";
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += volume;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;


        if(SVMakeSureDirectoryPathExists(debuginfoPath.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, debuginfoPath.c_str());
            rv = false;
            break;
        }

        debuginfoPath += isInDiffSync() ? "diffsync.txt" : "resync.txt";

        // PR#10815: Long Path support
        std::ofstream out(getLongPathName(debuginfoPath.c_str()).c_str(), std::ios::app);


        if(!out) 
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                        FUNCTION_NAME, debuginfoPath.c_str(), LINE_NO,FILE_NAME);
            rv = false;
            break;
        }

        out << dmpstr.str() << "\n";
#endif


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void CDPV2Writer::dump_cowlocations(const std::string & volumename,const cdpv3metadata_t & md)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    do
    {
        stringstream dmpstr;
        int i =0;
        dmpstr << "COW LOCATIONS:" << '\n';

        for ( i = 0;  i < md.m_drtds.size() ; ++i)
        {
            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(i);
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, i);
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_overlap_t>::const_iterator ov_iter = cowlocations.overlaps.begin();

            for( ; ov_iter != cowlocations.overlaps.end(); ++ov_iter)
            {
                cdp_cow_overlap_t overlap = *ov_iter;
                dmpstr << "overlap " << i << ":" << overlap.relative_voloffset << ' ' 
                       << overlap.length << ' '
                       << overlap.fileoffset << '\n' ;
            }

            std::vector<cdp_cow_nonoverlap_t>::const_iterator nov_iter = cowlocations.nonoverlaps.begin();
            for( ; nov_iter != cowlocations.nonoverlaps.end(); ++ nov_iter)
            {
                cdp_cow_nonoverlap_t nonoverlap = *nov_iter;
                dmpstr << "nonoverlap " << i << ":" << nonoverlap.relative_voloffset << ' ' 
                       << nonoverlap.length << '\n';
            }
        }


        DebugPrintf(SV_LOG_DEBUG, "%s\n", dmpstr.str().c_str());

#if 0
        std::string volume = volumename;
        replace_nonsupported_chars(volume);


        std::string debuginfoPath = m_installPath;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += "debug";
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        debuginfoPath += volume;
        debuginfoPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;


        if(SVMakeSureDirectoryPathExists(debuginfoPath.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, debuginfoPath.c_str());
            rv = false;
            break;
        }

        debuginfoPath += isInDiffSync() ? "diffsync.txt" : "resync.txt";

        // PR#10815: Long Path support
        std::ofstream out(getLongPathName(debuginfoPath.c_str()).c_str(), std::ios::app);


        if(!out) 
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                        FUNCTION_NAME, debuginfoPath.c_str(), LINE_NO,FILE_NAME);
            rv = false;
            break;
        }

        out << dmpstr.str() << "\n";
#endif

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}



// ==============================================
// volume metadata routines
// ==============================================

bool CDPV2Writer::get_volmetadata_for_timerange_v3(const SV_ULONGLONG &current_part_hrstart, 
                                                   const SV_ULONGLONG &current_part_hrend,
                                                   DiffPtr change_metadata,
                                                   cdpv3metadata_t &vol_metadata)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, 
		"ENTERED %s timerange [" ULLSPEC " - " ULLSPEC "]\n",
		FUNCTION_NAME, current_part_hrstart, current_part_hrend);

    do
    {
        vol_metadata.clear();

        //
        // changes that have to be skipped
        // as they belong to previous parts
        // 
        SV_UINT changes_skipped = 0;

        //
        // difference between current part tsfc/seq and
        // differential tsfc/seq
        //
        SV_INT seq_difference = 0;
        SV_INT time_difference = 0;

        // 
        // local variables
        //
        SV_OFFSET_TYPE aligned_offset = -1;
        SV_UINT aligned_length = 0;
        SV_INT seq_delta = 0;
        SV_INT time_delta = 0;
        SV_OFFSET_TYPE  write_voloffset = -1;
        SV_UINT         write_vollength = 0;
        SV_OFFSET_TYPE  read_offset = -1;
        SV_UINT         read_fileid = 0;
        SV_UINT delta = 0;
        SV_UINT pending_length = 0;
        SV_UINT block_alignment = m_cdpbitmap.get_block_size();

        cdp_drtdv2s_iter_t drtds_iter = change_metadata -> DrtdsBegin();
        cdp_drtdv2s_iter_t drtds_end  = change_metadata -> DrtdsEnd();
        cdp_drtdv2s_iter_t drtds_start = drtds_iter;

        //
        // skip changes which are from previous parts
        //
        for( ; drtds_iter != drtds_end ; ++drtds_iter)
        {
            if((change_metadata -> starttime() + drtds_iter ->get_timedelta())
               < current_part_hrstart)
            {
                ++changes_skipped;
			}
			else
            {
                break;
            }
        }


        // determine the tsfc, tsfcseq
        if(0 == changes_skipped)
        {
            vol_metadata.m_tsfc = change_metadata -> starttime();
            vol_metadata.m_tsfcseq = change_metadata -> StartTimeSequenceNumber();

		}
		else if (drtds_iter == drtds_end)
        {
            vol_metadata.m_tsfc = change_metadata -> endtime();
            vol_metadata.m_tsfcseq = change_metadata -> EndTimeSequenceNumber();
		}
		else
        {
            vol_metadata.m_tsfc = change_metadata -> starttime() + drtds_iter ->get_timedelta();
            vol_metadata.m_tsfcseq = change_metadata -> StartTimeSequenceNumber() + drtds_iter ->get_seqdelta();
        }

        seq_difference = vol_metadata.m_tsfcseq - change_metadata -> StartTimeSequenceNumber();
        time_difference = vol_metadata.m_tsfc - change_metadata -> starttime();

        //
        // add bookmarks if it is the first part
        //
        if(0 == changes_skipped)
        {
            UserTagsIterator udtsIter = change_metadata -> UserTagsBegin();
            UserTagsIterator udtsEnd  = change_metadata -> UserTagsEnd();
            for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
            {
                SvMarkerPtr pMarker = *udtsIter;
                vol_metadata.m_users.push_back(pMarker);
            }
        }

        drtds_start = drtds_iter;

        for ( /* empty */ ; drtds_iter != drtds_end ; ++drtds_iter )
        {
            cdp_drtdv2_t change_drtd = *drtds_iter;

            if(change_metadata -> starttime() + change_drtd.get_timedelta() >= current_part_hrend)
                break;
        }

        if(!construct_nonoverlapping_drtds(drtds_start, drtds_iter, vol_metadata.m_drtds))
        {
            rv = false;
            break;
        }


        if(drtds_iter == drtds_end)
        {
            vol_metadata.m_tslc = change_metadata -> endtime();
            vol_metadata.m_tslcseq = change_metadata -> EndTimeSequenceNumber();
		}
		else
        {
            vol_metadata.m_tslc = vol_metadata.m_tsfc + drtds_iter ->get_timedelta();
            vol_metadata.m_tslcseq = vol_metadata.m_tsfcseq + drtds_iter ->get_seqdelta();
        }

        vol_metadata.m_type = 0;
        if(change_metadata -> type() == RESYNC_MODE)
        {
            vol_metadata.m_type = 0;
		}
		else if (change_metadata->type() == OUTOFORDER_TS)
        {
            vol_metadata.m_type = 0; 
		}
		else
        {
            vol_metadata.m_type |= DIFFTYPE_DIFFSYNC_BIT_SET;

            if(change_metadata -> type() == NONSPLIT_IO)
            {
                vol_metadata.m_type |= DIFFTYPE_NONSPLITIO_BIT_SET;
            }

            if(change_metadata -> version() >= SVD_PERIOVERSION)
            {
                vol_metadata.m_type |= DIFFTYPE_PERIO_BIT_SET;
            }
        }


    } while (0);

    DebugPrintf(SV_LOG_DEBUG, 
		"EXITED %s timerange [" ULLSPEC " - " ULLSPEC "]\n",
		FUNCTION_NAME, current_part_hrstart, current_part_hrend);
    return rv;
}

// =============================================
// volume and cow write routines
// =============================================

bool CDPV2Writer::update_cow_v3(const std::string &volumename,
                                const std::string & diffpath, 
                                char * diff_data,
                                const cdpv3metadata_t & cow_metadata)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s volume:%s diffpath:%s \n",
		FUNCTION_NAME, volumename.c_str(), diffpath.c_str());

    do
    {

        cdp_drtdv2s_constiter_t cowdrtds_iter = cow_metadata.m_drtds.begin();

        assert(0 == m_pendingios);
        if(0 != m_pendingios)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption 0 == m_pendingios failed. m_pendingios: %d\n",
                        FUNCTION_NAME, m_pendingios);
            rv = false;
            break;
        }

        assert(false == m_error);
        if(false != m_error)
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption false == m_error failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        //assert(0 == m_memconsumed_for_io);
        //if(0 != m_memconsumed_for_io)
        //{
        //	DebugPrintf(SV_LOG_ERROR, 
        //		"FUNCTION:%s assumption 0 == m_memconsumed_for_io failed. m_memconsumed_for_io: %d\n",
        //		FUNCTION_NAME, m_memconsumed_for_io);
        //	rv = false;
        //	break;
        //}

        assert(m_buffers_allocated_for_io.empty());
        if(!m_buffers_allocated_for_io.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_buffers_allocated_for_io.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_asyncwrite_handles.empty());
        if(!m_asyncwrite_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_asyncwrite_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_syncwrite_handles.empty());
        if(!m_syncwrite_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_syncwrite_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_asyncread_handles.empty());
        if(!m_asyncread_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_asyncread_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        assert(m_syncread_handles.empty());
        if(!m_syncread_handles.empty())
        {
            DebugPrintf(SV_LOG_ERROR, 
                        "FUNCTION:%s assumption m_syncread_handles.empty() failed.\n",
                        FUNCTION_NAME);
            rv = false;
            break;
        }

        m_memtobefreed_on_io_completion = false;
        m_cdp_data_filepaths.clear();

        for(unsigned int i = 0 ; i < cow_metadata.m_filenames.size() ; ++i)
        {
            std::string cow_filepath = m_cdpdir;
            cow_filepath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            cow_filepath += cow_metadata.m_filenames[i];
            m_cdp_data_filepaths.push_back(cow_filepath);
        }

        if(m_isFileInMemory)
        {
            if(m_useAsyncIOs)
            {
                rv = updatecow_asynch_inmem_difffile_v3(volumename, diff_data,
                                                        cow_metadata.m_drtds);
            }
            else
            {
                rv = updatecow_synch_inmem_difffile_v3(volumename, diff_data,
                                                       cow_metadata.m_drtds);
            }
        }
        else
        {
            if(m_useAsyncIOs)
            {
                rv = updatecow_asynch_ondisk_difffile_v3(volumename, diffpath,
                                                         cow_metadata.m_drtds);
            }
            else
            {
                rv = updatecow_synch_ondisk_difffile_v3(volumename, diffpath,
                                                        cow_metadata.m_drtds);
            }
        }

        if(rv)
        {
            PerfTime vsnapupdate_start_time;
            getPerfTime(vsnapupdate_start_time);

            updatevsnaps_v3(volumename, cow_metadata.m_drtds.begin(),cow_metadata.m_drtds.end());

            PerfTime vsnapupdate_end_time;
            getPerfTime(vsnapupdate_end_time);
            m_vsnapupdate_time = getPerfTimeDiff(vsnapupdate_start_time,vsnapupdate_end_time);
        }


    }while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s volume:%s diffpath:%s \n",
		FUNCTION_NAME, volumename.c_str(), diffpath.c_str());
    return rv;
}

// =============================================================
//
// Asynchronous COW update routines:
// 
//  updatecow_asynch_inmem_difffile_v3
//    read_async_cowdrtds_inmem_difffile_v3
//       read_async_cowdata_inmem_difffile_v3
//    write_async_cowdata
//       get_cows_belonging_to_same_file
//          get_cdp_data_filepath
//       get_cdpdata_async_writehandle
//
// =============================================================



bool CDPV2Writer::updatecow_asynch_inmem_difffile_v3(const std::string &volumename, 
                                                     char * diff_data,
                                                     const cdp_drtdv2s_t & drtds)
{
    bool rv = true;
    SV_UINT drtd_no = 0;

    PerfTime volread_start_time;
    PerfTime volread_end_time;
    PerfTime cowwrite_start_time;
    PerfTime cowwrite_end_time;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            cdp_drtdv2s_constiter_t drtdsIter = drtds.begin();

            if (drtds.empty())
            {
                rv = true;
                break;
            }

            while(drtdsIter != drtds.end())          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());

                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t drtds_to_read_begin;
                cdp_drtdv2s_constiter_t drtds_to_read_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                drtds.end(), 
                                                maxUsableMemory, 
                                                drtds_to_read_begin,
                                                drtds_to_read_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }


                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                getPerfTime(volread_start_time);

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, drtds_to_read_begin,drtds_to_read_end);
                }

                if(!read_async_cowdrtds_inmem_difffile_v3(volumename, diff_data,
                                                          pcowdata.Get(),	drtds_to_read_begin,drtds_to_read_end,drtd_no, pcowdata.Size()))
                {
                    rv = false;
                    break;
                }

                getPerfTime(volread_end_time);
                m_volread_time += getPerfTimeDiff(volread_start_time, volread_end_time);

                getPerfTime(cowwrite_start_time);

                if(!write_async_cowdata(pcowdata.Get(), drtds_to_read_begin,drtds_to_read_end))
                {
                    rv = false;
                    break;
                }

                getPerfTime(cowwrite_end_time);
                m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);


                drtdsIter = drtds_to_read_end;
            }

        } while (0);

        getPerfTime(cowwrite_start_time);

        // flush all open cdp data files and close
        //Performance Improvement - 

        if(!flush_cdpdata_asyncwrite_handles())
            rv = false;

        if(!m_cache_cdpfile_handles)
        {
            close_cdpdata_asyncwrite_handles();
        }

        getPerfTime(cowwrite_end_time);
        m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

        if(!close_asyncread_handles())
            rv =false;  


    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_async_cowdrtds_inmem_difffile_v3(const std::string & volumename,
                                                        const char * diff_data,
                                                        char * cow_data,
                                                        const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                                        const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                                        SV_UINT & drtd_counter, int size)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {


        SV_UINT drtd_no = drtd_counter;
        SV_ULONG bytesRead = 0;
        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;
        cdp_drtdv2s_fileid_t drtdv2set;

        for( ; drtdsIter != drtds_to_read_end ; ++drtdsIter)
        {
            cdp_drtdv2_fileid_t drtd_fileid((*drtdsIter).get_length(),
                                            (*drtdsIter).get_volume_offset(),
                                            (*drtdsIter).get_file_offset(),
                                            (*drtdsIter).get_fileid(),
                                            drtd_no);

            drtdv2set.insert(drtd_fileid);
            ++drtd_no;
        }

        drtd_counter = drtd_no;

        cdp_drtdv2s_fileid_constiter_t cow_drtds_iter = drtdv2set.begin();
        cdp_drtdv2s_fileid_constiter_t cow_drtds_end = drtdv2set.end();

        for( ; cow_drtds_iter != cow_drtds_end ; ++cow_drtds_iter)
        {
            cdp_drtdv2_fileid_t drtd = *cow_drtds_iter;
			int size_len = size - bytesRead;
            if(!read_async_cowdata_inmem_difffile_v3(volumename,
                                                     diff_data,
                                                     cow_data + bytesRead,
													 drtd, size_len))
            {
                rv = false;
                break;
            }
            bytesRead += drtd.get_length();
        }

        if(!wait_for_xfr_completion())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_async_cowdata_inmem_difffile_v3(const std::string & volumename,
                                                       const char * diff_data,
                                                       char * cow_data,
                                                       const cdp_drtdv2_fileid_t & drtd, int cow_data_size)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s drtd_no:%d offset:" LLSPEC "\n",
		FUNCTION_NAME, drtd.get_drtdno(), drtd.get_volume_offset());

    try
    {
        do
        {
            SV_UINT bytes_copied = 0;

            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(drtd.get_drtdno());
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, drtd.get_drtdno());
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_overlap_t>::const_iterator ov_iter = cowlocations.overlaps.begin();

            for( ; ov_iter != cowlocations.overlaps.end(); ++ov_iter)
            {
                cdp_cow_overlap_t overlap = *ov_iter;
				inm_memcpy_s(cow_data + overlap.relative_voloffset, (cow_data_size - overlap.relative_voloffset), diff_data + overlap.fileoffset, overlap.length);
                bytes_copied +=  overlap.length;
            }

            std::vector<cdp_cow_nonoverlap_t>::const_iterator nov_iter = cowlocations.nonoverlaps.begin();
            for( ; nov_iter != cowlocations.nonoverlaps.end(); ++nov_iter)
            {
                cdp_cow_nonoverlap_t nonoverlap = *nov_iter;

                if(m_ismultisparsefile)
                {
                    if(!read_async_drtd_from_multisparsefile((char *) (cow_data + nonoverlap.relative_voloffset), 
                                                             drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                             nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }

                } else
                {

                    if(!canIssueNextIo(0))
                    {
                        rv = false;
                        break;
                    }

                    if(!initiate_next_async_read(*m_rfptr.get(), 
                                                 (char *) (cow_data + nonoverlap.relative_voloffset), 
                                                 nonoverlap.length,
                                                 drtd.get_volume_offset() + nonoverlap.relative_voloffset))
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                                    "Function:%s failed in initiate_next_async_read. relative_offset:%d. \n",
                                    FUNCTION_NAME,  nonoverlap.relative_voloffset);
                        rv = false;
                        break;
                    }
                }

                bytes_copied += nonoverlap.length;
            }

            if(!rv)
                break;

            assert(bytes_copied == drtd.get_length());
            if(bytes_copied != drtd.get_length())
            {
                DebugPrintf(SV_LOG_ERROR,
                            "Function:%s assumption bytes_copied (%d) == drtd.get_length()(%d) failed. \n",
                            FUNCTION_NAME, bytes_copied, drtd.get_length());
                rv = false;
                break;
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


bool CDPV2Writer::write_async_cowdata(const char * cow_data,
                                      const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                                      const cdp_drtdv2s_constiter_t & drtds_to_write_end)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_UINT mem_required = 0;
        std::string cdp_data_filepath;
        SV_OFFSET_TYPE cow_offset = -1;
        SV_UINT cow_length = 0;
        SV_UINT cow_length_consumed =0;	
        SV_UINT drtd_no = 0;
        cdp_drtdv2s_fileid_t drtdv2set;

        m_memtobefreed_on_io_completion = false;

        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_write_begin;
        for( ; drtdsIter != drtds_to_write_end ; ++drtdsIter)
        {
            cdp_drtdv2_fileid_t drtd_fileid((*drtdsIter).get_length(),
                                            (*drtdsIter).get_volume_offset(),
                                            (*drtdsIter).get_file_offset(),
                                            (*drtdsIter).get_fileid(),
                                            drtd_no);

            drtdv2set.insert(drtd_fileid);
            ++drtd_no;
        }

        cdp_drtdv2s_fileid_constiter_t cow_drtds_iter = drtdv2set.begin();
        cdp_drtdv2s_fileid_constiter_t cow_drtds_samefile_iter;
        cdp_drtdv2s_fileid_constiter_t cow_drtds_samefile_end;

        while(cow_drtds_iter != drtdv2set.end())
        {

            cdp_data_filepath.clear();
            cow_length =0;
            cow_offset =-1;

            if(!get_cows_belonging_to_same_file(cow_drtds_iter,drtdv2set.end(),
                                                cow_drtds_samefile_iter,
                                                cow_drtds_samefile_end,
                                                cdp_data_filepath, 
                                                cow_offset,
                                                cow_length))
            {
                rv = false;
                break;
            }

            ACE_HANDLE cow_handle = ACE_INVALID_HANDLE;
            ACE_Asynch_Write_File_Ptr wf_ptr;
            if(!get_cdpdata_async_writehandle(cdp_data_filepath, cow_handle, wf_ptr))
            {
                rv = false;
                break;
            }

            if(!canIssueNextIo(mem_required))
            {
                rv = false;
                break;
            }

            if(!initiate_next_async_write(wf_ptr, 
                                          cdp_data_filepath, cow_data + cow_length_consumed,
                                          cow_length, cow_offset))
            {
                rv = false;
                break;
            }

            cow_drtds_iter = cow_drtds_samefile_end; 
            cow_length_consumed += cow_length;
        }

        if(!wait_for_xfr_completion())
            rv = false;
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::get_cows_belonging_to_same_file(const cdp_drtdv2s_fileid_constiter_t & cow_drtds_begin,
                                                  const cdp_drtdv2s_fileid_constiter_t &cow_drtds_end,
                                                  cdp_drtdv2s_fileid_constiter_t &cow_drtds_samefile_begin,
                                                  cdp_drtdv2s_fileid_constiter_t &cow_drtds_samefile_end,
                                                  std::string & cdp_data_filepath, 
                                                  SV_OFFSET_TYPE & cow_offset,
                                                  SV_UINT &cow_length)
{
    bool rv = true;

    do
    {
        std::string curr_data_file;
        SV_UINT fileid = 0;
        if(cow_drtds_begin == cow_drtds_end)
        {
            rv = false;
            break;
        }

        cdp_data_filepath.clear();
        cow_drtds_samefile_begin = cow_drtds_begin;
        cow_drtds_samefile_end = cow_drtds_begin;
        cow_offset = cow_drtds_samefile_begin ->get_file_offset();
        cow_length = cow_drtds_samefile_begin ->get_length();

        if(!get_cdp_data_filepath(cow_drtds_samefile_begin ->get_fileid(), cdp_data_filepath))
        {
            rv = false;
            break;
        }

        fileid = cow_drtds_samefile_begin ->get_fileid();
        ++cow_drtds_samefile_end;
        while (cow_drtds_samefile_end != cow_drtds_end)
        {

            if(cow_drtds_samefile_end ->get_fileid() != fileid)
            {
                break;
            }

            cow_length += cow_drtds_samefile_end -> get_length();
            ++cow_drtds_samefile_end;
        }


        if(!rv)
            break;
    }while (0);

    return rv;
}

bool CDPV2Writer::get_cdp_data_filepath(const SV_UINT & idx, std::string & filepath)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    do
    {

        if(idx >= m_cdp_data_filepaths.size())
        {
            rv = false;
            break;
        }
        filepath = m_cdp_data_filepaths[idx];

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::get_async_writehandle(const std::string & filename, 
                                        ACE_HANDLE & handle,
                                        ACE_Asynch_Write_File_Ptr & wf_ptr)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_asyncwritehandles_t::iterator handle_iter = m_asyncwrite_handles.find(filename);
        if(handle_iter == m_asyncwrite_handles.end())
        {
            int openMode = O_CREAT | O_WRONLY | FILE_FLAG_OVERLAPPED;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }

            setumask();

            //Setting appropriate share on retention file
            mode_t share;
            setsharemode_for_all(share);

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode, share);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            wf_ptr.reset(new ACE_Asynch_Write_File);

            if(wf_ptr ->open (*this, handle) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "asynch open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                ACE_OS::close(handle);
                handle = ACE_INVALID_HANDLE;
                rv = false;
                break;
            }

            cdp_async_writehandle_t value(handle, wf_ptr);
            m_asyncwrite_handles.insert(std::make_pair(filename, value));
		}
		else
        {
            handle = handle_iter ->second.handle;
            wf_ptr = handle_iter ->second.asynch_handle;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}



// =========================================================
//
// Asynchronous COW update routines:
//
//  updatecow_asynch_ondisk_difffile_v3
//    read_async_cowdrtds_ondisk_difffile_v3
//      read_async_cowdata_ondisk_difffile_v3
//    write_async_cowdata
//       get_cows_belonging_to_same_file
//          get_cdp_data_filepath
//       get_async_writehandle
//
// =========================================================


bool CDPV2Writer::updatecow_asynch_ondisk_difffile_v3(const std::string &volumename, 
                                                      const std::string & diffpath,
                                                      const cdp_drtdv2s_t & drtds)
{
    bool rv = true;

    BasicIo diffBIo;
    SV_UINT drtd_no = 0;
    PerfTime volread_start_time;
    PerfTime volread_end_time;
    PerfTime cowwrite_start_time;
    PerfTime cowwrite_end_time;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            cdp_drtdv2s_constiter_t drtdsIter = drtds.begin();

            if (drtds.empty())
            {
                rv = true;
                break;
            }

            BasicIo::BioOpenMode diff_mode =
                (BasicIo::BioReadExisting | 
                 BasicIo::BioShareRead | 
                 BasicIo::BioShareWrite | 
                 BasicIo::BioShareDelete);

            diffBIo.Open(diffpath, diff_mode);

            if(!diffBIo.Good())
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            diffpath.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            while(drtdsIter != drtds.end())          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());

                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t drtds_to_read_begin;
                cdp_drtdv2s_constiter_t drtds_to_read_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                drtds.end(), 
                                                maxUsableMemory, 
                                                drtds_to_read_begin,
                                                drtds_to_read_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }


                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;



                getPerfTime(volread_start_time);

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, drtds_to_read_begin,drtds_to_read_end);
                }

                if(!read_async_cowdrtds_ondisk_difffile_v3(volumename,
                                                           diffBIo, pcowdata.Get(),
                                                           drtds_to_read_begin,drtds_to_read_end,drtd_no))
                {
                    rv = false;
                    break;
                }

                getPerfTime(volread_end_time);
                m_volread_time += getPerfTimeDiff(volread_start_time, volread_end_time);

                getPerfTime(cowwrite_start_time);

                if(!write_async_cowdata(pcowdata.Get(), drtds_to_read_begin,drtds_to_read_end))
                {
                    rv = false;
                    break;
                }

                getPerfTime(cowwrite_end_time);
                m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

                drtdsIter = drtds_to_read_end;
            }

        } while (0);

        getPerfTime(cowwrite_start_time);

        // flush all open cdp data files and close
        //Performance Improvement - 

        if(!flush_cdpdata_asyncwrite_handles())
            rv = false;


        if(!m_cache_cdpfile_handles)
        {
            close_cdpdata_asyncwrite_handles();
        }

        getPerfTime(cowwrite_end_time);
        m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

        if(!close_asyncread_handles())
            rv =false;  


        if(diffBIo.isOpen())
        {
            diffBIo.Close();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_async_cowdrtds_ondisk_difffile_v3(const std::string &volumename, 
                                                         BasicIo & diffBio,
                                                         char * cow_data,
                                                         const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                                         const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                                         SV_UINT & drtd_counter)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {

            SV_ULONG bytesRead = 0;
            cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;
            SV_UINT drtd_no = drtd_counter;
            cdp_drtdv2s_fileid_t drtdv2set;

            for( ; drtdsIter != drtds_to_read_end ; ++drtdsIter)
            {
                cdp_drtdv2_fileid_t drtd_fileid((*drtdsIter).get_length(),
                                                (*drtdsIter).get_volume_offset(),
                                                (*drtdsIter).get_file_offset(),
                                                (*drtdsIter).get_fileid(),
                                                drtd_no);

                drtdv2set.insert(drtd_fileid);
                ++drtd_no;
            }

            drtd_counter = drtd_no;

            cdp_drtdv2s_fileid_constiter_t cow_drtds_iter = drtdv2set.begin();
            cdp_drtdv2s_fileid_constiter_t cow_drtds_end = drtdv2set.end();

            for( ; cow_drtds_iter != cow_drtds_end ; ++cow_drtds_iter)
            {
                cdp_drtdv2_fileid_t drtd = *cow_drtds_iter;

                if(!read_cowoverlapdata_ondisk_difffile_v3(diffBio,
                                                           cow_data + bytesRead,
                                                           drtd))
                {
                    rv = false;
                    break;
                }

                bytesRead += drtd.get_length();
            }

            if(!rv)
                break;

            cow_drtds_iter = drtdv2set.begin();
            bytesRead = 0;
            for( ; cow_drtds_iter != cow_drtds_end ; ++cow_drtds_iter)
            {
                cdp_drtdv2_fileid_t drtd = *cow_drtds_iter;

                if(!read_async_cownonoverlapdata_v3(volumename,
                                                    cow_data + bytesRead,
                                                    drtd))
                {
                    rv = false;
                    break;
                }

                bytesRead += drtd.get_length();
            }

        } while (0);

        if(!wait_for_xfr_completion())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_cowoverlapdata_ondisk_difffile_v3(BasicIo & diffBio,
                                                         char * cow_data,
                                                         const cdp_drtdv2_fileid_t & drtd)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            SV_UINT mem_required = 0;
            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(drtd.get_drtdno());
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, drtd.get_drtdno());
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_overlap_t>::const_iterator ov_iter = cowlocations.overlaps.begin();

            for( ; ov_iter != cowlocations.overlaps.end(); ++ov_iter)
            {
                cdp_cow_overlap_t overlap = *ov_iter;

                if(!readDRTDFromBio(diffBio,
                                    (char *) (cow_data + overlap.relative_voloffset),
                                    overlap.fileoffset,
                                    overlap.length))
                {
                    rv = false;
                    break;
                }
            }

            if(!rv)
                break;

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


bool CDPV2Writer::read_async_cownonoverlapdata_v3(const std::string &volumename, 
                                                  char * cow_data,
                                                  const cdp_drtdv2_fileid_t & drtd)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(drtd.get_drtdno());
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, drtd.get_drtdno());
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_nonoverlap_t>::const_iterator nov_iter = cowlocations.nonoverlaps.begin();
            for( ; nov_iter != cowlocations.nonoverlaps.end(); ++ nov_iter)
            {
                cdp_cow_nonoverlap_t nonoverlap = *nov_iter;

                if(m_ismultisparsefile)
                {
                    if(!read_async_drtd_from_multisparsefile((char *) (cow_data + nonoverlap.relative_voloffset), 
                                                             drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                             nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }

				}
				else
                {

                    if(!canIssueNextIo(0))
                    {
                        rv = false;
                        break;
                    }

                    if(!initiate_next_async_read(*m_rfptr.get(), 
                                                 (char *) (cow_data + nonoverlap.relative_voloffset), 
                                                 nonoverlap.length,
                                                 drtd.get_volume_offset() + nonoverlap.relative_voloffset))
                    {
                        rv = false;
                        break;
                    }
                }
            }

            if(!rv)
                break;

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

// ======================================================
//
// Synchronous COW update routines:
// 
//  updatecow_synch_inmem_difffile_v3
//    read_sync_cowdrtds_inmem_difffile_v3
//       read_sync_cowdata_inmem_difffile_v3
//    write_sync_cowdata
//       get_cows_belonging_to_same_file
//          get_cdp_data_filepath
//       get_sync_writehandle
//
// ======================================================


bool CDPV2Writer::updatecow_synch_inmem_difffile_v3(const std::string &volumename, 
                                                    char * diff_data,
                                                    const cdp_drtdv2s_t & drtds)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_UINT drtd_no = 0;
    PerfTime volread_start_time;
    PerfTime volread_end_time;
    PerfTime cowwrite_start_time;
    PerfTime cowwrite_end_time;

    try
    {
        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = drtds.begin();

            if(drtds.empty())
            {
                rv = true;
                break;
            }

            while(drtdsIter != drtds.end())          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());


                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t cow_drtds_begin;
                cdp_drtdv2s_constiter_t cow_drtds_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                drtds.end(), 
                                                maxUsableMemory, 
                                                cow_drtds_begin,
                                                cow_drtds_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }

                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                getPerfTime(volread_start_time);

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, cow_drtds_begin,cow_drtds_end);
                }

                if(!read_sync_cowdrtds_inmem_difffile_v3(volumename, 
                                                         diff_data,
                                                         pcowdata.Get(),
                                                         cow_drtds_begin,
                                                         cow_drtds_end,
                                                         drtd_no, pcowdata.Size()))
                {
                    rv = false;
                    break;
                }

                getPerfTime(volread_end_time);
                m_volread_time += getPerfTimeDiff(volread_start_time, volread_end_time);

                getPerfTime(cowwrite_start_time);

                if(!write_sync_cowdata(pcowdata.Get(), cow_drtds_begin, cow_drtds_end))
                {
                    rv = false;
                    break;
                }

                getPerfTime(cowwrite_end_time);
                m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

                drtdsIter = cow_drtds_end;
            }

        } while (0);

        getPerfTime(cowwrite_start_time);

        // flush all open cdp data files and close
        //Performance Improvement - 
        if(!m_useUnBufferedIo)
        {
            if(!flush_cdpdata_syncwrite_handles())
                rv = false;
        }
        if(!m_cache_cdpfile_handles)
        {
            if(!close_cdpdata_syncwrite_handles())
                rv = false;
        }

        getPerfTime(cowwrite_end_time);
        m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

        if(!close_syncread_handles())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::read_sync_cowdrtds_inmem_difffile_v3(const std::string &volumename, 
                                                       const char * diff_data,
                                                       char * cow_data,
                                                       const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                                       const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                                       SV_UINT & drtd_counter, int size)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_ULONG bytesRead = 0;
        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;
        SV_UINT drtd_no = drtd_counter;
        cdp_drtdv2s_fileid_t drtdv2set;

        for( ; drtdsIter != drtds_to_read_end ; ++drtdsIter)
        {
            cdp_drtdv2_fileid_t drtd_fileid((*drtdsIter).get_length(),
                                            (*drtdsIter).get_volume_offset(),
                                            (*drtdsIter).get_file_offset(),
                                            (*drtdsIter).get_fileid(),
                                            drtd_no);

            drtdv2set.insert(drtd_fileid);
            ++drtd_no;
        }

        drtd_counter = drtd_no;

        cdp_drtdv2s_fileid_constiter_t cow_drtds_iter = drtdv2set.begin();
        cdp_drtdv2s_fileid_constiter_t cow_drtds_end = drtdv2set.end();

        for( ; cow_drtds_iter != cow_drtds_end ; ++cow_drtds_iter)
        {
            cdp_drtdv2_fileid_t drtd = *cow_drtds_iter;
			int size_len = size - bytesRead;
            if(!read_sync_cowdata_inmem_difffile_v3(volumename,
                                                    diff_data,
                                                    cow_data + bytesRead,
                                                    drtd, size_len))
            {
                rv = false;
                break;
            }

            bytesRead += drtd.get_length();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_sync_cowdata_inmem_difffile_v3(const std::string &volumename, 
                                                      const char * diff_data,
                                                      char * cow_data,
                                                      const cdp_drtdv2_fileid_t & drtd, int cow_data_size)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            SV_UINT mem_required = 0;
            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(drtd.get_drtdno());
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, drtd.get_drtdno());
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_overlap_t>::const_iterator ov_iter = cowlocations.overlaps.begin();

            for( ; ov_iter != cowlocations.overlaps.end(); ++ov_iter)
            {
                cdp_cow_overlap_t overlap = *ov_iter;
				inm_memcpy_s(cow_data + overlap.relative_voloffset, (cow_data_size - overlap.relative_voloffset), diff_data + overlap.fileoffset, overlap.length);
            }

            std::vector<cdp_cow_nonoverlap_t>::const_iterator nov_iter = cowlocations.nonoverlaps.begin();
            for( ; nov_iter != cowlocations.nonoverlaps.end(); ++ nov_iter)
            {
                cdp_cow_nonoverlap_t nonoverlap = *nov_iter;

                if(m_ismultisparsefile)
                {
                    if(!read_sync_drtd_from_multisparsefile((char *) (cow_data + nonoverlap.relative_voloffset), 
                                                            drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                            nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }

                } else
                {

                    if(!read_sync_drtd_from_volume(m_volhandle,
                                                   (char *) (cow_data + nonoverlap.relative_voloffset),
                                                   drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                   nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }
                }
            }

            if(!rv)
                break;

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_sync_drtd_from_volume(ACE_HANDLE & vol_handle,
                                             char * pbuffer,
                                             const SV_OFFSET_TYPE & offset,
                                             const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {


        SV_UINT remaining_length = length;
        SV_UINT bytes_to_read = 0; 
        SV_UINT bytes_read = 0;

        if(ACE_OS::llseek(m_volhandle,offset, SEEK_SET) < 0)
        {
            stringstream l_stderr;
            l_stderr   << "seek at offset " << offset
                       << " failed for volume " << m_volumename
                       << ". error code: " << ACE_OS::last_error() << std::endl;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            rv = false;
            break;
        }

        while(remaining_length > 0)
        {
            bytes_to_read = min(m_max_rw_size, remaining_length);

            if( bytes_to_read != ACE_OS::read(m_volhandle, 
                                              pbuffer + bytes_read, 
                                              bytes_to_read))
            {
                stringstream l_stderr;
                l_stderr   << "read at offset " << (offset + bytes_read)
                           << " failed for volume " << m_volumename
                           << ". error code: " << ACE_OS::last_error() << std::endl;

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }
            bytes_read += bytes_to_read;
            remaining_length -= bytes_to_read;
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::write_sync_cowdata(const char * cow_data,
                                     const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                                     const cdp_drtdv2s_constiter_t & drtds_to_write_end)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_UINT mem_required = 0;

        std::string cdp_data_filepath;
        SV_OFFSET_TYPE cow_offset = -1;
        SV_UINT cow_length = 0;
        SV_UINT cow_length_consumed =0;
        SV_UINT drtd_no = 0;
        cdp_drtdv2s_fileid_t drtdv2set;

        m_memtobefreed_on_io_completion = false;

        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_write_begin;
        for( ; drtdsIter != drtds_to_write_end ; ++drtdsIter)
        {
            cdp_drtdv2_fileid_t drtd_fileid((*drtdsIter).get_length(),
                                            (*drtdsIter).get_volume_offset(),
                                            (*drtdsIter).get_file_offset(),
                                            (*drtdsIter).get_fileid(),
                                            drtd_no);

            drtdv2set.insert(drtd_fileid);
            ++drtd_no;
        }

        cdp_drtdv2s_fileid_constiter_t cow_drtds_iter = drtdv2set.begin();
        cdp_drtdv2s_fileid_constiter_t cow_drtds_samefile_iter;
        cdp_drtdv2s_fileid_constiter_t cow_drtds_samefile_end;

        while(cow_drtds_iter != drtdv2set.end())
        {

            cdp_data_filepath.clear();
            cow_length =0;
            cow_offset =-1;

            if(!get_cows_belonging_to_same_file(cow_drtds_iter,drtdv2set.end(),
                                                cow_drtds_samefile_iter,
                                                cow_drtds_samefile_end,
                                                cdp_data_filepath, 
                                                cow_offset,
                                                cow_length))
            {
                rv = false;
                break;
            }

            ACE_HANDLE cow_handle = ACE_INVALID_HANDLE;
            if(!get_cdpdata_sync_writehandle(cdp_data_filepath, cow_handle))
            {
                rv = false;
                break;
            }

            if(ACE_OS::llseek(cow_handle,cow_offset,SEEK_SET) < 0)
            {
                stringstream l_stderr;
                l_stderr   << "Seek to offset " << cow_offset
                           << " failed for the dat file " << cdp_data_filepath
                           << ". error code: " << ACE_OS::last_error() << std::endl;
                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                rv = false;
                break;
            }

            unsigned int byteswrote = ACE_OS::write(cow_handle, cow_data + cow_length_consumed, cow_length);
            if(byteswrote != cow_length)
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s write to file %s failed. byteswrote: %d, expected:%d. error=%d\n",
                            FUNCTION_NAME, cdp_data_filepath.c_str(), byteswrote, cow_length, ACE_OS::last_error());

                rv = false;
                break;
            }


            cow_drtds_iter = cow_drtds_samefile_end; 
            cow_length_consumed += cow_length;
        }

        if(!wait_for_xfr_completion())
            rv = false;
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::get_sync_writehandle(const std::string & filename, 
                                       ACE_HANDLE & handle)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_syncwritehandles_t::iterator handle_iter = m_syncwrite_handles.find(filename);
        if(handle_iter == m_syncwrite_handles.end())
        {
            int openMode = O_CREAT | O_WRONLY ;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }

            setumask();

            mode_t share;
            setsharemode_for_all(share);

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode, share);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            m_syncwrite_handles.insert(std::make_pair(filename, handle));
		}
		else
        {
            handle = handle_iter ->second;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}


// =========================================================
//
// Synchronous COW update routines:
//
//  updatecow_synch_ondisk_difffile_v3
//    read_sync_cowdrtds_ondisk_difffile_v3
//      read_sync_cowdata_ondisk_difffile_v3
//    write_sync_cowdata
//       get_cows_belonging_to_same_file
//          get_cdp_data_filepath
//       get_sync_writehandle
//
// =========================================================

bool CDPV2Writer::updatecow_synch_ondisk_difffile_v3(const std::string &volumename, 
                                                     const std::string & diffpath,
                                                     const cdp_drtdv2s_t & drtds)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    BasicIo diffBIo;
    SV_UINT drtd_no = 0;
    PerfTime volread_start_time;
    PerfTime volread_end_time;
    PerfTime cowwrite_start_time;
    PerfTime cowwrite_end_time;

    try
    {
        do
        {
            cdp_drtdv2s_constiter_t drtdsIter = drtds.begin();

            if(drtds.empty())
            {
                rv = true;
                break;
            }

            BasicIo::BioOpenMode diff_mode =
                (BasicIo::BioReadExisting | 
                 BasicIo::BioShareRead | 
                 BasicIo::BioShareWrite | 
                 BasicIo::BioShareDelete);

            diffBIo.Open(diffpath, diff_mode);

            if(!diffBIo.Good())
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            diffpath.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }


            while(drtdsIter != drtds.end())          
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());


                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t cow_drtds_begin;
                cdp_drtdv2s_constiter_t cow_drtds_end;

                SV_ULONGLONG memRequired = 0;
                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                drtds.end(), 
                                                maxUsableMemory, 
                                                cow_drtds_begin,
                                                cow_drtds_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }

                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                getPerfTime(volread_start_time);

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, cow_drtds_begin,cow_drtds_end);
                }

                if(!read_sync_cowdrtds_ondisk_difffile_v3(volumename, 
                                                          diffBIo,
                                                          pcowdata.Get(),
                                                          cow_drtds_begin,
                                                          cow_drtds_end,
                                                          drtd_no))
                {
                    rv = false;
                    break;
                }

                getPerfTime(volread_end_time);
                m_volread_time += getPerfTimeDiff(volread_start_time, volread_end_time);

                getPerfTime(cowwrite_start_time);

                if(!write_sync_cowdata(pcowdata.Get(), cow_drtds_begin, cow_drtds_end))
                {
                    rv = false;
                    break;
                }

                getPerfTime(cowwrite_end_time);
                m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

                drtdsIter = cow_drtds_end;
            }

        } while (0);

        getPerfTime(cowwrite_start_time);

        // flush all open cdp data files and close
        //Performance Improvement - 
        if(!m_useUnBufferedIo)
        {
            if(!flush_cdpdata_syncwrite_handles())
                rv = false;
        }

        if(!m_cache_cdpfile_handles)
        {
            close_cdpdata_syncwrite_handles();
        }

        getPerfTime(cowwrite_end_time);
        m_cowwrite_time += getPerfTimeDiff(cowwrite_start_time, cowwrite_end_time);

        if(!close_syncread_handles())
            rv = false;

        if(diffBIo.isOpen())
        {
            diffBIo.Close();
        }

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::read_sync_cowdrtds_ondisk_difffile_v3(const std::string &volumename,
                                                        BasicIo & diffBio,
                                                        char * cow_data,
                                                        const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                                        const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                                        SV_UINT & drtd_counter)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_ULONG bytesRead = 0;
        cdp_drtdv2s_constiter_t drtdsIter = drtds_to_read_begin;


        for( ; drtdsIter != drtds_to_read_end ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;

            if(!read_sync_cowdata_ondisk_difffile_v3(volumename,
                                                     diffBio,
                                                     cow_data + bytesRead,
                                                     drtd,
                                                     drtd_counter))
            {
                rv = false;
                break;
            }

            bytesRead += drtd.get_length();
            ++drtd_counter;
        }

        if(!wait_for_xfr_completion())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::read_sync_cowdata_ondisk_difffile_v3(const std::string &volumename,
                                                       BasicIo & diffBio,
                                                       char * cow_data,
                                                       const cdp_drtdv2_t & drtd,
                                                       const SV_UINT &drtd_no)
{

    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            SV_UINT mem_required = 0;
            cdp_cow_locations_t::const_iterator cowlocations_iter = m_cowlocations.find(drtd_no);
            if(cowlocations_iter == m_cowlocations.end())
            {
                DebugPrintf(SV_LOG_ERROR, 
                            "FUNCTION:%s internal error, unable to find the locations for reading cow data for drtd:%d.\n",
                            FUNCTION_NAME, drtd_no);
                rv = false;
                break;
            }

            cdp_cow_location_t cowlocations = cowlocations_iter ->second;
            std::vector<cdp_cow_overlap_t>::const_iterator ov_iter = cowlocations.overlaps.begin();

            for( ; ov_iter != cowlocations.overlaps.end(); ++ov_iter)
            {
                cdp_cow_overlap_t overlap = *ov_iter;

                if(!readDRTDFromBio(diffBio,
                                    (char *) (cow_data + overlap.relative_voloffset),
                                    overlap.fileoffset,
                                    overlap.length))
                {
                    rv = false;
                    break;
                }
            }

            std::vector<cdp_cow_nonoverlap_t>::const_iterator nov_iter = cowlocations.nonoverlaps.begin();
            for( ; nov_iter != cowlocations.nonoverlaps.end(); ++ nov_iter)
            {
                cdp_cow_nonoverlap_t nonoverlap = *nov_iter;

                if(m_ismultisparsefile)
                {
                    if(!read_sync_drtd_from_multisparsefile((char *) (cow_data + nonoverlap.relative_voloffset), 
                                                            drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                            nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }

				}
				else
                {

                    if(!read_sync_drtd_from_volume(m_volhandle,
                                                   (char *) (cow_data + nonoverlap.relative_voloffset),
                                                   drtd.get_volume_offset() + nonoverlap.relative_voloffset,
                                                   nonoverlap.length))
                    {
                        rv = false;
                        break;
                    }
                }
            }

            if(!rv)
                break;

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


// =====================================================
// 
//   Vsnap map updates:
//    updatevsnaps  
//
// =====================================================



/*
 * FUNCTION NAME :  CDPV2Writer::updatevsnaps_v3
 *
 * DESCRIPTION : As the diffs are applied, virtual volume driver is informed to
 *               update the vsnap map files
 *
 * INPUT PARAMETERS : volumename - target volume name from which the vsnaps are created
 *                    cdp_datafile - retention data file name
 *                    metadata - the differential data applied
 * 
 * OUTPUT PARAMETERS : none
 *
 * ALGORITHM :
 * 
 *    1.  Open virtual volume driver
 *    2.  for each change in current differential,
 *        issue a map update request to the driver
 *    3.  Close the virtual volume driver
 *
 * return value : true if vsnap updation successfull and false otherwise
 *
 */
bool CDPV2Writer::updatevsnaps_v3(const std::string & volumename, 
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{

    bool rv = true;

    std::string vsnap_parentvol = volumename;
    std::string cdp_filename = "";
    FormatVolumeName(vsnap_parentvol);
    bool cow = false;
    int idx = 0;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        do
        {
            if(!m_vsnapdriver_available)
            {
                rv = true;
                break;
            }

            if(!m_volpackdriver_available && m_isvirtualvolume)
            {
                vsnap_parentvol = m_volguid;
            }
            cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;


            if (nonoverlapping_batch_start == nonoverlapping_batch_end)
            {
                rv = true;
                break;
            }

            if(!m_cdpenabled)
            {
                cow = true;
			}
			else
            {
                cow = false;
            }



            if(strnicmp(vsnap_parentvol.c_str(), "\\\\.\\", 4) == 0)
                idx = 4;
#if SV_WINDOWS
            vsnap_parentvol[idx] = toupper(vsnap_parentvol[idx]);
#endif

            for ( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter )
            {
                cdp_drtdv2_t drtd = *drtdsIter;

                if(m_cdpenabled)
                {
                    if(!get_cdp_data_filepath(drtd.get_fileid(), cdp_filename))
                    {
                        rv = false;
                        break;
                    }
                    cdp_filename = basename_r(cdp_filename.c_str());
                }

                if(IssueMapUpdateIoctlToVVDriver(m_vsnapctrldev, 
                                                 (const char *)(vsnap_parentvol.c_str() + idx),
                                                 cdp_filename.c_str(),
                                                 drtd.get_length(),
                                                 drtd.get_volume_offset(),
                                                 drtd.get_file_offset(),
                                                 cow) < 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "vsnap map update for %s failed.\n",
                                volumename.c_str());
                    rv = false;
                    break;
                }
            }

        } while (0);

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return rv;
}


// =============================================
// bitmap routines
// =============================================

bool CDPV2Writer::set_ts_inbitmap_v3(const cdpv3metadata_t & cow_metadata, const cdpv3transaction_t & txn)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    do
    {

        SV_TIME svtime;
        SV_ULONGLONG ts = 0;
        SV_ULONGLONG ts_in_hrs = 0;
        cdp_timestamp_t new_iotime;



        cdp_drtdv2s_constiter_t cowdrtds_iter = cow_metadata.m_drtds.begin();
        for( ; cowdrtds_iter != cow_metadata.m_drtds.end(); ++cowdrtds_iter)
        {

            ts = cow_metadata.m_tsfc + cowdrtds_iter ->get_timedelta();

            if(!CDPUtil::ToSVTime(ts,svtime))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s offset: " ULLSPEC " invalid timestamp: " ULLSPEC "\n",
                            FUNCTION_NAME, cowdrtds_iter ->get_volume_offset(), ts);
                rv = false;
                break;
            }

            if(!CDPUtil::ToTimestampinHrs(ts, ts_in_hrs))
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s offset: " ULLSPEC " invalid timestamp: " ULLSPEC "\n",
                            FUNCTION_NAME, cowdrtds_iter ->get_volume_offset(), ts);
                rv = false;
                break;
            }

            assert(ts_in_hrs == txn.m_bkmks.m_hr);
            if(ts_in_hrs != txn.m_bkmks.m_hr)
            {
                DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s timestamp mismatch txn time: " ULLSPEC " drtd time: " ULLSPEC "\n",
                            FUNCTION_NAME, txn.m_bkmks.m_hr, ts_in_hrs);
                rv = false;
                break;
            }

            new_iotime.year = svtime.wYear - CDP_BASE_YEAR;
            new_iotime.month = svtime.wMonth;
            new_iotime.days = svtime.wDay;
            new_iotime.hours = svtime.wHour;
            new_iotime.events = txn.m_bkmks.m_bkmkcount;

            SV_UINT bytespending = cowdrtds_iter ->get_length();
            SV_UINT bytesconsumed = 0;
            while(bytespending > 0)
            {

                if(!m_cdpbitmap.set_ts(cowdrtds_iter ->get_volume_offset() + bytesconsumed,new_iotime))
                {
                    rv = false;
                    break;
                }

                bytesconsumed += m_cdpbitmap.get_block_size();
                bytespending -= m_cdpbitmap.get_block_size();
            }

            if(!rv)
                break;
        }

    }while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}


// ===========================================
// db update routines
// ===========================================

bool CDPV2Writer::update_dbrecord_v3(const cdpv3metadata_t & cow_metadata, const cdpv3transaction_t & txn )
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    do
    {
        CDPV3DatabaseImpl db(m_dbname);
        CDPV3RecoveryRange timerange;
        std::vector<CDPV3BookMark> bookmarks;
        bool is_revoke = false;


        timerange.c_tsfc = cow_metadata.m_tsfc;
        timerange.c_tslc = cow_metadata.m_tslc;
        timerange.c_tsfcseq = cow_metadata.m_tsfcseq;
        timerange.c_tslcseq = cow_metadata.m_tslcseq;
        timerange.c_granularity = CDP_ANY_POINT_IN_TIME;
        timerange.c_accuracy = m_recoveryaccuracy;
        timerange.c_cxupdatestatus = CS_ADD_PENDING;

        for(int i = 0 ; i < cow_metadata.m_users.size(); ++i)
        {
            SvMarkerPtr bkmk_ptr = cow_metadata.m_users[i];
            if(bkmk_ptr -> IsRevocationTag())
            {
                is_revoke = true;
            }

            CDPV3BookMark bkmk;
            bkmk.c_apptype = bkmk_ptr ->TagType();
            bkmk.c_value = bkmk_ptr ->Tag().get();
            bkmk.c_accuracy = m_recoveryaccuracy;
            bkmk.c_timestamp = cow_metadata.m_tsfc;
            bkmk.c_sequence = cow_metadata.m_tsfcseq;
            bkmk.c_lifetime = bkmk_ptr -> LifeTime();
            if(bkmk_ptr -> GuidBuffer())
            {
                bkmk.c_identifier = bkmk_ptr -> GuidBuffer().get();
			}
			else
            {
                bkmk.c_identifier = "";
            }

            bkmk.c_verified = NONVERIFIEDEVENT;
            bkmk.c_lockstatus= BOOKMARK_STATE_UNLOCKED;
            bkmk.c_comment = "";
            bkmk.c_cxupdatestatus = CS_ADD_PENDING;
            bkmk.c_counter = txn.m_bkmks.m_bkmkcount;
            bookmarks.push_back(bkmk);
        }

        if(!db.update_db(timerange,bookmarks, is_revoke))
        {
            rv = false;
            break;
        }

    } while(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}



// =============================================================
//  unused routines:
// 
//    isTsoFile
//    ParseTsoFile
//    get_previous_diff_information
//
// =============================================================

/*
 * FUNCTION NAME :  CDPV2Writer::isTsoFile
 *
 * DESCRIPTION : Identifies whether the file is a TSO file or not
 *
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 * 
 * OUTPUT PARAMETERS : none
 *
 * return value : true if it is a TSO file and false otherwise
 *
 */
bool CDPV2Writer::isTsoFile(const std::string & filePath)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string::size_type idx = filePath.find(Tso_File_SubString);
    if (std::string::npos == idx ) {
        rv = false; // not a tso file
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
 * FUNCTION NAME :  CDPV2Writer::ParseTsoFile
 *
 * DESCRIPTION : parses the TSO file and extracts metadata from file name
 *
 * INPUT PARAMETERS : filePath - absolute path to diff file in cache dir
 * 
 * OUTPUT PARAMETERS : metadata - will be filled with the metadata corresponding
 *                                to the TSO file name
 *
 * return value : true if the parse is successfull and false otherwise
 *
 */
bool CDPV2Writer::ParseTsoFile(const std::string & filePath, DiffPtr & metadata)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    try
    {
        do
        {
            metadata -> initialize();
            metadata -> StartOffset(0);
            metadata -> EndOffset(Tso_File_Size);

            std::vector<char> vBasename(SV_MAX_PATH, '\0');
            BaseName(filePath.c_str(), &vBasename[0], vBasename.size());

            // skip past .dat[.gz]
            std::string id(vBasename.data());
            std::string::size_type dotPos = id.rfind(".dat");
            // find the last '_' 
            std::string::size_type continuationTagEndPos = id.rfind("_", dotPos - 1);

            std::string::size_type continuationTagPos = id.rfind("_", continuationTagEndPos - 1);

            if (std::string::npos == continuationTagPos || !('W' == id[continuationTagPos + 1] 
                                                             || 'M' == id[continuationTagPos + 1] || 'F' == id[continuationTagPos + 1]))
            {
                // OK using an old format
                metadata -> SetContinuationId(static_cast<SV_UINT>(-1));

                // get cx time stamp
                std::string::size_type cxTimeStamp= id.rfind("_", dotPos - 1);

                // in this case use the cx timestamp as both start and end time
                SV_ULONGLONG tslc =
                    boost::lexical_cast<unsigned long long>(id.substr(cxTimeStamp + 1,
                                                                      dotPos - (cxTimeStamp + 1)).c_str());

                metadata -> EndTime(tslc);
                metadata -> StartTime(tslc);
            }
            else
            {
                SV_UINT ctid =
                    boost::lexical_cast<SV_UINT>(id.substr(continuationTagPos + 3, 
                                                           continuationTagEndPos - (continuationTagPos + 3)).c_str());

                metadata -> SetContinuationId(ctid);

                if(('E' == id[continuationTagPos + 2]) && 
                   ((metadata -> ContinuationId() == 1) || (metadata -> ContinuationId() == 0)))
                {
                    metadata -> type(NONSPLIT_IO);
                }
                else
                {
                    metadata -> type(SPLIT_IO);
                }

                // get end time sequence number
                std::string::size_type endTimeSequnceNumberPos = id.rfind("_", continuationTagPos - 1);
                SV_ULONGLONG tslcseq =
                    boost::lexical_cast<unsigned long long>(id.substr(endTimeSequnceNumberPos + 1,
                                                                      continuationTagPos - (endTimeSequnceNumberPos + 1)).c_str());

                metadata -> EndTimeSequenceNumber(tslcseq);

                // get end time
                std::string::size_type endTimePos = id.rfind("_", endTimeSequnceNumberPos - 1);
                SV_ULONGLONG tslc =
                    boost::lexical_cast<unsigned long long>(id.substr(endTimePos + 2,
                                                                      endTimeSequnceNumberPos - (endTimePos + 2)).c_str());
                if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
                {
                    tslc += SVEPOCHDIFF;
                }

                metadata -> EndTime(tslc);

                // get start time sequence number
                std::string::size_type startTimeSequenceNumberPos = id.rfind("_", endTimePos - 1);
                SV_ULONGLONG tsfcseq =
                    boost::lexical_cast<unsigned long long>(id.substr(startTimeSequenceNumberPos + 1,
                                                                      endTimePos - (startTimeSequenceNumberPos + 1)).c_str());

                metadata -> StartTimeSequenceNumber(tsfcseq);

                // get start time
                std::string::size_type startTimePos = id.rfind("_", startTimeSequenceNumberPos - 1);
                SV_ULONGLONG tsfc =
                    boost::lexical_cast<unsigned long long>(id.substr(startTimePos + 2,
                                                                      startTimeSequenceNumberPos - (startTimePos + 2)).c_str());
                if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
                {
                    tsfc += SVEPOCHDIFF;
                }

                metadata -> StartTime(tsfc);

            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  CDPV2Writer::get_previous_diff_information
 *
 * DESCRIPTION : gets the previously diff details from the applied info struct
 *
 * INPUT PARAMETERS : none
 *                    
 * OUTPUT PARAMETERS : 
 *             filename - previous diff filename
 *             version  - if the prev diff contains per io changes
 *
 * NOTE :  
 * 
 * return value : true if successfull and false otherwise
 */
bool CDPV2Writer::get_previous_diff_information(std::string& filename,
                                                SV_UINT& version)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    filename = "none";
    version = 0;

    try
    {
        do
        {
            if(isInDiffSync())
            {
                filename = m_lastFileApplied.previousDiff.filename;
                version = m_lastFileApplied.previousDiff.SVDVersion;
            }
        } while (0);
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}




/*
 * FUNCTION NAME :  CDPV2Writer::write_async_drtds_to_volume
 *
 * DESCRIPTION : Initiate asyncronous writes of non-overlapping DRTDs
 *               and wait for the writes to complete
 *
 * INPUT PARAMETERS : vol_handle - handle to the file(target volume) to which
 *                                 we write the data asynchronously
 *                    diff_data -  pointer to the aligned buffer which contains
 *                                 data to be written
 *                    drtds    -   vector of non overlapping DRTDs which can be
 *                                 written asynchronously
 * 
 * OUTPUT PARAMETERS : byteswritten - no. of bytes written to the volume
 *
 * ALGORITHM :
 * 
 *     1.  For each change in the given set of non-overlapping changes, repeat step2
 *     2.  Initiate an asynchronous write to the volume
 *     3.  Wait for the initiated writes to complete
 *
 * return value : false if initiation of asynchronous write fails, otherwise true 
 *
 */
bool CDPV2Writer::write_async_drtds_to_volume(ACE_Asynch_Write_File_Ptr vol_handle, 
                                              const std::string & volumename,
                                              char * diff_data, 
                                              const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                              const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        SV_ULONG bytesWritten = 0;
        cdp_drtdv2s_constiter_t drtdsIter = nonoverlapping_batch_start;

        for( ; drtdsIter != nonoverlapping_batch_end ; ++drtdsIter)
        {
            cdp_drtdv2_t drtd = *drtdsIter;

            if(!initiate_next_async_write(vol_handle, 
                                          volumename,
                                          (char *) (diff_data + bytesWritten), 
                                          drtd.get_length(),
                                          drtd.get_volume_offset()))
            {
                rv = false;
                break;
            }
            bytesWritten += drtd.get_length();
        }

        if(!wait_for_xfr_completion())
            rv = false;

    }
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}
std::string CDPV2Writer::getNextCowFileName( const cdp_timestamp_t & old_iotime, const cdp_timestamp_t & new_iotime,const SV_UINT sequenceNumber)
{
    bool overlap = (old_iotime == new_iotime);
    char filename[64];

	inm_sprintf_s(filename, ARRAYSIZE(filename), "%s%02d%02d%02d%02d%04d_%d_%02d%02d%02d%02d%04d_%d%s",
            CDPV3_DATA_PREFIX.c_str(),new_iotime.year,new_iotime.month,new_iotime.days,new_iotime.hours,new_iotime.events,overlap,
            old_iotime.year,old_iotime.month,old_iotime.days,old_iotime.hours,old_iotime.events,sequenceNumber,CDPV3_DATA_SUFFIX.c_str());

    return string(filename);	
}

// ============================================
// changes for 
//  1) Reading/writing directly to virtual volume
//     through sparse volume instead of going
//     through virtual volume driver
//  2) Instead of single sparse file, we are 
//     using multiple sparse files to avoid
//     windows ntfs size limitation on sparse
//     file size when volume is fragmented
// =============================================

bool CDPV2Writer::get_async_readhandle(const std::string & filename,
                                       ACE_HANDLE & handle, 
                                       ACE_Asynch_Read_File_Ptr & rf_ptr)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());

    do
    {
        cdp_asyncreadhandles_t::iterator handle_iter = m_asyncread_handles.find(filename);
        if(handle_iter == m_asyncread_handles.end())
        {
            int openMode = O_RDONLY | FILE_FLAG_OVERLAPPED;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }


            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            rf_ptr.reset(new ACE_Asynch_Read_File);

            if(rf_ptr ->open (*this, handle) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "asynch open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                ACE_OS::close(handle);
                handle = ACE_INVALID_HANDLE;
                rv = false;
                break;
            }

            cdp_async_readhandle_t value(handle, rf_ptr);
            m_asyncread_handles.insert(std::make_pair(filename, value));
		}
		else
        {
            handle = handle_iter ->second.handle;
            rf_ptr = handle_iter ->second.asynch_handle;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}


bool CDPV2Writer::get_sync_readhandle(const std::string & filename, 
                                      ACE_HANDLE & handle)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_syncreadhandles_t::iterator handle_iter = m_syncread_handles.find(filename);
        if(handle_iter == m_syncread_handles.end())
        {
            int openMode = O_RDONLY ;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            m_syncread_handles.insert(std::make_pair(filename, handle));
		}
		else
        {
            handle = handle_iter ->second;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}

bool CDPV2Writer::close_asyncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_asyncwritehandles_t::const_iterator handle_iter =  m_asyncwrite_handles.begin();
    for( ; handle_iter != m_asyncwrite_handles.end(); ++handle_iter)
    {
        cdp_async_writehandle_t handle_value = handle_iter ->second;

        if(!flush_io(handle_value.handle, handle_iter ->first))
            rv = false;

        ACE_OS::close(handle_value.handle);
    }
    m_asyncwrite_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::close_asyncread_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_asyncreadhandles_t::const_iterator handle_iter =  m_asyncread_handles.begin();
    for( ; handle_iter != m_asyncread_handles.end(); ++handle_iter)
    {
        cdp_async_readhandle_t handle_value = handle_iter ->second;
        ACE_OS::close(handle_value.handle);
    }
    m_asyncread_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::close_syncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    cdp_syncwritehandles_t::const_iterator handle_iter =  m_syncwrite_handles.begin();
    for( ; handle_iter != m_syncwrite_handles.end(); ++handle_iter)
    {
        ACE_HANDLE handle_value = handle_iter ->second;
        if(!m_useUnBufferedIo)
        {
            if(!flush_io(handle_value, handle_iter ->first))
                rv = false;
        }
        ACE_OS::close(handle_value);
    }
    m_syncwrite_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::close_syncread_handles()
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_syncreadhandles_t::const_iterator handle_iter =  m_syncread_handles.begin();
    for( ; handle_iter != m_syncread_handles.end(); ++handle_iter)
    {
        ACE_HANDLE handle_value = handle_iter ->second;
        ACE_OS::close(handle_value);
    }
    m_syncread_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::all_zeroes(const char * const buffer, const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    for(SV_UINT i = 0; i < length; i++)
    {
        if(buffer[i] != 0)
        {
            rv = false;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::get_writehandle(const std::string & filename,ACE_HANDLE & handle)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        int openMode = O_CREAT | O_WRONLY | O_APPEND;
        if(m_useUnBufferedIo)
        {
            setdirectmode(openMode);
        }

        setumask();

        //Setting appropriate share on retention file
        mode_t share;
        setsharemode_for_all(share);

        // PR#10815: Long Path support
        handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                              openMode, share);

        if(ACE_INVALID_HANDLE == handle)
        {
            DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                        filename.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }
    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}

bool CDPV2Writer::punch_hole_sparsefile(const std::string & filename,
                                        const SV_OFFSET_TYPE & offset,
                                        const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());

#ifdef SV_WINDOWS

    ACE_HANDLE file_handle ;
    get_writehandle(filename,file_handle);

    FILE_ZERO_DATA_INFORMATION FileZeroDataInformation;
    FileZeroDataInformation.FileOffset.QuadPart = offset;
    FileZeroDataInformation.BeyondFinalZero.QuadPart = offset + length;

    SV_ULONG bytereturned = 0;
    if(!DeviceIoControl(file_handle,FSCTL_SET_ZERO_DATA,
                        &FileZeroDataInformation,sizeof(FileZeroDataInformation),
                        NULL,0,
                        &bytereturned,
                        NULL))
    {
        //FSCTL_SET_ZERO_DATA is not supported for certain filesystems. Instead of erroring out we want to use
        //the normal write system apis.
        if(GetLastError()==ERROR_NOT_SUPPORTED) 
        {
            m_punch_hole_supported = false;
            DebugPrintf(SV_LOG_ERROR, "FSCTL_SET_ZERO_DATA: failed for %s. ERROR_NOT_SUPPORTED\n", 
                        filename.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "FSCTL_SET_ZERO_DATA: failed for %s. offset:" LLSPEC " length: %u error: %lu\n", 
                        filename.c_str(), offset, length, GetLastError());
        }
        rv = false;
    }
    ACE_OS::close(file_handle);
#else
    m_punch_hole_supported = false;
    rv = false;
    DebugPrintf(SV_LOG_ERROR, "punch hole is not implemented on non-windows platforms.\n");
#endif

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}


bool CDPV2Writer::split_drtd_by_sparsefilename(const SV_OFFSET_TYPE & sparsevol_offset,
                                               const SV_UINT & sparsevol_io_size,
                                               cdp_drtds_sparsefile_t & splitdrtds)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::stringstream sparsepartfile;
    SV_UINT bytes_processed = 0;
    SV_UINT remaining_io = sparsevol_io_size;

    SV_LONGLONG fileid = sparsevol_offset / m_maxsparsefilepartsize;
    SV_OFFSET_TYPE sparsefile_offset = sparsevol_offset %  m_maxsparsefilepartsize;
    SV_UINT sparsefile_io_length = min((SV_OFFSET_TYPE)remaining_io, (m_maxsparsefilepartsize - sparsefile_offset));
    sparsepartfile << m_multisparsefilename << SPARSE_PARTFILE_EXT << fileid;

    while(remaining_io > 0)
    {
        cdp_drtd_sparsefile_t split_drtd(sparsefile_io_length, 
                                         sparsevol_offset + bytes_processed,
                                         sparsefile_offset, sparsepartfile.str());
        splitdrtds.push_back(split_drtd);

        DebugPrintf(SV_LOG_DEBUG, "%s\n", sparsepartfile.str().c_str());

        remaining_io -= sparsefile_io_length;
        bytes_processed += sparsefile_io_length;
        ++fileid;

        sparsepartfile.clear();
        sparsepartfile.str("");
        sparsepartfile << m_multisparsefilename << SPARSE_PARTFILE_EXT << fileid;
        sparsefile_offset = 0;
        sparsefile_io_length = min((SV_OFFSET_TYPE)remaining_io, m_maxsparsefilepartsize);
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::write_async_inmemdrtd_multisparse_volume(const char *const diff_data , 
                                                           const cdp_drtdv2_t & drtd, 
                                                           char * buffer_for_reading_data, int size )
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer;
        SV_UINT bytes_processed = 0;

        if(!split_drtd_by_sparsefilename(drtd.get_volume_offset(), drtd.get_length(), 
                                         split_drtds))
        {
            rv = false;
            break;
        }

        if(buffer_for_reading_data)
        {
            pbuffer = buffer_for_reading_data;
            inm_memcpy_s(pbuffer, size, diff_data + drtd.get_file_offset(), drtd.get_length());
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            ACE_Asynch_Write_File_Ptr wf_ptr;

            if(!get_async_writehandle(split_drtd.get_filename(), file_handle, wf_ptr))
            {
                rv = false;
                break;
            }

#ifdef SV_WINDOWS

            // for single sparse virtual volumes 
            // writes are now happening directly to sparse file instead of going through
            // volpack driver
            // accordingly, even punching of hole in case of all data being zeroes
            // is moved to userspace

            if(m_punch_hole_supported && m_sparse_enabled && all_zeroes(diff_data + drtd.get_file_offset() + bytes_processed, 
                                                                        split_drtd.get_length()))
            {
                if(!punch_hole_sparsefile(split_drtd.get_filename(),
                                          split_drtd.get_file_offset(),
                                          split_drtd.get_length()))
                {
                    if(m_punch_hole_supported)
                    {
                        rv = false;
                        break;
                    }
                }
                else
                {
                    if(m_checkDI)
                    {
                        DiffChecksum checksum;
                        checksum.length = split_drtd.get_length();
                        checksum.offset = split_drtd.get_vol_offset();
                        ComputeHash(diff_data + drtd.get_file_offset() + bytes_processed, 
                                    split_drtd.get_length(),
                                    checksum.digest);
	
                        m_checksums.checksums.push_back(checksum);
                    }
	
                    // continue with next drtd, this drtd is all zeroes
                    // and hole is punched
                    bytes_processed += split_drtd.get_length();
                    continue;
                }
            }

#endif // punch hole support for virtual volume in windows

			int pbuffer_size = 0; //to find pbuffer size
            if(!buffer_for_reading_data)
            {

                if(!canIssueNextIo(split_drtd.get_length()))
                {
                    rv = false;
                    break;
                }

                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer palignedbuffer(split_drtd.get_length(), m_sector_size);

                //
                // insert this SharedAlignedBuffer into a map
                // so that it's use count is increased
                // it will be freed when the write to sparse file 
                // is done by handle_write_file routine
                // 
                SV_UINT bytespending = split_drtd.get_length();
                SV_ULONGLONG start_offset = split_drtd.get_file_offset();
                while(bytespending > 0)
                {
                    SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                    add_memory_allocation(file_handle,start_offset,bytes_to_queue,palignedbuffer);
                    start_offset += bytes_to_queue;
                    bytespending -= bytes_to_queue;
                }

                pbuffer = palignedbuffer.Get();
				pbuffer_size = palignedbuffer.Size(); // To find buffer length

				inm_memcpy_s(pbuffer, pbuffer_size, diff_data + drtd.get_file_offset() + bytes_processed,
                       split_drtd.get_length());
                bytes_processed += split_drtd.get_length();

            } else
            {
                if(!canIssueNextIo(0))
                {
                    rv = false;
                    break;
                }

                pbuffer = buffer_for_reading_data + bytes_processed;
                pbuffer_size = size - bytes_processed;
                bytes_processed += split_drtd.get_length();
            }

#ifdef SV_WINDOWS
            // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
            // in general this should not happen as we can make sure that sync hash
            // compare block size is always greater then FST_SCTR_OFFSET_511
            if ((0 == split_drtd.get_vol_offset()) && (split_drtd.get_length() >= FST_SCTR_OFFSET_511))
            {
                HideBeforeApplyingSyncData(pbuffer,pbuffer_size);
            }
#endif

            if(m_checkDI)
            {
                DiffChecksum checksum;
                checksum.length = split_drtd.get_length();
                checksum.offset = split_drtd.get_vol_offset();
                ComputeHash(pbuffer, split_drtd.get_length(), checksum.digest);
                m_checksums.checksums.push_back(checksum);
            }

            if(!initiate_next_async_write(wf_ptr, split_drtd.get_filename(), 
                                          pbuffer, split_drtd.get_length(),
                                          split_drtd.get_file_offset()))
            {
                rv = false;
                break;
            }
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::write_async_ondiskdrtd_multisparse_volume(BasicIo & diff_bio,
                                                            const cdp_drtdv2_t & drtd, 
                                                            char * buffer_for_reading_data, int size)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer;
        SV_UINT bytes_processed = 0;

        if(!split_drtd_by_sparsefilename(drtd.get_volume_offset(),
                                         drtd.get_length(), split_drtds))
        {
            rv = false;
            break;
        }

        if(buffer_for_reading_data)
        {
            pbuffer = buffer_for_reading_data;
            if(!readDRTDFromBio(diff_bio, pbuffer, 
				drtd.get_file_offset(), drtd.get_length()))
            { 
                rv = false;
                break;
            }
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            ACE_Asynch_Write_File_Ptr wf_ptr;

            if(!get_async_writehandle(split_drtd.get_filename(), file_handle, wf_ptr))
            {
                rv = false;
                break;
            }

			int pbufferSize = 0;
            if(!buffer_for_reading_data)
            {

                if(!canIssueNextIo(split_drtd.get_length()))
                {
                    rv = false;
                    break;
                }

                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer palignedbuffer(split_drtd.get_length(), m_sector_size);

                //
                // insert this SharedAlignedBuffer into a map
                // so that it's use count is increased
                // it will be freed when the write to sparse file 
                // is done by handle_write_file routine
                // 
                SV_UINT bytespending = split_drtd.get_length();
                SV_ULONGLONG start_offset = split_drtd.get_file_offset();
                while(bytespending > 0)
                {
                    SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                    add_memory_allocation(file_handle,start_offset,bytes_to_queue,palignedbuffer);
                    start_offset += bytes_to_queue;
                    bytespending -= bytes_to_queue;
                }

                pbuffer = palignedbuffer.Get();
				pbufferSize = palignedbuffer.Size();

                if(!readDRTDFromBio(diff_bio, pbuffer, 
                                    drtd.get_file_offset() + bytes_processed, split_drtd.get_length()))
                { 
                    rv = false;
                    break;
                }

                bytes_processed += split_drtd.get_length();

            } else
            {
                if(!canIssueNextIo(0))
                {
                    rv = false;
                    break;
                }
                pbuffer = buffer_for_reading_data + bytes_processed;
				pbufferSize = size - bytes_processed;
                bytes_processed += split_drtd.get_length();
            }

#ifdef SV_WINDOWS
            // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
            // in general this should not happen as we can make sure that sync hash
            // compare block size is always greater then FST_SCTR_OFFSET_511
            if ((0 == split_drtd.get_vol_offset()) && (split_drtd.get_length() >= FST_SCTR_OFFSET_511))
            {
				HideBeforeApplyingSyncData(pbuffer, pbufferSize);
            }
#endif

            if(m_checkDI)
            {
                DiffChecksum checksum;
                checksum.length = split_drtd.get_length();
                checksum.offset = split_drtd.get_vol_offset();
                ComputeHash(pbuffer, split_drtd.get_length(), checksum.digest);
                m_checksums.checksums.push_back(checksum);
            }

#ifdef SV_WINDOWS

            // for sparse virtual volumes 
            // writes are now happening directly to sparse file instead of going through
            // volpack driver
            // accordingly, even punching of hole in case of all data being zeroes
            // is moved to userspace

            if(m_punch_hole_supported && m_sparse_enabled && all_zeroes(pbuffer, split_drtd.get_length()))
            {
                if(!punch_hole_sparsefile(split_drtd.get_filename(),
                                          split_drtd.get_file_offset(),
                                          split_drtd.get_length()))
                {
                    if(m_punch_hole_supported)
                    {
                        rv = false;
                        break;
                    }
                }
                else
                {
                    // memory is not allocated for whole chunk, but allocated per drtd
                    if(!buffer_for_reading_data)
                    {
                        SV_UINT bytespending = split_drtd.get_length();
                        SV_ULONGLONG start_offset = split_drtd.get_file_offset();
                        while(bytespending > 0)
                        {
                            SV_UINT bytes_to_queue = min((SV_UINT)m_max_rw_size, (SV_UINT)(bytespending));
                            if(!release_memory_allocation(file_handle,start_offset,bytes_to_queue))
                            {
                                DebugPrintf(SV_LOG_ERROR, 
                                            "FUNCTION:%s  encountered failure when releasing allocated memory.\n",
                                            FUNCTION_NAME);
                                rv  = false;
                                break;
                            }
                            start_offset += bytes_to_queue;
                            bytespending -= bytes_to_queue;
                        }
                        if(!rv)
                        {                                
                            rv  = false;
                            break;
                        }                    
                    }
	
                    // continue with next drtd, this drtd is all zeroes
                    // and hole is punched
                    continue;
                }
            }

#endif // punch hole support for virtual volume in windows


            if(!initiate_next_async_write(wf_ptr, split_drtd.get_filename(), 
                                          pbuffer, split_drtd.get_length(),
                                          split_drtd.get_file_offset()))
            {
                rv = false;
                break;
            }
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPV2Writer::write_sync_drtd_to_multisparse_volume(const cdp_drtdv2_t &drtd,
                                                        char * pbuffer_containing_data,int size)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer = 0;
        SV_UINT bytes_processed = 0;

        if(!split_drtd_by_sparsefilename(drtd.get_volume_offset(),
                                         drtd.get_length(),split_drtds))
        {
            rv = false;
            break;
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;
            pbuffer = pbuffer_containing_data + bytes_processed;
			int pbufferSize = size - bytes_processed;
            bytes_processed += split_drtd.get_length();

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            if(!get_sync_writehandle(split_drtd.get_filename(), file_handle))
            {
                rv = false;
                break;
            }

#ifdef SV_WINDOWS
            // TODO: what if offset 0 but buffer not >= FST_SCTR_OFFSET_511
            // in general this should not happen as we can make sure that sync hash
            // compare block size is always greater then FST_SCTR_OFFSET_511
            if ((0 == split_drtd.get_vol_offset()) && (split_drtd.get_length() >= FST_SCTR_OFFSET_511))
            {
				HideBeforeApplyingSyncData(pbuffer, pbufferSize);
            }
#endif

            if(m_checkDI)
            {
                DiffChecksum checksum;
                checksum.length = split_drtd.get_length();
                checksum.offset = split_drtd.get_vol_offset();
                ComputeHash(pbuffer, split_drtd.get_length(),checksum.digest);
                m_checksums.checksums.push_back(checksum);
            }

#ifdef SV_WINDOWS

            // for sparse virtual volumes 
            // writes are now happening directly to sparse file instead of going through
            // volpack driver
            // accordingly, even punching of hole in case of all data being zeroes
            // is moved to userspace


            if(m_punch_hole_supported && m_sparse_enabled && all_zeroes(pbuffer, split_drtd.get_length()))
            {
                if(!punch_hole_sparsefile(split_drtd.get_filename(),
                                          split_drtd.get_file_offset(),
                                          split_drtd.get_length()))
                {
                    if(m_punch_hole_supported)
                    {
                        rv = false;
                        break;
                    }
                }
                else
                {
                    // continue with next drtd, this drtd is all zeroes
                    // and hole is punched
                    continue;
                }
            }

#endif // punch hole support for virtual volume in windows


            SV_UINT remaining_length = split_drtd.get_length();
            SV_UINT bytes_to_write = 0; 
            SV_UINT bytes_written = 0;
            while(remaining_length > 0)
            {
                bytes_to_write = min(m_max_rw_size, remaining_length);

                if( bytes_to_write != ACE_OS::pwrite(file_handle, 
                                                     pbuffer + bytes_written, 
                                                     bytes_to_write,
                                                     split_drtd.get_file_offset() + bytes_written))
                {
                    stringstream l_stderr;
                    l_stderr   << "write at offset " << (split_drtd.get_file_offset() + bytes_written)
                               << " failed for file " << split_drtd.get_filename()
                               << ". error code: " << ACE_OS::last_error() << std::endl;

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    rv = false;
                    break;
                }
                bytes_written += bytes_to_write;
                remaining_length -= bytes_to_write;
            }

            if(!rv)
                break;
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::read_async_drtd_from_multisparsefile(char * buffer_to_read_data,
                                                       const SV_OFFSET_TYPE & offset,
                                                       const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer = 0;
        SV_UINT bytes_processed = 0;

        if(!split_drtd_by_sparsefilename(offset,length,split_drtds))
        {
            rv = false;
            break;
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;
            pbuffer = buffer_to_read_data + bytes_processed;
            bytes_processed += split_drtd.get_length();

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            ACE_Asynch_Read_File_Ptr rf_ptr;
            if(!get_async_readhandle(split_drtd.get_filename(), file_handle,rf_ptr))
            {
                rv = false;
                break;
            }

            if(!canIssueNextIo(0))
            {
                rv = false;
                break;
            }

            if(!initiate_next_async_read(*rf_ptr.get(), pbuffer, split_drtd.get_length(),
                                         split_drtd.get_file_offset()))
            {
                rv = false;
                break;
            }
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::read_sync_drtd_from_multisparsefile(char * buffer_to_read_data,
                                                      const SV_OFFSET_TYPE & offset,
                                                      const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer = 0;
        SV_UINT bytes_processed = 0;

        if(!split_drtd_by_sparsefilename(offset,length,split_drtds))
        {
            rv = false;
            break;
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;
            pbuffer = buffer_to_read_data + bytes_processed;
            bytes_processed += split_drtd.get_length();

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            if(!get_sync_readhandle(split_drtd.get_filename(), file_handle))
            {
                rv = false;
                break;
            }

            SV_UINT remaining_length = split_drtd.get_length();
            SV_UINT bytes_to_read = 0; 
            SV_UINT bytes_read = 0;
            while(remaining_length > 0)
            {
                bytes_to_read = min(m_max_rw_size, remaining_length);

                if( bytes_to_read != ACE_OS::pread(file_handle, 
                                                   pbuffer + bytes_read, 
                                                   bytes_to_read,
                                                   split_drtd.get_file_offset() + bytes_read))
                {
                    stringstream l_stderr;
                    l_stderr   << "read at offset " << (split_drtd.get_file_offset() + bytes_read)
                               << " failed for file " << split_drtd.get_filename()
                               << ". error code: " << ACE_OS::last_error() << std::endl;

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    rv = false;
                    break;
                }
                bytes_read += bytes_to_read;
                remaining_length -= bytes_to_read;
            }

            if(!rv)
                break;
        }

        if(!rv)
            break;

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


// TODO:
//
// Note: the below routines can be removed by proper code refactoring
//   write_sync_inmemdrtds_to_multisparse_volume
// 

bool CDPV2Writer::write_sync_inmemdrtds_to_multisparse_volume(char * pbuffer_containing_data,
                                                              const cdp_drtdv2s_constiter_t & drtds_to_write_begin, 
                                                              const cdp_drtdv2s_constiter_t & drtds_to_write_end,int size)
{
    bool rv = true;

    cdp_drtdv2s_constiter_t drtdsIter = drtds_to_write_begin;

    SV_ULONGLONG bytes_written = 0;
    for( ; drtdsIter != drtds_to_write_end; ++drtdsIter)
    {
        cdp_drtdv2_t drtd = *drtdsIter;
        if(!write_sync_drtd_to_multisparse_volume(drtd,
			pbuffer_containing_data + bytes_written, (size - bytes_written)))
        {
            rv = false;
            break;
        }
        bytes_written += drtd.get_length();
    }

    return rv;
}

void CDPV2Writer::setbiodirect(BasicIo::BioOpenMode &mode)
{
#ifdef SV_WINDOWS
    mode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
    mode |= BasicIo::BioDirect;
#endif
}

// =================================================================================================
// changes for Bugid 17548
//  Reserve space for each retention volume by writing a file with configured amount of size
//		a) If the reserve file is not present creates it. compression is disabled for the file.
//		b) If the size of the reserve file is not equal to configured value,
//			the file is recreated with the given size
//  CDPMgr deletes these files to create some space when there is insufficient amount of space in 
//	   in the retention volume. This will free up space and will avoid any hangs in dp due to no space on device.
// ========================================================================================================
bool CDPV2Writer::reserve_retention_space()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
    SV_ULONGLONG reservesize = m_reserved_cdpspace;
    std::string dpreservefile = m_cdpdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPUtil::CDPRESERVED_FILE;
    ACE_stat reservefile_stat ={0};
    bool rv = true;
    bool reservefile = false;
    do
    {

        SV_ULONGLONG freespace = 0;
        SV_UINT const MAX_RW = m_max_rw_size;


        if( sv_stat( getLongPathName(dpreservefile.c_str()).c_str(), &reservefile_stat ) < 0 ) 
        {
            reservefile = true;
        }
        else
        {
            //If the file is available, check the size of the file.
            //We should truncate is or increase the size based on the existing configured value.
            if(reservefile_stat.st_size != reservesize)
            {
                reservefile = true;
            }
        }

        if(reservefile)
        {
            SV_ULONGLONG freespace = 0;
            if(GetDiskFreeCapacity(m_cdpdir.c_str(),freespace))
            {
                if(freespace < (m_cdpfreespace_trigger + reservesize))
                {
                    rv = false;
                    break;
                }
            }

            //Create the file if not available and reserve the space.
            int openMode = O_CREAT | O_WRONLY | O_TRUNC;

            ACE_HANDLE handle = ACE_OS::open(getLongPathName(dpreservefile.c_str()).c_str(),openMode);

            if(handle == ACE_INVALID_HANDLE)
            {
                DebugPrintf(SV_LOG_ERROR,"Unable to create reserve space for volume %s, errno:%d\n",m_cdpdir.c_str(),ACE_OS::last_error());
                rv = false;
                break;
            }

#ifdef SV_WINDOWS

            //disable compressionon for the file.
            if(!DisableCompressonOnFile(dpreservefile))
            {
                DebugPrintf(SV_LOG_ERROR,"Disabling compression on file %s failed\n",dpreservefile.c_str());
                // see PR #22983
                //rv = false;
                //break;
            }
#endif
            SharedAlignedBuffer pbuffer(MAX_RW, m_sector_size);
            if( !pbuffer )
            {
                DebugPrintf(SV_LOG_ERROR,"Memory allocation failed while reserving space for %s\n",m_cdpdir.c_str());
                rv = false;
                break;
            }

            ACE_OS::memset(pbuffer.Get(),0,MAX_RW);

            while(reservesize)
            {
                int bytestowrite = reservesize > (MAX_RW)?(MAX_RW):reservesize;
                if(ACE_OS::write(handle,pbuffer.Get(),bytestowrite) != bytestowrite)
                {
                    rv = false;
                    DebugPrintf(SV_LOG_ERROR,"Failed to reserve retention space for volume %s errno:%d\n",m_cdpdir.c_str(),ACE_OS::last_error());
                    ACE_OS::close(handle);
                    ACE_OS::unlink(getLongPathName(dpreservefile.c_str()).c_str());
                    break;
                }	
                reservesize -= bytestowrite;
            }

            if (rv)
            {
                if(ACE_OS::fsync(handle) < 0)
                {
                    rv = false;
                    DebugPrintf(SV_LOG_ERROR, "%s flush call failed while trying to reserve retention space in %s.\n", 
                                FUNCTION_NAME, m_cdpdir.c_str());
                }

                ACE_OS::close(handle);
            }
        }

    }while(0);
    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
    return rv;
}


//Performance Improvement - Avoid open/close of retention files.
bool CDPV2Writer::get_cdpdata_async_writehandle(const std::string & filename, 
                                                ACE_HANDLE & handle,
                                                ACE_Asynch_Write_File_Ptr & wf_ptr)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_asyncwritehandles_t::iterator handle_iter = m_cdpdata_asyncwrite_handles.find(filename);

        if(handle_iter == m_cdpdata_asyncwrite_handles.end())
        {
            int openMode = O_CREAT | O_WRONLY | FILE_FLAG_OVERLAPPED;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }

            setumask();

            //Setting appropriate share on retention file
            mode_t share;
            setsharemode_for_all(share);

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode, share);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            wf_ptr.reset(new ACE_Asynch_Write_File);

            if(wf_ptr ->open (*this, handle) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "asynch open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                ACE_OS::close(handle);
                handle = ACE_INVALID_HANDLE;
                rv = false;
                break;
            }

            cdp_async_writehandle_t value(handle, wf_ptr);
            m_cdpdata_asyncwrite_handles.insert(std::make_pair(filename, value));
		}
		else
        {
            handle = handle_iter ->second.handle;
            wf_ptr = handle_iter ->second.asynch_handle;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}

bool CDPV2Writer::get_cdpdata_sync_writehandle(const std::string & filename, 
                                               ACE_HANDLE & handle)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_syncwritehandles_t::iterator handle_iter = m_cdpdata_syncwrite_handles.find(filename);
        if(handle_iter == m_cdpdata_syncwrite_handles.end())
        {
            int openMode = O_CREAT | O_WRONLY ;
            if(m_useUnBufferedIo)
            {
                setdirectmode(openMode);
            }

            setumask();

            mode_t share;
            setsharemode_for_all(share);

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                                  openMode, share);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                            filename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            m_cdpdata_syncwrite_handles.insert(std::make_pair(filename, handle));
		}
		else
        {
            handle = handle_iter ->second;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}

bool CDPV2Writer::close_cdpdata_syncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    cdp_syncwritehandles_t::const_iterator handle_iter =  m_cdpdata_syncwrite_handles.begin();
    for( ; handle_iter != m_cdpdata_syncwrite_handles.end(); ++handle_iter)
    {
        ACE_HANDLE handle_value = handle_iter ->second;
        if(ACE_OS::close(handle_value) < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "close failed for %s\n", handle_iter ->first.c_str());
        }
    }
    m_cdpdata_syncwrite_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::close_cdpdata_asyncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_asyncwritehandles_t::const_iterator handle_iter =  m_cdpdata_asyncwrite_handles.begin();
    for( ; handle_iter != m_cdpdata_asyncwrite_handles.end(); ++handle_iter)
    {
        cdp_async_writehandle_t handle_value = handle_iter ->second;
        if(ACE_OS::close(handle_value.handle) < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "close failed for %s\n", handle_iter ->first.c_str());
        }
    }
    m_cdpdata_asyncwrite_handles.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::flush_cdpdata_syncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    cdp_syncwritehandles_t::const_iterator handle_iter =  m_cdpdata_syncwrite_handles.begin();
    for( ; handle_iter != m_cdpdata_syncwrite_handles.end(); ++handle_iter)
    {
        ACE_HANDLE handle_value = handle_iter ->second;
        if(!flush_io(handle_value, handle_iter ->first))
            rv = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::flush_cdpdata_asyncwrite_handles()
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_asyncwritehandles_t::const_iterator handle_iter =  m_cdpdata_asyncwrite_handles.begin();
    for( ; handle_iter != m_cdpdata_asyncwrite_handles.end(); ++handle_iter)
    {
        cdp_async_writehandle_t handle_value = handle_iter ->second;
        if(!flush_io(handle_value.handle, handle_iter ->first))
            rv = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::IsInsufficientRetentionSpaceOccured()
{
    bool rv = true;
    SV_ULONGLONG diskFreeSpace = 0;
    do
    {
        if(!GetDiskFreeCapacity(m_cdpdir.c_str(),diskFreeSpace))
        {
            DebugPrintf(SV_LOG_ERROR, "Applychanges : Unable to get disk free space for %s\n", m_cdpdir.c_str()); 
            rv = false;
            break;
        }

        if(diskFreeSpace > m_cdpfreespace_trigger)
        {
            rv = false;
            break;
        }

    }while(false);

    return rv;
}

bool CDPV2Writer::EndQuasiState()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int status = 0;
    bool rv = sendEndQuasiStateRequest((*m_Configurator), m_volumename, status);
    if(!rv || (0 != status))
    {

        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME 
                 << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                 << " Error Message: " 
                 << " failed to send end Quasi State Request, will try again on next invocation \n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        return false;
    }

    m_isInDiffSync = true;
    m_inquasi_state = false;

    return true;
}

/*SRM :Start*/
/*
 * rollback_volume_to_requested_point(Timestamp to which the volume should be rolled back)
 * 1) Verify if the requested point is crash consistent, return failure to CX
 * 2) Create redo drtds with the current data in the target volume for all the offsets/lengths modified
 from the time specified and create redo logs in the retention directory.
 * 3) Rollback the volume to the requested time and send a success to CX.
 */

bool CDPV2Writer::rollback_volume_to_requested_point(CDPDRTDsMatchingCndn &cndn, std::string &error_msg, int &error_num)
{
    boost::tribool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);

    std::string txn_filename = m_cdpdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + FLUSH_AND_HOLD_TXNFILENAME;
    m_flush_and_hold_txn.reset(new FlushAndHoldTxnFile(txn_filename));

    do
    {
        //Parsing failed, the volume might be in inconsistent state. We should set resync required and send
        //a failure to CX.
        if(!m_flush_and_hold_txn -> parse())
        {
            rv = false;
            std::stringstream message;
            message << "Flush and Hold transaction file parsing failed for "
                    << m_volumename
                    << "\n";
            DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
            error_msg = message.str();

            break;
        }

        //If the requested time is greater than the last time in the database, then there 
        //are no changes or differentials applied to the volume. Just return Success to CX.

        CDPDatabase db(m_dbname);
        CDPSummary summary;
        if(!db.getcdpsummary(summary))
        {
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to get cdp summary for volume %s\n",FUNCTION_NAME,m_volumename.c_str());
            rv = false;
            break;
        }

        if(cndn.toTime() > summary.end_ts)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: No data to rollback.\n",m_volumename.c_str());
            rv = true;
            break;
        } 

        if(!m_flush_and_hold_txn -> is_redolog_creation_complete()) //Either not started or is in progress.
        {
            SV_UINT filecount = 0;
            rv = create_redo_logs(db,cndn,filecount,error_msg,error_num);
            if(ServiceQuitRequested(rv))
            {
                rv = false;
                break;
            }
            else if(rv)
            {
                m_flush_and_hold_txn -> set_redolog_status(FlushAndHoldTxnFile::COMPLETE,filecount);
            }
            else
            {
                clean_redo_logs(filecount);
                filecount = 0;
                m_flush_and_hold_txn -> set_redolog_status(FlushAndHoldTxnFile::FAILED,filecount);
                std::stringstream message;
                message << "create redo logs failed : "
                        << error_msg
                        << "\n";
                DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
                error_msg = message.str();
                break;
            }
        }

        if(!m_flush_and_hold_txn -> is_undo_volume_changes_complete()) //Either not started or is in progress.
        {
            //Unmount all the VSNAPs in time range after the requested time point as these refer to the volume.
            CDPUtil::UnmountVsnapsWithLaterRecoveryTime(cndn.toTime(),m_volumename);

            rv = rollback(db,cndn,error_msg,error_num);
            if(ServiceQuitRequested(rv))
            {
                rv = false;
                break;
            }
            else if(rv)
            {
                m_flush_and_hold_txn -> set_undo_volume_status(FlushAndHoldTxnFile::COMPLETE);
            }
            else
            {
                m_flush_and_hold_txn -> set_undo_volume_status(FlushAndHoldTxnFile::FAILED);
                std::stringstream message;
                message << "rollback failed : "
                        << error_msg
                        << "\n";
                DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
                error_msg = message.str();
                break;
            }
        }
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
	return bool(rv);
}

boost::tribool CDPV2Writer::create_redo_logs(CDPDatabase &db, CDPDRTDsMatchingCndn & cndn, SV_UINT &filecount,std::string &errmsg,int &error_num)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);

    boost::tribool rv = true;
    cdp_rollback_drtd_t drtd;

    CDPDatabaseImpl::Ptr dbptr = db.FetchDRTDs(cndn);

    if(!dbptr)
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s initialize db pointer failed.\n",
                    FUNCTION_NAME);
        return false;
    }

    cdp_drtdv2s_t rollback_drtds;
    bool end_of_drtd = false;
    SV_ULONGLONG size_of_file = CdpRedoLogFile::GetSizeOfHeader(); 
    SVERROR sv = SVS_OK;
    while ( !end_of_drtd && rv) 
    {
        if(CDPUtil::QuitRequested())
        {
            DebugPrintf(SV_LOG_DEBUG,"%s: Service Quit Requested\n",FUNCTION_NAME);
            rv = boost::indeterminate;
            break;
        }

        if((sv = dbptr -> read(drtd)) == SVS_OK)
        {
            cdp_drtdv2_t cdp_drtd(drtd.length,drtd.voloffset,0,0,0,0);

            rollback_drtds.push_back(cdp_drtd);

            size_of_file += SVD_DIRTY_BLOCK_SIZE;
            size_of_file += drtd.length;
        }
        else if(sv == SVS_FALSE)
        {
            end_of_drtd = true;
        }
        else
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR,"FUNCTION:%s , %d read from db failed.\n",
                        FUNCTION_NAME,__LINE__);
            errmsg += "Error: read failure from retention store\n";
            error_num = CDPReadFromRetentionFailed;
            break;
        }

        if((size_of_file >= m_cdp_max_redolog_size || end_of_drtd)&& 
           rollback_drtds.size() > 0)
        {
            filecount++;
            std::stringstream filename_stream;
            filename_stream << m_cdpdir
                            << ACE_DIRECTORY_SEPARATOR_CHAR_A
                            << CDP_REDOLOG_FILENAME_PREFIX
                            << filecount
                            << CDP_REDOLOG_FILENAME_SUFFIX;

            std::string redo_log = string(filename_stream.str());

            CdpRedoLogFile redologfile(redo_log,m_sector_size,m_max_rw_size);

            if(!redologfile.WriteMetaData(rollback_drtds))
            {
                std::stringstream lerror;
                lerror << "Failed to create redolog file "<<redo_log <<"\n"
                       << "Error "<<Error::Msg(ACE_OS::last_error()) <<"\n";
                errmsg = string(lerror.str());

                DebugPrintf(SV_LOG_ERROR,"%s\n",lerror.str().c_str());
                rv = false;
                break;
            }

            cdp_drtdv2s_constiter_t drtdsIter = rollback_drtds.begin();

            while(drtdsIter != rollback_drtds.end())
            {
                SV_ULONGLONG maxUsableMemory = max(m_maxMemoryForDrtds,(*drtdsIter).get_length());
                SV_ULONGLONG memRequired = 0;

                // get drtds to read based on memory available
                cdp_drtdv2s_constiter_t drtds_to_read_begin;
                cdp_drtdv2s_constiter_t drtds_to_read_end;

                if(!get_drtds_limited_by_memory(drtdsIter, 
                                                rollback_drtds.end(), 
                                                maxUsableMemory, 
                                                drtds_to_read_begin,
                                                drtds_to_read_end,
                                                memRequired) )
                {
                    rv = false;
                    break;
                }


                // allocate the sector aligned buffer of required length
                SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
                m_memconsumed_for_io = memRequired;
                m_memtobefreed_on_io_completion = false;

                if(!m_ismultisparsefile)
                {
                    (void)issue_pagecache_advise(m_volhandle, drtds_to_read_begin,drtds_to_read_end);
                }

                if(m_useAsyncIOs)
                {
                    if(!read_async_drtds_from_volume(m_volumename,pcowdata.Get(), drtds_to_read_begin,drtds_to_read_end))
                    {
                        std::stringstream lerror;
                        lerror << "Failed to read from volume "<<m_volumename <<"\n"
                               << "Error "<<Error::Msg(ACE_OS::last_error()) <<"\n";
                        errmsg = string(lerror.str());

                        DebugPrintf(SV_LOG_ERROR,"%s\n",lerror.str().c_str());

                        rv = false;
                        break;
                    }
                }
                else
                {
                    if(!read_sync_drtds_from_volume(m_volumename,pcowdata.Get(), drtds_to_read_begin,drtds_to_read_end ))
                    {
                        std::stringstream lerror;
                        lerror << "Failed to read from volume "<<m_volumename <<"\n"
                               << "Error "<<Error::Msg(ACE_OS::last_error()) <<"\n";
                        errmsg = string(lerror.str());

                        DebugPrintf(SV_LOG_ERROR,"%s\n",lerror.str().c_str());
                        rv = false;
                        break;
                    }
                }

                if(rv)
                {
                    if(!redologfile.WriteData(pcowdata.Get(),memRequired))	
                    {
                        std::stringstream lerror;
                        lerror << "Failed to create redolog file "<<redo_log <<"\n"
                               << "Error "<<Error::Msg(ACE_OS::last_error()) <<"\n";
                        errmsg = string(lerror.str());

                        DebugPrintf(SV_LOG_ERROR,"%s\n",lerror.str().c_str());
                        rv = false;
                        break;
                    }
                }
                else
                {
                    rv = false;
                    break;
                }

                drtdsIter = drtds_to_read_end;
            }
            rollback_drtds.clear();
            size_of_file = CdpRedoLogFile::GetSizeOfHeader(); //Re-initialize the file size.
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);

    return rv;

}

boost::tribool CDPV2Writer::rollback(CDPDatabase &db, CDPDRTDsMatchingCndn & cndn,std::string &errmsg,int &error_num)
{
    SVERROR sv = SVS_OK;
    std::string filename;
    std::string pathname;
    BasicIo retentionFile;
    cdp_rollback_drtd_t drtd;
    boost::tribool rv = true;
    bool failure = false;
    SV_UINT totalBytesRead = 0;
    SV_UINT cbRemaining = 0;
    SV_UINT cbWritten = 0;
    SV_UINT cbRead = 0;
    SV_UINT cbWrite = 0;
    SV_UINT readin = 0;
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);

    cdp_volume_t tgtvol(m_volumename,m_ismultisparsefile);

    SharedAlignedBuffer pbuffer(m_max_rw_size, m_sector_size);

    CDPDatabaseImpl::Ptr dbptr = db.FetchDRTDs(cndn);

    if(!dbptr)
    {
        stringstream msg;
        msg << "failed to initialize db for target volume :" 
            << tgtvol.GetName() << '\n';
        errmsg = msg.str();
        error_num = CDPReadFromRetentionFailed;
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s initialize db pointer failed.\n",
                    FUNCTION_NAME);
        return false;
    }

    if(!tgtvol.OpenExclusive() || !tgtvol.Good())
    {
        stringstream msg;
        msg << "Failed to open target volume :" 
            << tgtvol.GetName() << ", error : "<<tgtvol.LastError()<<'\n';
        errmsg = msg.str();
        error_num = CDPTgtOpenFailed;
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s \n",
                    FUNCTION_NAME,errmsg.c_str());
        return false;
    }

    do
    {

        while ((sv = dbptr -> read(drtd)) == SVS_OK ) 
        {
            if(CDPUtil::QuitRequested())
            {
                DebugPrintf(SV_LOG_DEBUG,"%s: Service Quit Requested\n",FUNCTION_NAME);
                rv = boost::indeterminate;
                break;
            }

            if(drtd.filename.empty())
            {
                rv = false;
                errmsg += "Error: read failure from retention store\n";
                error_num = CDPReadFromRetentionFailed;
                DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s, Volume %s recieved empty filename in the drtd.\n",
                            FUNCTION_NAME,m_volumename.c_str());
                break;
            }

            std::string tmppathname = db.dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            tmppathname += drtd.filename;

            if(filename != drtd.filename)
            {
                retentionFile.Close();

                filename = drtd.filename;
                pathname = db.dbdir() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
                pathname += filename;

                retentionFile.Open(pathname, BasicIo::BioOpen |BasicIo::BioRead |  BasicIo::BioShareRW | BasicIo::BioBinary);
                if(!retentionFile.Good())
                {
                    errmsg += "Error: read failure from retention store\n";
                    error_num = CDPReadFromRetentionFailed;

                    rv = false;
                    break;
                }
            }


            cbRemaining = drtd.length;
            totalBytesRead = 0;
            cbWritten = 0;
            cbRead = 0;
            cbWrite = 0;

            while( cbRemaining > 0)
            {
                cbRead = min(cbRemaining, m_max_rw_size);

                retentionFile.Seek( (drtd.fileoffset+ totalBytesRead), BasicIo::BioBeg);
                readin = retentionFile.FullRead(pbuffer.Get(),cbRead);

                if( readin != cbRead )
                {
                    errmsg = "Error: read failure from retention store\n";
                    error_num = CDPReadFromRetentionFailed;

                    stringstream lerror;
                    lerror   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                             << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                             << "Error during read operation on differential file:" << retentionFile.GetName() << "\n"
                             << " Offset : " << ( drtd.fileoffset+ totalBytesRead ) << "\n"
                             << "Expected Read:" << cbRead << "\n"
                             << "Actual Read:"   << readin << "\n"
                             << " Error Code: " << retentionFile.LastError() << "\n"
                             << " Error Message: " << Error::Msg(retentionFile.LastError()) << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", lerror.str().c_str());

                    failure = true;
                    rv = false;
                    break;
                }

                tgtvol.Seek(drtd.voloffset + cbWritten, BasicIo::BioBeg);
                if(!tgtvol.Good())
                {
                    errmsg = "Error: seek failed on target volume\n";
                    error_num = CDPSeekOnTgtFailed;

                    DebugPrintf(SV_LOG_ERROR,"Failed to seek on volume %s, offset " ULLSPEC" Error %s\n",
                                m_volumename.c_str(),
                                (drtd.voloffset + cbWritten),
                                Error::Msg(tgtvol.LastError()).c_str());
                    failure = true;
                    rv = false;
                    break;
                }

                if( ((cbWrite = tgtvol.Write(pbuffer.Get(), cbRead)) != cbRead )
                    || (!tgtvol.Good()))
                {
                    errmsg = "Error: Failed to write to target volume\n";
                    error_num = CDPWriteToTgtFailed;
                    stringstream msg;
                    msg << "Error during write operation\n"
                        << " Offset : " << tgtvol.Tell()<< '\n'
                        << "Expected Write: " << cbRead << '\n'
                        << "Actual Write Bytes: " << cbWrite << '\n'
                        << "Error Code: " << tgtvol.LastError() << '\n'
                        << "Error Message: " << Error::Msg(tgtvol.LastError()) << '\n';					
                    DebugPrintf(SV_LOG_ERROR,"%s:%s\n",FUNCTION_NAME,msg.str().c_str());
                    failure = true;
                    rv = false;
                    break;
                }

                totalBytesRead += cbRead;
                cbWritten += cbRead;
                cbRemaining -= cbRead;

            } // end of while loop transferring one dirty block

            // if we could not complete the transfer,
            // there was an error and we will break further transfer
            if(failure)
            {
                break;
            }

        }
        if(sv.failed())
        {
            DebugPrintf(SV_LOG_ERROR,"FUNCTION:%s, Line :%d, read from db failed.\n",
                        FUNCTION_NAME,__LINE__);
            errmsg += "Error: read failure from retention store\n";
            error_num = CDPReadFromRetentionFailed;
            rv = false;
        }

#ifdef SV_SUN

        // On sun, we are opening in buffered mode. so we need to flush
        if(ACE_INVALID_HANDLE != tgtvol.GetHandle())
        {
            if(ACE_OS::fsync(tgtvol.GetHandle()) == -1)
            {
                stringstream msg;
                msg << "failed in flush writes to target volume :" 
                    << tgtvol.GetName()<< " error: " << ACE_OS::last_error() << '\n';
                errmsg += msg.str();
                error_num = CDPWriteToTgtFailed;
                tgtvol.Close();
                break;
            }
        }
#endif

        tgtvol.Close();
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::replay_redo_logs(std::string &errmsg,int &error_num)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
    cdp_volume_t tgtvol(m_volumename,m_ismultisparsefile);
    bool rv = true;

    std::string txn_filename = m_cdpdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + FLUSH_AND_HOLD_TXNFILENAME;
    m_flush_and_hold_txn.reset(new FlushAndHoldTxnFile(txn_filename));
    SV_UINT filecount = 0;

    do
    {
        if(!m_flush_and_hold_txn -> parse())
        {
            std::stringstream lerror;
            lerror << "Parsing flush and hold transaction file for volume "<< m_volumename <<" failed";
            errmsg = string(lerror.str());
            DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: %s\n",FUNCTION_NAME,errmsg.c_str());
            rv = false;
            break;
        }

        filecount = m_flush_and_hold_txn->get_cdp_redolog_count();

        if(m_flush_and_hold_txn->is_replaylog_to_volume_complete() )
        {
            rv = true;
            DebugPrintf(SV_LOG_DEBUG,"Replaying log to volume is complete for %s. Delete the redologs\n",m_volumename.c_str());
            break;
        }

        if(tgtvol.OpenExclusive() && tgtvol.Good())
        {
            for(SV_UINT i = 1;i <= filecount; i++)
            {
                if(CDPUtil::QuitRequested())
                {
                    rv = false;
                    DebugPrintf(SV_LOG_DEBUG,"FUNCTION %s: Quit Requested\n",FUNCTION_NAME);
                    break;
                }

                std::stringstream filename_stream;
                filename_stream << m_cdpdir
                                << ACE_DIRECTORY_SEPARATOR_CHAR_A
                                << CDP_REDOLOG_FILENAME_PREFIX
                                << i
                                << CDP_REDOLOG_FILENAME_SUFFIX;
                std::string redo_log_file_name = string(filename_stream.str());

                ACE_stat stat = {0};

                if(sv_stat(getLongPathName(redo_log_file_name.c_str()).c_str(),&stat)<0)
                {
                    rv = false;
                    std::stringstream lerror;
                    lerror << "Missing redofile "<< redo_log_file_name <<"\n";
                    errmsg = string(lerror.str());
                    DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: %s\n",FUNCTION_NAME,errmsg.c_str());
                    break;
                }
                CdpRedoLogFile redologfile(redo_log_file_name,m_sector_size,m_max_rw_size);

                if(redologfile.Parse())
                {
                    rv = write_redodrtds_to_volume(redologfile,tgtvol);
                    if(!rv)
                    {
                        DebugPrintf(SV_LOG_ERROR,"%s:Failed to write redodrtds to volume. Retry after 60 secs\n",FUNCTION_NAME);
                        break;
                    }
                }
                else
                {
                    //Parsing failed. Don't retry for parsing failures.
                    rv = false;
                    std::stringstream lerror;
                    lerror << "Parsing cdp redolog "<< redo_log_file_name <<" failed";
                    errmsg = string(lerror.str());
                    DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: %s\n",FUNCTION_NAME,errmsg.c_str());
                    break;
                }
            }

#ifdef SV_SUN

            // On sun, we are opening in buffered mode. so we need to flush
            if(ACE_INVALID_HANDLE != tgtvol.GetHandle())
            {
                if(ACE_OS::fsync(tgtvol.GetHandle()) == -1)
                {
                    stringstream msg;
                    msg << "failed in flush writes to target volume :" 
                        << tgtvol.GetName()<< " error: " << ACE_OS::last_error() << '\n';
                    errmsg += msg.str();
                    error_num = CDPWriteToTgtFailed;
                    tgtvol.Close();
                    break;
                }
            }
#endif

            tgtvol.Close();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s Failed to get exclusive access to volume %s.\n",
                        FUNCTION_NAME,m_volumename.c_str());
            rv = false;
        }
    }while(0);

    if(rv)
    {
        m_flush_and_hold_txn->set_replaylog_status(FlushAndHoldTxnFile::COMPLETE);
        rv = clean_redo_logs(filecount);
        if(rv) //Deletion of redo logs failed. Retry
        {
            //Remove the transaction file as the trasaction is complete.
            std::string transaction_file = m_cdpdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + FLUSH_AND_HOLD_TXNFILENAME;
            unlink(transaction_file.c_str()); 
        }

    }
    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV2Writer::write_redodrtds_to_volume(CdpRedoLogFile & redologfile, cdp_volume_t &tgtvol)
{
    bool rv = true;
    cdp_drtdv2s_t redo_drtds = redologfile.GetDrtds();

    cdp_drtdv2s_constiter_t drtdsIter = redo_drtds.begin();
    while(drtdsIter != redo_drtds.end())
    {

        SV_ULONGLONG maxUsableMemory = m_configMemoryForDrtds_Diffsync;
        SV_ULONGLONG memRequired = 0;

        // get drtds to read based on memory available
        cdp_drtdv2s_constiter_t drtds_to_read_begin;
        cdp_drtdv2s_constiter_t drtds_to_read_end;

        if(!get_drtds_limited_by_memory(drtdsIter, 
                                        redo_drtds.end(), 
                                        maxUsableMemory, 
                                        drtds_to_read_begin,
                                        drtds_to_read_end,
                                        memRequired) )
        {
            rv = false;
            break;
        }


        // allocate the sector aligned buffer of required length
        SharedAlignedBuffer pcowdata(memRequired, m_sector_size);
        SV_ULONGLONG length_of_data = 0;

        if(!redologfile.GetData(pcowdata.Get(),memRequired))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get data from redolog file %s\n",redologfile.GetName().c_str());
            rv = false;
            break;
        }

        //TODO: restrict each write to max_rw_size
        while(drtds_to_read_begin != drtds_to_read_end)
        {
            if(CDPUtil::QuitRequested())
            {
                rv = false;
                DebugPrintf(SV_LOG_DEBUG,"%s: Quit requested.\n",FUNCTION_NAME);
                break;
            }

            if(tgtvol.Seek((*drtds_to_read_begin).get_volume_offset(),BasicIo::BioBeg)<0)
            {
                rv = false;
                DebugPrintf(SV_LOG_ERROR,"%s: Failed to seek on target volume %s, offset " ULLSPEC" Error :%d\n",
                            m_volumename.c_str(),(*drtds_to_read_begin).get_volume_offset(),ACE_OS::last_error());
                break;
            }
            if(tgtvol.Write(pcowdata.Get()+length_of_data,(*drtds_to_read_begin).get_length())!=(*drtds_to_read_begin).get_length())
            {
                rv = false;
                DebugPrintf(SV_LOG_ERROR,"%s: Failed to write to target volume %s, offset " ULLSPEC" Length of data %u Error :%d\n",
                            m_volumename.c_str(),(*drtds_to_read_begin).get_volume_offset(),(*drtds_to_read_begin).get_length(),ACE_OS::last_error());
                break;
            }

            length_of_data += (*drtds_to_read_begin).get_length();
            drtds_to_read_begin++;
        }


        drtdsIter = drtds_to_read_end;
    }
    return rv;

}

bool CDPV2Writer::isCrossedRequestedPoint(SV_ULONGLONG diff_startts,bool & app_consitent_point,CDPEvent &cdpevent)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERING %s\n",FUNCTION_NAME);
    do
    {
        if(m_flush_and_hold_request.m_consistency_type == FLUSH_AND_HOLD_REQUEST::APPLICATION)
        {
            app_consitent_point = true;

            std::vector<CDPEvent> events;
            CDPMarkersMatchingCndn cndn;
            cndn.afterTime(m_timeto_pause_apply_ts);
            cndn.mode(ACCURACYEXACT);
            if(!m_flush_and_hold_request.m_bookmark.empty())
            {
                DebugPrintf(SV_LOG_DEBUG,"%s: Bookmark name is %s\n",FUNCTION_NAME,m_flush_and_hold_request.m_bookmark.c_str());
                cndn.type(m_flush_and_hold_request.m_apptype);
                cndn.value(m_flush_and_hold_request.m_bookmark);
            }
            if(CDPUtil::getevents(m_dbname,cndn,events))
            {
                if(events.size()!=0)
                {
                    rv = true;
                    std::vector<CDPEvent>::iterator it = events.begin();
                    if(it != events.end())
                    {
                        cdpevent = *it;
                    }
                }
                DebugPrintf(SV_LOG_DEBUG,"%s: Number of events found %d\n",FUNCTION_NAME,events.size());

			}
			else
            {
                DebugPrintf(SV_LOG_DEBUG,"%s: Did not find the requested bookmark %s\n",FUNCTION_NAME,m_flush_and_hold_request.m_bookmark.c_str());
                break;
            }

        }
        else if(m_flush_and_hold_request.m_consistency_type == FLUSH_AND_HOLD_REQUEST::CRASH_CONSISTENCY)
        {
            app_consitent_point = false;
            rv = diff_startts < m_timeto_pause_apply_ts ? false:true;
        }
	} while (0);
    DebugPrintf(SV_LOG_DEBUG,"LEAVING %s\n",FUNCTION_NAME);

    return rv;
}

bool CDPV2Writer::clean_redo_logs(SV_UINT filecount)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERING %s\n",FUNCTION_NAME);
    for(SV_UINT i = 1;i <= filecount; i++)
    {
        std::stringstream filename_stream;
        filename_stream << m_cdpdir
			<< ACE_DIRECTORY_SEPARATOR_CHAR_A
			<< CDP_REDOLOG_FILENAME_PREFIX
			<< i
			<< CDP_REDOLOG_FILENAME_SUFFIX;
        std::string redo_log_file_name = string(filename_stream.str());
        ACE_stat file_stat = {0};
        if(sv_stat(getLongPathName(redo_log_file_name.c_str()).c_str(),&file_stat) == 0)
        {
            if(ACE_OS::unlink(getLongPathName(redo_log_file_name.c_str()).c_str()) < 0) 
            {
                rv = false;
                DebugPrintf(SV_LOG_ERROR, "FUNCTION %s: Unable to delete redolog file %s\n",FUNCTION_NAME,redo_log_file_name.c_str());
                break;
            }
        }
    }
    return rv;
    DebugPrintf(SV_LOG_DEBUG,"LEAVING %s\n",FUNCTION_NAME);
}

bool CDPV2Writer::is_valid_point(CDPDRTDsMatchingCndn & cndn, std::string & error_msg)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERING %s\n",FUNCTION_NAME);

    do
    {
        CDPDatabase db(m_dbname);
        SV_ULONGLONG start_ts = 0;
        SV_ULONGLONG end_ts = 0;
        if(!db.getrecoveryrange(start_ts,end_ts))
        {
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to get cdp summary for volume %s\n",FUNCTION_NAME,m_volumename.c_str());
            rv = false;
            break;
        }

        if(start_ts == 0)
        {
            std::stringstream message;
            message << "Recovery ranges not available for volume "
                    << m_volumename
                    << "\n";
            DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
            error_msg = message.str();
            rv = false;
            break;
        }

        if(cndn.toTime() > end_ts)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: No data to rollback.\n",m_volumename.c_str());
            rv = true;
            break;
        } 

        if(cndn.toTime() < start_ts)
        {
            std::stringstream message;
            message << "Requested point is beyond than the retention start time for volume "
                    << m_volumename
                    << "\n";
            DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
            error_msg = message.str();
            rv = false;
            break;
        }

        bool isconsistent = false;
        bool isavailable = false;
        CDPUtil::isCCP(m_dbname,cndn.toTime(),isconsistent,isavailable);
        if(!isconsistent)
        {
            rv = false;
            std::stringstream message;
            message << "The requested point of time to pause apply is not in write ordered mode for "
                    << m_volumename
                    << "\n";
            DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
            error_msg = message.str();
            break;
        }

        if(!isavailable)
        {
            rv = false;
            std::stringstream message;
            message << "The requested point of time to pause apply is already coalesced/non-available point"
                    << m_volumename
                    << "\n";
            DebugPrintf(SV_LOG_ERROR,"%s\n",message.str().c_str());
            error_msg = message.str();
            break;
        }

    }while(0);

    return rv;
    DebugPrintf(SV_LOG_DEBUG,"LEAVING %s\n",FUNCTION_NAME);
}

bool CDPV2Writer::isInFlushAndHoldPendingState()
{
    return (m_pair_state == VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING);
}

bool CDPV2Writer::isInFlushAndHoldState()
{
    return (m_pair_state == VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES);
}

bool CDPV2Writer::isInResumePendingState()
{
    return (m_pair_state == VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING);
}

bool CDPV2Writer::getFlushAndHoldRequestSettingsFromCx()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
    bool rv = true;
    do
    {
        if((rv = getFlushAndHoldRequestSettings(*TheConfigurator,m_volumename,m_flush_and_hold_request)))
        {
            std::stringstream message;
            message<<" Consistency type : "<<m_flush_and_hold_request.m_consistency_type<<"\n"
                   <<" Time to pause apply : "<<m_flush_and_hold_request.m_timeto_pause_apply <<"\n"
                   <<" Application type : "<<m_flush_and_hold_request.m_apptype<<"\n"
                   <<" Bookmark : "<<m_flush_and_hold_request.m_bookmark<<"\n";
            DebugPrintf(SV_LOG_DEBUG,"%s",message.str().c_str());
            if(!m_flush_and_hold_request.m_timeto_pause_apply.empty()) 
            {
                SV_TIME svtime;

                sscanf(m_flush_and_hold_request.m_timeto_pause_apply.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                       &svtime.wYear,
                       &svtime.wMonth,
                       &svtime.wDay,
                       &svtime.wHour,
                       &svtime.wMinute,
                       &svtime.wSecond,
                       &svtime.wMilliseconds,
                       &svtime.wMicroseconds,
                       &svtime.wHundrecNanoseconds);

                if(!CDPUtil::ToFileTime(svtime, m_timeto_pause_apply_ts))	{
                    DebugPrintf(SV_LOG_ERROR,"Incorrect FLUSH AND HOLD REQUEST SETTINGS : %s\n",message.str().c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Incorrect FLUSH AND HOLD REQUEST SETTINGS : %s\n",message.str().c_str());
                rv = false;
            }

            break;
        }
    }while(!CDPUtil::QuitRequested(60));

    DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
    return rv;
}


bool CDPV2Writer::open_volume_handle()
{
    bool rv = true;

    do
    {
		if (isAzureTarget())
		{
			m_volhandle = ACE_INVALID_HANDLE;
			rv = true;
			break;
		}

        if(m_ismultisparsefile)
        {
            rv = false;
            break;
        }

		if (cdp_volume_t::CDP_DISK_TYPE == DeviceType()) {
			rv = GetdiskHandle();
			if (!rv){
				break;
			}
		}
		else{
			int openMode = O_RDWR;
			if (m_useAsyncIOs)
			{
				openMode |= FILE_FLAG_OVERLAPPED;
			}

			if (m_useUnBufferedIo)
			{
				setdirectmode(openMode);
			}

			// PR# 28917 - raw device during initial sync and resync gives
			// better performance than block device on AIX

			std::string final_volume_path = m_volguid;
#ifdef SV_AIX
			if (!isInDiffSync())
				final_volume_path = GetRawVolumeName(m_volguid);
#endif

			// PR#10815: Long Path support
			m_volhandle = ACE_OS::open(getLongPathName(final_volume_path.c_str()).c_str(),
				openMode,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);
		}

        if(ACE_INVALID_HANDLE == m_volhandle)
        {
            DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                        m_volumename.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        if(m_useAsyncIOs)
        {
            m_wfptr.reset(new ACE_Asynch_Write_File);
            if(m_wfptr -> open (*this, m_volhandle) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "asynch open for write on %s failed. error no:%d\n",
                            m_volumename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }

            m_rfptr.reset(new ACE_Asynch_Read_File);
            if(m_rfptr -> open (*this, m_volhandle) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "asynch open for read on %s failed. error no:%d\n",
                            m_volumename.c_str(), ACE_OS::last_error());
                rv = false;
                break;
            }
        }

    } while(0);

    return rv;
}

bool CDPV2Writer::close_volume_handle()
{
    m_rfptr.reset();
    m_wfptr.reset();

    if(ACE_INVALID_HANDLE != m_volhandle)
    {
        ACE_OS::close(m_volhandle);
        m_volhandle = ACE_INVALID_HANDLE;
    }
    return true;
}

void CDPV2Writer::profile_source_io(DiffPtr change_metadata)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    src_ioLessThan512 = 0;
    src_ioLessThan1K = 0;
    src_ioLessThan2K = 0;
    src_ioLessThan4K = 0;
    src_ioLessThan8K = 0;
    src_ioLessThan16K = 0;
    src_ioLessThan32K = 0;
    src_ioLessThan64K = 0;
    src_ioLessThan128K = 0;
    src_ioLessThan256K = 0;
    src_ioLessThan512K = 0;
    src_ioLessThan1M = 0;
    src_ioLessThan2M = 0;
    src_ioLessThan4M = 0;
    src_ioLessThan8M = 0;
    src_ioGreaterThan8M = 0;
    src_ioTotal = 0;


    cdp_drtdv2s_constiter_t drtds_remaining_iter = change_metadata -> DrtdsBegin();
    cdp_drtdv2s_constiter_t drtds_end = change_metadata -> DrtdsEnd();
    SV_UINT io_length = 0;

    while(drtds_remaining_iter != drtds_end)
    {
        io_length =(*drtds_remaining_iter).get_length();
        ++drtds_remaining_iter;

        src_ioTotal += io_length;

        if(io_length <= SV_DEFAULT_IO_SIZE_BUCKET_0)
        {
            ++src_ioLessThan512;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_1)
        {
            ++src_ioLessThan1K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_2)
        {
            ++src_ioLessThan2K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_3)
        {
            ++src_ioLessThan4K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_4)
        {
            ++src_ioLessThan8K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_5)
        {
            ++src_ioLessThan16K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_6)
        {
            ++src_ioLessThan32K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_7)
        {
            ++src_ioLessThan64K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_8)
        {
            ++src_ioLessThan128K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_9)
        {
            ++src_ioLessThan256K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_10)
        {
            ++src_ioLessThan512K;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_11)
        {
            ++src_ioLessThan1M;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_12)
        {
            ++src_ioLessThan2M;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_13)
        {
            ++src_ioLessThan4M;
		}
		else if (io_length <= SV_DEFAULT_IO_SIZE_BUCKET_14)
        {
            ++src_ioLessThan8M;
		}
		else
        {
            ++src_ioGreaterThan8M;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void CDPV2Writer::profile_volume_read()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    vr_ioLessThan512 = 0;
    vr_ioLessThan1K = 0;
    vr_ioLessThan2K = 0;
    vr_ioLessThan4K = 0;
    vr_ioLessThan8K = 0;
    vr_ioLessThan16K = 0;
    vr_ioLessThan32K = 0;
    vr_ioLessThan64K = 0;
    vr_ioLessThan128K = 0;
    vr_ioLessThan256K = 0;
    vr_ioLessThan512K = 0;
    vr_ioLessThan1M = 0;
    vr_ioLessThan2M = 0;
    vr_ioLessThan4M = 0;
    vr_ioLessThan8M = 0;
    vr_ioGreaterThan8M = 0;
    vr_ioTotal = 0;


    std::map<SV_UINT,  cdp_cow_location_t>::iterator myiter = m_cowlocations.begin();

    for( ; myiter != m_cowlocations.end(); myiter++)
    {

        cdp_cow_location_t c = myiter ->second;
        for(int i = 0; i < c.nonoverlaps.size(); i++)
        {
            vr_ioTotal += c.nonoverlaps[i].length;
            if(c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_0)
            {
                ++vr_ioLessThan512;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_1)
            {
                ++vr_ioLessThan1K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_2)
            {
                ++vr_ioLessThan2K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_3)
            {
                ++vr_ioLessThan4K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_4)
            {
                ++vr_ioLessThan8K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_5)
            {
                ++vr_ioLessThan16K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_6)
            {
                ++vr_ioLessThan32K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_7)
            {
                ++vr_ioLessThan64K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_8)
            {
                ++vr_ioLessThan128K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_9)
            {
                ++vr_ioLessThan256K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_10)
            {
                ++vr_ioLessThan512K;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_11)
            {
                ++vr_ioLessThan1M;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_12)
            {
                ++vr_ioLessThan2M;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_13)
            {
                ++vr_ioLessThan4M;
			}
			else if (c.nonoverlaps[i].length <= SV_DEFAULT_IO_SIZE_BUCKET_14)
            {
                ++vr_ioLessThan8M;
            } 
            else
            {
                ++vr_ioGreaterThan8M;
            }
        }
    } 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void CDPV2Writer::profile_volume_write(const cdpv3metadata_t & vol_metadata)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    vw_ioLessThan512 = 0;
    vw_ioLessThan1K = 0;
    vw_ioLessThan2K = 0;
    vw_ioLessThan4K = 0;
    vw_ioLessThan8K = 0;
    vw_ioLessThan16K = 0;
    vw_ioLessThan32K = 0;
    vw_ioLessThan64K = 0;
    vw_ioLessThan128K = 0;
    vw_ioLessThan256K = 0;
    vw_ioLessThan512K = 0;
    vw_ioLessThan1M = 0;
    vw_ioLessThan2M = 0;
    vw_ioLessThan4M = 0;
    vw_ioLessThan8M = 0;
    vw_ioGreaterThan8M = 0;
    vw_ioTotal = 0;


    cdp_drtdv2s_constiter_t myiter = vol_metadata.m_drtds.begin();

    for( ; myiter != vol_metadata.m_drtds.end(); myiter++)
    {
        SV_UINT length = (*myiter).get_length();

        vw_ioTotal +=  length;
        if(length <= SV_DEFAULT_IO_SIZE_BUCKET_0)
        {
            ++vw_ioLessThan512;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_1)
        {
            ++vw_ioLessThan1K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_2)
        {
            ++vw_ioLessThan2K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_3)
        {
            ++vw_ioLessThan4K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_4)
        {
            ++vw_ioLessThan8K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_5)
        {
            ++vw_ioLessThan16K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_6)
        {
            ++vw_ioLessThan32K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_7)
        {
            ++vw_ioLessThan64K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_8)
        {
            ++vw_ioLessThan128K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_9)
        {
            ++vw_ioLessThan256K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_10)
        {
            ++vw_ioLessThan512K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_11)
        {
            ++vw_ioLessThan1M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_12)
        {
            ++vw_ioLessThan2M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_13)
        {
            ++vw_ioLessThan4M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_14)
        {
            ++vw_ioLessThan8M;
		}
		else
        {
            ++vw_ioGreaterThan8M;
        }
    } 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void CDPV2Writer::profile_cdp_write(const cdpv3metadata_t & cow_metadata)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    cdp_ioLessThan512 = 0;
    cdp_ioLessThan1K = 0;
    cdp_ioLessThan2K = 0;
    cdp_ioLessThan4K = 0;
    cdp_ioLessThan8K = 0;
    cdp_ioLessThan16K = 0;
    cdp_ioLessThan32K = 0;
    cdp_ioLessThan64K = 0;
    cdp_ioLessThan128K = 0;
    cdp_ioLessThan256K = 0;
    cdp_ioLessThan512K = 0;
    cdp_ioLessThan1M = 0;
    cdp_ioLessThan2M = 0;
    cdp_ioLessThan4M = 0;
    cdp_ioLessThan8M = 0;
    cdp_ioGreaterThan8M = 0;
    cdp_ioTotal = 0;

    std::map<int,unsigned long long> iosizes;
    cdp_drtdv2s_constiter_t myiter = cow_metadata.m_drtds.begin();

    for( ; myiter != cow_metadata.m_drtds.end(); myiter++)
    {
        cdp_drtdv2_t drtd = *myiter;
        if(iosizes.find(drtd.get_fileid()) == iosizes.end())
            iosizes.insert(std::make_pair(drtd.get_fileid(),drtd.get_length()));
        else
            iosizes[drtd.get_fileid()] += drtd.get_length();
    }

    std::map<int,unsigned long long>::iterator iiter = iosizes.begin();
    for(; iiter != iosizes.end(); ++iiter)
    {
        SV_ULONGLONG length =  iiter ->second;

        cdp_ioTotal +=  length;
        if(length <= SV_DEFAULT_IO_SIZE_BUCKET_0)
        {
            ++cdp_ioLessThan512;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_1)
        {
            ++cdp_ioLessThan1K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_2)
        {
            ++cdp_ioLessThan2K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_3)
        {
            ++cdp_ioLessThan4K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_4)
        {
            ++cdp_ioLessThan8K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_5)
        {
            ++cdp_ioLessThan16K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_6)
        {
            ++cdp_ioLessThan32K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_7)
        {
            ++cdp_ioLessThan64K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_8)
        {
            ++cdp_ioLessThan128K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_9)
        {
            ++cdp_ioLessThan256K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_10)
        {
            ++cdp_ioLessThan512K;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_11)
        {
            ++cdp_ioLessThan1M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_12)
        {
            ++cdp_ioLessThan2M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_13)
        {
            ++cdp_ioLessThan4M;
		}
		else if (length <= SV_DEFAULT_IO_SIZE_BUCKET_14)
        {
            ++cdp_ioLessThan8M;
		}
		else
        {
            ++cdp_ioGreaterThan8M;
        }
    } 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPV2Writer::write_perf_stats(const std::string &filepath)
{
    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(filepath.c_str()).c_str(), std::ios::app);

    if(!out) 
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                    FUNCTION_NAME, filepath.c_str(), LINE_NO,FILE_NAME);
        return false;
    }

    if(m_cownonoverlap_computation_time)
        m_cownonoverlap_computation_time -= m_bitmapread_time;

    double totaltime = (m_diffread_time 
                        + m_diffuncompress_time 
                        + m_diffparse_time 
                        + m_metadata_time
                        + m_cownonoverlap_computation_time
                        + m_volread_time
                        + m_cowwrite_time
                        + m_vsnapupdate_time
                        + m_dboperation_time
                        + m_volwrite_time
                        + m_volnonoverlap_computation_time
                        + m_bitmapread_time
                        +m_bitmapwrite_time);

    out << m_diffsize << ","
        << m_total_time << ","
        << m_diffread_time << ","
        << m_diffuncompress_time << ","
        << m_diffparse_time << ","
        << m_cownonoverlap_computation_time << ","
        << m_metadata_time << ","
        << m_volread_time << ","
        << m_cowwrite_time << ","
        << m_vsnapupdate_time << ","
        << m_dboperation_time << ","
        << m_volwrite_time << ","
        << m_volnonoverlap_computation_time << ","
        << m_bitmapread_time << ","
        << m_bitmapwrite_time << ","
        << totaltime << "\n";

    if(!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed writing perf counters to %s.\n",
                    filepath.c_str());
        return false;
    }

    return true;
}

bool CDPV2Writer::write_src_profile_stats(const std::string &filepath)
{
    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(filepath.c_str()).c_str(), std::ios::app);

    if(!out) 
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                    FUNCTION_NAME, filepath.c_str(), LINE_NO,FILE_NAME);
        return false;
    }

    out << m_diffsize << ","
        << src_ioLessThan512 << ","
        << src_ioLessThan1K << ","
        << src_ioLessThan2K << ","
        << src_ioLessThan4K << ","
        << src_ioLessThan8K << ","
        << src_ioLessThan16K << ","
        << src_ioLessThan32K << ","
        << src_ioLessThan64K << ","
        << src_ioLessThan128K << ","
        << src_ioLessThan256K << ","
        << src_ioLessThan512K << ","
        << src_ioLessThan1M << ","
        << src_ioLessThan2M << ","
        << src_ioLessThan4M << ","
        << src_ioLessThan8M << ","
        << src_ioGreaterThan8M << ","
        << src_ioTotal << "\n";


    if(!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed writing perf counters to %s.\n",
                    filepath.c_str());
        return false;
    }

    return true;
}

bool CDPV2Writer::write_volread_profile_stats(const std::string &filepath)
{
    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(filepath.c_str()).c_str(), std::ios::app);

    if(!out) 
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                    FUNCTION_NAME, filepath.c_str(), LINE_NO,FILE_NAME);
        return false;
    }

    out << m_diffsize << ","
        << vr_ioLessThan512 << ","
        << vr_ioLessThan1K << ","
        << vr_ioLessThan2K << ","
        << vr_ioLessThan4K << ","
        << vr_ioLessThan8K << ","
        << vr_ioLessThan16K << ","
        << vr_ioLessThan32K << ","
        << vr_ioLessThan64K << ","
        << vr_ioLessThan128K << ","
        << vr_ioLessThan256K << ","
        << vr_ioLessThan512K << ","
        << vr_ioLessThan1M << ","
        << vr_ioLessThan2M << ","
        << vr_ioLessThan4M << ","
        << vr_ioLessThan8M << ","
        << vr_ioGreaterThan8M << ","
        << vr_ioTotal << "\n";


    if(!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed writing perf counters to %s.\n",
                    filepath.c_str());
        return false;
    }

    return true;
}

bool CDPV2Writer::write_volwrite_profile_stats(const std::string &filepath)
{
    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(filepath.c_str()).c_str(), std::ios::app);

    if(!out) 
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                    FUNCTION_NAME, filepath.c_str(), LINE_NO,FILE_NAME);
        return false;
    }

    out << m_diffsize << ","
        << vw_ioLessThan512 << ","
        << vw_ioLessThan1K << ","
        << vw_ioLessThan2K << ","
        << vw_ioLessThan4K << ","
        << vw_ioLessThan8K << ","
        << vw_ioLessThan16K << ","
        << vw_ioLessThan32K << ","
        << vw_ioLessThan64K << ","
        << vw_ioLessThan128K << ","
        << vw_ioLessThan256K << ","
        << vw_ioLessThan512K << ","
        << vw_ioLessThan1M << ","
        << vw_ioLessThan2M << ","
        << vw_ioLessThan4M << ","
        << vw_ioLessThan8M << ","
        << vw_ioGreaterThan8M << ","
        << vw_ioTotal << "\n";


    if(!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed writing perf counters for to %s.\n",
                    filepath.c_str());
        return false;
    }

    return true;
}


bool CDPV2Writer::write_cdpwrite_profile_stats(const std::string &filepath)
{
    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(filepath.c_str()).c_str(), std::ios::app);

    if(!out) 
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                    FUNCTION_NAME, filepath.c_str(), LINE_NO,FILE_NAME);
        return false;
    }

    out << m_diffsize << ","
        << cdp_ioLessThan512 << ","
        << cdp_ioLessThan1K << ","
        << cdp_ioLessThan2K << ","
        << cdp_ioLessThan4K << ","
        << cdp_ioLessThan8K << ","
        << cdp_ioLessThan16K << ","
        << cdp_ioLessThan32K << ","
        << cdp_ioLessThan64K << ","
        << cdp_ioLessThan128K << ","
        << cdp_ioLessThan256K << ","
        << cdp_ioLessThan512K << ","
        << cdp_ioLessThan1M << ","
        << cdp_ioLessThan2M << ","
        << cdp_ioLessThan4M << ","
        << cdp_ioLessThan8M << ","
        << cdp_ioGreaterThan8M << ","
        << cdp_ioTotal << "\n";


    if(!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed writing perf counters for to %s.\n",
                    filepath.c_str());
        return false;
    }

    return true;
}


//====================================================================
// Azure Specific changes
// When the target is azure, 
//  For IR/resync - the changes are applied to base blob using MARS agent interface
//  For resync step2/diffsync
//   the files are accumulated in <azureCachFolder> until
//    	The upload pending size reaches/exceeds configured limit OR
//      session recoverability state is not same as file being processed OR
//      There are no more files available in PS/cachemgr cache folder
//   If any of the above conditions are met, it is ready for upload
// 
//=====================================================================

bool CDPV2Writer::AzureApply(const std::string & fileToApply, bool isLastFileToProcess)
{
	INM_TRACE_ENTER_FUNC

	bool rv = true;

	try {
		// algorithm

		// 
		// if first invocation, 
		//   upload files that are pending
		//   delete files that have been uploaded to azure
		//   reset first invocation flag

		// note: first invocation logic is handled in Applychanges routine

		// if we are starting a new session
		//   set the session recoverability to current file

		// if session recoverability state matches with file to process, 
		//   move the file to pending upload folder
		//   add the file to current session
		//   add up the file size to pending upload size
		//   

		// check if we are ready for upload
		//  you are ready if any of following conditions are met :
		//    	The upload pending size reaches/exceeds configured limit OR
		//      session recoverability state is not same as file being processed OR
		//      There are no more files available in PS/cachemgr cache folder
		// if ready to upload, 
		//   upload the files to azure
		//   rename the pendingupload folder to uploaded folder
		//   cleanup and delete the uploaded folder
		//   clear the in-memory list of files to be uploaded

		// are we in resync step 2, move to diffsync if we have applied (uploaded) all changes of step 2

		// if current file was not processed (it had different recoverability state from session recoverability state)
		//   start new session
		//   set the session recoverability to current file
		//   move the file to upload folder
		//   add the file to current session
		//   add up the file size to pending upload size
		// 

		bool uploadedToAzure = false;

		if (!m_AzureAgent){
			throw INMAGE_EX("internal error: diffsync azure agent is not initialized")(m_volumename);
		}

		SetResetLastFileToProcess(isLastFileToProcess);

		FindCurrentFileRecoverabilityState(fileToApply);

		if (isNewSession()){
			SetSessionRecoverabilityState(isCurrentFileContainingRecoverablePoints());

		}


		bool fileAddedToCurrentSession = false;
		std::string fileToUpload;
		if (isSessionContainingRecoverabilePoints() == isCurrentFileContainingRecoverablePoints()){
			fileToUpload = MoveCurrentfileToPendingUpload(fileToApply);
			addFileToCurrentSession(fileToUpload);
			UpdateLastAppliedFileInformation();
			fileAddedToCurrentSession = true;
		}

		if (isReadyForUpload(fileToApply)){
			UploadPendingFilesToAzure();
			RenamePendingUploadFolderToUploaded();
			DeleteAzureFolder(UploadedFolder());
			CleanupCurrrentSession();
			uploadedToAzure = true;
		}
		else if (!fileAddedToCurrentSession) {

			// only case file is not added to current session:
			//  file recoverable state is not same as session (in this scenario, isReadyForUpload should return true)
			//
			throw INMAGE_EX("internal error, current file not processed.")(m_volumename)(fileToApply);
		}

		if (fileAddedToCurrentSession && uploadedToAzure && isInResyncStep2() && ReadyToTransitionToDiffSync(fileToApply)){
			TransitionToDiffSync();
		}

		if (!fileAddedToCurrentSession){
			SetSessionRecoverabilityState(isCurrentFileContainingRecoverablePoints());
			fileToUpload = MoveCurrentfileToPendingUpload(fileToApply);
			addFileToCurrentSession(fileToUpload);
			UpdateLastAppliedFileInformation();
			fileAddedToCurrentSession = true;
		}

		// validate file has been processed
		if (!fileAddedToCurrentSession){

			throw INMAGE_EX("internal error, current file not processed.")(m_volumename)(fileToApply);
		}
	}
	catch (ContextualException& ce)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch (std::exception const& e)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch (...)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	INM_TRACE_EXIT_FUNC

	return rv;
}

bool CDPV2Writer::AzureApplyPendingFilesFromPreviousSession(const std::string & firstFileInNewSession)
{
	INM_TRACE_ENTER_FUNC

	bool rv = true;

	try {

		// if first invocation, 
		//   upload files that are pending
		//   update the last applied file information
		//   delete files that have been uploaded to azure
		//   reset first invocation flag

		if (!m_firstInvocation) {
			throw INMAGE_EX("internal error, upload previous session file(s) called more than once")(m_volumename);
		}

		initAzureSpecifParams(firstFileInNewSession);
		GetPendingDataFromPreviousSession();
		if (isPendingFilesForUpload()){
			UploadPendingFilesToAzure();
			UpdateLastAppliedFileInformation();
			RenamePendingUploadFolderToUploaded();
			DeleteAzureFolder(UploadedFolder());
			CleanupCurrrentSession();
		}
		else{
			DeleteStaleOrEmptyAzureUploadFolders();
		}

		m_firstInvocation = false;
	}
	catch (AzureCancelOpException & ce) {

		//  cancel the current resync and restart fresh session
		//  this is achieved by just sending resync required flag to cs
		
		rv = false;

        const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
        std::stringstream msg("encountered AzureCancelOpException ");
        msg << ce.what() << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
		CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp);

		CDPUtil::QuitRequested(600);
	}
	catch (AzureInvalidOpException & ie) {

		// pause the replication pair
		// example, after a failover operation you cannot upload files, start resync etc operations

		rv = false;
		ReplicationPairOperations pairOp;
		stringstream replicationPauseMsg;
		replicationPauseMsg << "Replication for " << m_volumename
			<< "is paused due to unrecoverable error(" 
			<< std::hex << ie.ErrorCode() 
			<< ")." ;

		if (pairOp.PauseReplication(TheConfigurator, m_volumename, INMAGE_HOSTTYPE_TARGET, replicationPauseMsg.str().c_str(), ie.ErrorCode())) {
			DebugPrintf(SV_LOG_ERROR, replicationPauseMsg.str().c_str());
		}
		else {
			DebugPrintf(SV_LOG_ERROR, "Request '%s' to CS failed", replicationPauseMsg.str().c_str());
		}

		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ie.what());

		CDPUtil::QuitRequested(600);
	}
	catch (ContextualException& ce)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch (std::exception const& e)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch (...)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	INM_TRACE_EXIT_FUNC

	return rv;
}

void CDPV2Writer::initAzureSpecifParams(const std::string & fileToApply)
{
	INM_TRACE_ENTER_FUNC

	if (!m_firstInvocation) {
		throw INMAGE_EX("internal error, initialization called more than once")(m_volumename);
	}

	boost::filesystem::path diffFilePath(fileToApply);

	boost::filesystem::path uploadPathNonRecoverablePoints(diffFilePath.parent_path());
	uploadPathNonRecoverablePoints /= "AzurePendingUploadNonRecoverable";

	boost::filesystem::path uploadPathRecoverablePoints(diffFilePath.parent_path());
	uploadPathRecoverablePoints /= "AzurePendingUploadRecoverable";

	boost::filesystem::path uploadedFolder(diffFilePath.parent_path());
	uploadedFolder /= "AzureUploaded";

	m_azureCacheFolderForRecoverablePoints = ExtendedLengthPath::name(uploadPathRecoverablePoints.string());
	m_azureCacheFolderForNonRecoverablePoints = ExtendedLengthPath::name(uploadPathNonRecoverablePoints.string());
	m_azurePendingUploadFolder = m_azureCacheFolderForNonRecoverablePoints;
	m_azureUploadedFolder = ExtendedLengthPath::name(uploadedFolder.string());
	m_isSessionHavingRecoverablePoints = false;
	m_isCurrentFileContainsRecoverablePoints = false;

	std::string uploadTrackingLog(m_appliedInfoDir);
	uploadTrackingLog += ACE_DIRECTORY_SEPARATOR_STR_A;
	uploadTrackingLog += "azureupload.log";

	m_azureTrackingUploadLog = ExtendedLengthPath::name(uploadTrackingLog);

	// check for  resync required flag from previous session

	std::string resyncRequiredFilePath = m_appliedInfoDir;
	resyncRequiredFilePath += ACE_DIRECTORY_SEPARATOR_STR_A;
	resyncRequiredFilePath += "resyncRequired";
	ACE_stat file_stat = { 0 };
	// PR#10815: Long Path support
	if (sv_stat(getLongPathName(resyncRequiredFilePath.c_str()).c_str(), &file_stat) == 0)
	{
		m_resyncRequired = true;
	}
	else
	{
		m_resyncRequired = false;
	}

	if (isInResyncStep2())
	{
		m_AzureAgent -> CompleteResync(true);
	}

	
	INM_TRACE_EXIT_FUNC
}


void CDPV2Writer::GetPendingDataFromPreviousSession()
{
	// algorithm
	//  check for upload pending non-recoverable folder
	//  check for upload pending recoverable folder
	//  check for uploaded folder
	//  if more than one folder exists, then we have a bug - throw exception
	//  call agent api for upload
	// 

	INM_TRACE_ENTER_FUNC

	const std::string matchPrefix = "completed_ediffcompleted_diff";

	bool pendingUploadFolderWithRecoverablePointsExists = boost::filesystem::exists(PendingUploadFolderForRecoverablePoints());
	bool pendingUploadFolderWithNonRecoverablePointsExists = boost::filesystem::exists(PendingUploadFolderForNonRecoverablePoints());
	bool uploadedFolderExists = boost::filesystem::exists(UploadedFolder());

	unsigned int folderCount = pendingUploadFolderWithRecoverablePointsExists
		+ pendingUploadFolderWithNonRecoverablePointsExists
		+ uploadedFolderExists;

	// nothing pending
	if (0 == folderCount){
		return;
	}

	if (folderCount > 1) {
		DeleteStaleOrEmptyAzureUploadFolders();
		throw INMAGE_EX("internal error, multiple folders exists for pending upload/uploaded")(m_volumename);
	}


	if (pendingUploadFolderWithRecoverablePointsExists) {

		if (!boost::filesystem::is_directory(PendingUploadFolderForRecoverablePoints())) {
			throw INMAGE_EX("internal error, pending upload not a directory")(ExtendedLengthPath::nonExtName(PendingUploadFolderForRecoverablePoints().string()));
		}

		DiffSortCriterion::OrderedEndTime_t orderedDataWithRecoverablePoints;

		extendedLengthPathDirIter_t pendingUploadFilesIter(PendingUploadFolderForRecoverablePoints());
		extendedLengthPathDirIter_t pendingUploadFilesIterEnd;

		for (/* empty */; pendingUploadFilesIter != pendingUploadFilesIterEnd; ++pendingUploadFilesIter) {

			std::string nonExtFileName = ExtendedLengthPath::nonExtName((*pendingUploadFilesIter).path().filename());
			std::string nonExtPathName = ExtendedLengthPath::nonExtName((*pendingUploadFilesIter).path().string());

			if (boost::filesystem::is_regular_file(pendingUploadFilesIter->status()) &&
				(0 == strncmp(nonExtFileName.c_str(), matchPrefix.c_str(), matchPrefix.length()))) {

				orderedDataWithRecoverablePoints.insert(DiffSortCriterion(nonExtPathName.c_str(), std::string()));
			}
		}

		if (!orderedDataWithRecoverablePoints.empty()) {
			addFilesToCurrentSession(orderedDataWithRecoverablePoints, true);
		}
	}

	if (pendingUploadFolderWithNonRecoverablePointsExists) {

		if (!boost::filesystem::is_directory(PendingUploadFolderForNonRecoverablePoints())) {
			throw INMAGE_EX("internal error, pending upload not a directory")(ExtendedLengthPath::nonExtName(PendingUploadFolderForNonRecoverablePoints().string()));
		}

		DiffSortCriterion::OrderedEndTime_t orderedDataWithNonRecoverablePoints;

		extendedLengthPathDirIter_t pendingUploadFilesIter(PendingUploadFolderForNonRecoverablePoints());
		extendedLengthPathDirIter_t pendingUploadFilesIterEnd;

		for (/* empty */; pendingUploadFilesIter != pendingUploadFilesIterEnd; ++pendingUploadFilesIter) {

			std::string nonExtFileName = ExtendedLengthPath::nonExtName((*pendingUploadFilesIter).path().filename());
			std::string nonExtPathName = ExtendedLengthPath::nonExtName((*pendingUploadFilesIter).path().string());

			if (boost::filesystem::is_regular_file(pendingUploadFilesIter->status()) &&
				(0 == strncmp(nonExtFileName.c_str(), matchPrefix.c_str(), matchPrefix.length()))) {

				orderedDataWithNonRecoverablePoints.insert(DiffSortCriterion(nonExtPathName.c_str(), std::string()));
			}
		}

		if (!orderedDataWithNonRecoverablePoints.empty()) {
			addFilesToCurrentSession(orderedDataWithNonRecoverablePoints, false);
		}
	}

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::DeleteStaleOrEmptyAzureUploadFolders()
{
	INM_TRACE_ENTER_FUNC


	bool pendingUploadFolderWithRecoverablePointsExists = boost::filesystem::exists(PendingUploadFolderForRecoverablePoints());
	bool pendingUploadFolderWithNonRecoverablePointsExists = boost::filesystem::exists(PendingUploadFolderForNonRecoverablePoints());
	bool uploadedFolderExists = boost::filesystem::exists(UploadedFolder());

	unsigned int folderCount = pendingUploadFolderWithRecoverablePointsExists
		+ pendingUploadFolderWithNonRecoverablePointsExists
		+ uploadedFolderExists;

	// nothing pending
	if (0 == folderCount){
		return;
	}

	if (pendingUploadFolderWithRecoverablePointsExists) {

		if (!boost::filesystem::is_directory(PendingUploadFolderForRecoverablePoints())) {
			throw INMAGE_EX("internal error, pending upload not a directory")(ExtendedLengthPath::nonExtName(PendingUploadFolderForRecoverablePoints().string()));
		}

		DeleteAzureFolder(PendingUploadFolderForRecoverablePoints());
	}

	if (pendingUploadFolderWithNonRecoverablePointsExists) {

		if (!boost::filesystem::is_directory(PendingUploadFolderForNonRecoverablePoints())) {
			throw INMAGE_EX("internal error, pending upload not a directory")(ExtendedLengthPath::nonExtName(PendingUploadFolderForNonRecoverablePoints().string()));
		}

		DeleteAzureFolder(PendingUploadFolderForNonRecoverablePoints());
	}

	if (uploadedFolderExists) {

		if (!boost::filesystem::is_directory(UploadedFolder())) {
			throw INMAGE_EX("internal error, pending upload not a directory")(ExtendedLengthPath::nonExtName(UploadedFolder().string()));
		}

		DeleteAzureFolder(UploadedFolder());
	}


	INM_TRACE_EXIT_FUNC
}

bool CDPV2Writer::isDiffFromCurrentResyncSession(const DiffSortCriterion & fileInfo)
{
	INM_TRACE_ENTER_FUNC

	bool bFileFromCurrentSession = true;

	SV_ULONGLONG diffEndTime = fileInfo.EndTime();
	SV_ULONGLONG diffEndTimeSeq = fileInfo.EndTimeSequenceNumber();

	SV_ULONGLONG  resyncStartTime = m_Configurator->getResyncStartTimeStamp(m_volumename);
	SV_ULONG      resyncStartTimeSeq = m_Configurator->getResyncStartTimeStampSeq(m_volumename);

	// do not apply files which bear timestamp earlier than
	// the resync start time. we just delete them

	if ((diffEndTime < resyncStartTime) ||
		((diffEndTime == resyncStartTime) && (diffEndTimeSeq < resyncStartTimeSeq))) {
		std::stringstream msg;
		msg << "Ignoring differential file " << fileInfo.Id() << " with timestamp less than"
			<< " resync start time(TS:" << resyncStartTime << " Seq:" << resyncStartTimeSeq << ").\n";
		DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
		bFileFromCurrentSession = false;
	}

	INM_TRACE_EXIT_FUNC

	return bFileFromCurrentSession;
}

void CDPV2Writer::addFilesToCurrentSession(DiffSortCriterion::OrderedEndTime_t & pendingdataFromPreviousSession, bool containsrecoverablePoints)
{
	INM_TRACE_ENTER_FUNC

	DiffSortCriterion::OrderedEndTime_t::iterator iter(pendingdataFromPreviousSession.begin());
	DiffSortCriterion::OrderedEndTime_t::iterator endIter(pendingdataFromPreviousSession.end());

	SetSessionRecoverabilityState(containsrecoverablePoints);

	for (/*empty*/; iter != endIter; ++iter) {

		// do not apply files which bear timestamp earlier than
		// the resync start time. we just delete them
		
		if (isDiffFromCurrentResyncSession(*iter)){
			addFileToCurrentSession((*iter).Id());
		} else
		{
			std::string fileToRemove = (*iter).Id();
			boost::filesystem::remove(ExtendedLengthPath::name(fileToRemove));
		}
	}

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::SetSessionRecoverabilityState(bool containsRecoverablePoints, bool force)
{
	INM_TRACE_ENTER_FUNC

	if (!force) {
		if (!isNewSession()){
			throw INMAGE_EX("internal error, changing recovery state for in-progress session") (m_volumename);
		}

		if (0 != m_uploadSize) {
			throw INMAGE_EX("internal error, changing recovery state for in-progress session, uploadSize of current session is non-zero")(m_volumename)(m_uploadSize);
		}
	}

	m_isSessionHavingRecoverablePoints = containsRecoverablePoints;
	if (m_isSessionHavingRecoverablePoints) {
		m_azurePendingUploadFolder = m_azureCacheFolderForRecoverablePoints;
	}
	else {
		m_azurePendingUploadFolder = m_azureCacheFolderForNonRecoverablePoints;
	}

	if (!boost::filesystem::exists(m_azurePendingUploadFolder)) {
		boost::filesystem::create_directory(m_azurePendingUploadFolder);
	}

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::UploadPendingFilesToAzure()
{
	INM_TRACE_ENTER_FUNC

	// call mars agent api for upload
	// UploadLog(source host id, source device name, PendingUploadFolder(), m_orderedFileNames, m_isSessionHavingRecoverablePoints)

	// Return values of MARS api
	// success
	// already uploaded previously -> also returned as success
	// failed to upload as protection service has detected a resync required and we are passing containsallRecoverablePoints as true 
	//    --> update the resync required flag locally and also to cs
	//    --> change session  to non recoverable (ChangePendingUploadStateToNonRecoverable())
	//    --> retry upload with containsallRecoverablePoints as false
	// failed to upload as protection service has detected a missing file
	//    --> update the resync required flag locally and also to cs
	//    --> change session  to non recoverable (ChangePendingUploadStateToNonRecoverable())
	//    --> no need to retry upload (as it will never succeed)
	// throw exception for any other failure

	bool done = false;
	bool azureresyncRequiredException = false;
	bool azureClientResyncRequiredException = false;
	std::string exceptionMsg;
	long errorCode = 0;
	bool azuremissingfileException = false;
	bool azureinvalidopexception = false;

	while (!done){

		try{

			if (azureresyncRequiredException){
				
				if (!m_isSessionHavingRecoverablePoints){
					throw INMAGE_EX("internal error: Azure error returned resync required exception when session is already having resync flag set")(m_volumename);
				}

                m_resyncRequired = true;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                std::stringstream msg("encountered azureresyncRequiredException: ");
                msg << exceptionMsg << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp, false, errorCode))
				{
					throw INMAGE_EX("internal error: unable to persist resync required")(m_volumename);
				}

				ChangePendingUploadStateToNonRecoverable();

			}
			else if (azureClientResyncRequiredException){

				m_resyncRequired = true;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                std::stringstream msg("encountered azureClientResyncRequiredException: ");
                msg << exceptionMsg << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp, false, errorCode))
				{
					throw INMAGE_EX("internal error: unable to persist resync required")(m_volumename);
				}

				// we are not going to retry the upload for cases: Zero sized file, VDL mismatch, Checksum mismatch, Invalid header.
				// we have set resync flag and will just delete the files.
				done = true;
			}
			else if (azuremissingfileException) {

				m_resyncRequired = true;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                std::stringstream msg("encountered azuremissingfileException: ");
                msg << exceptionMsg << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp, false, errorCode))
				{
					throw INMAGE_EX("internal error: unable to persist resync required")(m_volumename);
				}

				ChangePendingUploadStateToNonRecoverable();

				// we are not going to retry the upload as a file is missing,
				// we have set resync flag and will just delete the files
				done = true;
			}
			else if (azureinvalidopexception)
			{
				// action to be taken : pause the replication pair
				// scenario for this exception: after a failover operation you cannot upload files

				ReplicationPairOperations pairOp;
				stringstream replicationPauseMsg;
				replicationPauseMsg << "Replication for " << m_volumename
					<< "is paused due to unrecoverable error("
					<< std::hex << errorCode
					<< ").";

				if (pairOp.PauseReplication(TheConfigurator, m_volumename, INMAGE_HOSTTYPE_TARGET, replicationPauseMsg.str().c_str(), errorCode)) {
					DebugPrintf(SV_LOG_ERROR, replicationPauseMsg.str().c_str());
				}
				else {
					DebugPrintf(SV_LOG_ERROR, "Request '%s' to CS failed", replicationPauseMsg.str().c_str());
				}

				m_resyncRequired = true;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                std::stringstream msg("encountered azureinvalidopexception: ");
                msg << exceptionMsg << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                if (!CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp, false, errorCode))
				{
					throw INMAGE_EX("internal error: unable to persist resync required")(m_volumename);
				}

				CDPUtil::QuitRequested(600);

				// we are not going to retry the upload
				// we have set resync flag and will just delete the files
				done = true;
			}
			
			if (!done) {

				// azure api expects complete path (instead of folder and filename within the folder)
				// convert as per api signature
				std::vector<AzureAgentInterface::AzureDiffLogInfo> orderedAzureDiffInfos;


				for (size_t i = 0; i < m_orderedFileNames.size(); ++i){
					AzureAgentInterface::AzureDiffLogInfo azureDiffInfo;
					std::string filePath = ExtendedLengthPath::nonExtName(PendingUploadFolder());
					filePath += ACE_DIRECTORY_SEPARATOR_CHAR;
					filePath += m_orderedFileNames[i];

					DiffSortCriterion fileNameParser((m_orderedFileNames[i]).c_str(), m_volumename);
					azureDiffInfo.PrevEndTime = fileNameParser.PreviousEndTime();
					azureDiffInfo.PrevEndTimeSequenceNumber = fileNameParser.PreviousEndTimeSequenceNumber();
					azureDiffInfo.PrevContinuationId = fileNameParser.PreviousContinuationId();
					azureDiffInfo.EndTime = fileNameParser.EndTime();
					azureDiffInfo.EndTimeSequenceNumber = fileNameParser.EndTimeSequenceNumber();
					azureDiffInfo.ContinuationId = fileNameParser.ContinuationId();
					azureDiffInfo.FileSize = boost::filesystem::file_size(ExtendedLengthPath::name(filePath));
					azureDiffInfo.FilePath = ExtendedLengthPath::name(filePath);
					orderedAzureDiffInfos.push_back(azureDiffInfo);
				}


				m_AzureAgent->UploadLogs(orderedAzureDiffInfos, m_isSessionHavingRecoverablePoints);
				UpdateLastUploadTime();
				TrackFileUploadtoAzure(orderedAzureDiffInfos, m_isSessionHavingRecoverablePoints);
				done = true;
			}
		}
		catch (AzureInvalidOpException & ie) {

			// pause the replication pair
			// example, after a failover operation you cannot upload files

			azureinvalidopexception = true;
			exceptionMsg = ie.what();
			errorCode = ie.ErrorCode();
			DebugPrintf(SV_LOG_ERROR, "%s\n", ie.what());

		}
		catch (AzureCancelOpException & re)
		{
			azureresyncRequiredException = true;
			exceptionMsg = re.what();
			errorCode = re.ErrorCode();
			DebugPrintf(SV_LOG_ERROR, "%s\n", re.what());
		}
		catch (AzureClientResyncRequiredOpException & cre)
		{
			azureClientResyncRequiredException = true;
			exceptionMsg = cre.what();
			errorCode = cre.ErrorCode();
			DebugPrintf(SV_LOG_ERROR, "%s\n", cre.what());
		}
		catch (AzureMissingFileException & me)
		{
			azuremissingfileException = true;
			exceptionMsg = me.what();
			DebugPrintf(SV_LOG_ERROR, "%s\n", me.what());
		}
		catch (ErrorException & ee)
		{
			throw;
		}
		catch (std::exception & e)
		{
			throw;
		}
	}

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::TrackFileUploadtoAzure(const std::vector<AzureAgentInterface::AzureDiffLogInfo> & orderedFileInfos, bool isSessionHavingRecoverablePoints)
{
	if (!m_bLogUploadedFilesToAzure)
		return;
	std::ofstream AzureLogStream(m_azureTrackingUploadLog.string().c_str(), std::ios::app);

	if (!AzureLogStream)
	{
		std::stringstream msg;
		msg << "Failed to open file " << m_azureTrackingUploadLog.string().c_str() << " for volume "
			<< m_volumename << std::endl;
		DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
	}
	else
	{
		AzureLogStream << "Session Status: uploading . State:" << ((isSessionHavingRecoverablePoints) ? "crash consistent" : "Non crash consistent" ) << std::endl;
		for (size_t i = 0; i < orderedFileInfos.size(); ++i){
			AzureLogStream << (orderedFileInfos[i]).FileSize << " " << ExtendedLengthPath::nonExtName(orderedFileInfos[i].FilePath) << std::endl;
		}
	}

}

void CDPV2Writer::UpdateLastAppliedFileInformation()
{
	INM_TRACE_ENTER_FUNC

	if (!update_previous_diff_information(m_orderedFileNames.back(), SVD_VERSION(SVD_PERIOVERSION_MAJOR, SVD_PERIOVERSION_MINOR)))
	{
		throw INMAGE_EX("internal error, could not update last file applied information (will be retried)\n")(m_orderedFileNames.back());
	}

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::ChangePendingUploadStateToNonRecoverable()
{
	INM_TRACE_ENTER_FUNC

	if (!isSessionContainingRecoverabilePoints()){
		throw INMAGE_EX("internal error: invalid request - session is already in non-recoverable state,")(m_volumename);
	}

	if (!boost::filesystem::exists(PendingUploadFolderForRecoverablePoints())) {
		throw INMAGE_EX("internal error, invalid request - pending upload folder does not exist, cannot rename")(ExtendedLengthPath::nonExtName(PendingUploadFolderForRecoverablePoints().string()));
	}

	if (boost::filesystem::exists(PendingUploadFolderForNonRecoverablePoints())) {
		throw INMAGE_EX("internal error, folder already exists, cannot rename")(ExtendedLengthPath::nonExtName(PendingUploadFolderForNonRecoverablePoints().string()));
	}

	boost::filesystem::rename(PendingUploadFolderForRecoverablePoints(), PendingUploadFolderForNonRecoverablePoints());
	// 1st arg - recoverablepoints = false
	// 2nd arg - force = true
	SetSessionRecoverabilityState(false, true);

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::RenamePendingUploadFolderToUploaded()
{
	INM_TRACE_ENTER_FUNC

	if (boost::filesystem::exists(UploadedFolder())) {
		throw INMAGE_EX("internal error, we have files pending for upload as well as already uploaded")(m_volumename);
	}

	boost::filesystem::rename(PendingUploadFolder(), UploadedFolder());

	INM_TRACE_EXIT_FUNC
}

void CDPV2Writer::DeleteAzureFolder(extendedLengthPath_t folderPath)
{
	INM_TRACE_ENTER_FUNC

	// algorithm
	// get handle to uploaded folder
	// read the directory entries
	// delete the files

	if (boost::filesystem::exists(folderPath)) {

		extendedLengthPathDirIter_t uploadedFilesIter(folderPath);
		extendedLengthPathDirIter_t uploadedFilesIterEnd;

		for (/* empty */; uploadedFilesIter != uploadedFilesIterEnd; ++uploadedFilesIter) {
			if (boost::filesystem::is_regular_file(uploadedFilesIter->status())) {
				boost::filesystem::remove((*uploadedFilesIter).path().string());
			}
		}
		boost::filesystem::remove(folderPath);
	}

	INM_TRACE_EXIT_FUNC

}

void CDPV2Writer::CleanupCurrrentSession()
{
	m_orderedFileNames.clear();
	m_uploadSize = 0;
	m_isSessionHavingRecoverablePoints = false;
	m_azurePendingUploadFolder = m_azureCacheFolderForNonRecoverablePoints;
}


void CDPV2Writer::FindCurrentFileRecoverabilityState(const std::string & fileToProcess)
{
	INM_TRACE_ENTER_FUNC

	// Non recoverable in following conditions
	// 1. resync is required
	// 2. Resync Step 2
	// 3. Differential Sync + Resync Required flag 
	//            + (if set by source current differential end time > resync required flag timestamp ) 
	// 4. Metadata mode write ordering
	//


	if (m_resyncRequired || m_inquasi_state) {
		m_isCurrentFileContainsRecoverablePoints = false;

	}
	else {

		DiffSortCriterion fileNameParser(fileToProcess.c_str(), m_volumename);

		if ((m_Configurator->isResyncRequiredFlag(m_volumename)
			&& (!(m_Configurator->getResyncRequiredCause(m_volumename) == VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE)
			|| (fileNameParser.EndTime() > m_Configurator->getResyncRequiredTimestamp(m_volumename))
			))
			|| (std::string::npos != fileToProcess.find("_ME"))
			|| (std::string::npos != fileToProcess.find("_MC"))
			)
		{
			m_isCurrentFileContainsRecoverablePoints = false;

		}
		else {
			m_isCurrentFileContainsRecoverablePoints = true;
		}
	}

	INM_TRACE_EXIT_FUNC
}

std::string CDPV2Writer::MoveCurrentfileToPendingUpload(const std::string & fileToProcess)
{
	INM_TRACE_ENTER_FUNC

	extendedLengthPath_t fromPath(ExtendedLengthPath::name(fileToProcess));
	extendedLengthPath_t toPath(PendingUploadFolder());
	toPath /= fromPath.filename();

	boost::filesystem::rename(fromPath, toPath);

	INM_TRACE_EXIT_FUNC

	return ExtendedLengthPath::nonExtName(toPath.string());
}

void CDPV2Writer::addFileToCurrentSession(const std::string & filePath)
{
	INM_TRACE_ENTER_FUNC

	// algorithm
	//   add the file name to current session
	//   add up the file size to pending upload size


	// we are only storing filename and not complete path
	// this will help for scenario where we just rename the pendingupload folder from 
	// recoverable to non-recoverable
	// eg. say protection service returns resync required, we will rename pendinguploadfolder to non-recoverable
	// but do not have to change the in-memory vector as we are only passing the filenames

	boost::filesystem::path uploadPath(filePath);

	m_orderedFileNames.push_back(uploadPath.filename().string());
	m_uploadSize += boost::filesystem::file_size(ExtendedLengthPath::name(filePath));

	INM_TRACE_EXIT_FUNC
}

bool CDPV2Writer::isInResyncStep2()
{
	return  m_inquasi_state;
}

bool CDPV2Writer::ReadyToTransitionToDiffSync(const std::string & fileToProcess)
{
	INM_TRACE_ENTER_FUNC

	// we should not be calling this routine unless we are in resync step 2
	if (!isInResyncStep2()) {
		throw INMAGE_EX("Already in differential sync")(m_volumename);
	}

	DiffSortCriterion fileNameParser(fileToProcess.c_str(), m_volumename);

	SV_ULONGLONG diffStartTime = fileNameParser.PreviousEndTime();
	SV_ULONGLONG  diffStartTimeSeq = fileNameParser.PreviousEndTimeSequenceNumber();

	SV_ULONGLONG  quasiEndTime = m_Configurator->getResyncEndTimeStamp(m_volumename);
	SV_ULONG      quasiEndTimeSeq = m_Configurator->getResyncEndTimeStampSeq(m_volumename);

	INM_TRACE_EXIT_FUNC

	return ((diffStartTime > quasiEndTime) ||
		((diffStartTime == quasiEndTime) && (diffStartTimeSeq > quasiEndTimeSeq)));
}

void CDPV2Writer::TransitionToDiffSync()
{
	INM_TRACE_ENTER_FUNC

	if (!EndQuasiState())
		throw INMAGE_EX("Transition to differential sync failed")(m_volumename);

	INM_TRACE_EXIT_FUNC
}


bool CDPV2Writer::ApplyResyncFiletoAzureBlob(const std::string & fileToApply)
{
	INM_TRACE_ENTER_FUNC

	bool rv = true;

	try{
		
		if (!m_AzureAgent){
			throw INMAGE_EX("internal error: resync azure agent is not initialized")(m_volumename);
		}

        SV_ULONGLONG fsize = boost::filesystem::file_size(fileToApply);
        m_pSyncApply->start();
		m_AzureAgent->ApplyLog(fileToApply);
        m_pSyncApply->stop(fsize);
	}
	catch (AzureCancelOpException & ce) {

		//  cancel the current resync and restart fresh session
		//  this is achieved by just sending resync required flag to cs

		rv = false;
        const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
        std::stringstream msg("encountered AzureCancelOpException: ");
        msg << ce.what() << ". Marking resync for the target device " << m_volumename << " with resyncReasonCode=" << resyncReasonCode;
        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
        CDPUtil::store_and_sendcs_resync_required(m_volumename, msg.str(), resyncReasonStamp);

		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered AzureCancelOpException %s.\n",
			FUNCTION_NAME, ce.what());

		CDPUtil::QuitRequested(600);
	}
	catch (AzureInvalidOpException & ie) {

		// pause the replication pair
		// example, after a failover operation you cannot upload files, start resync etc operations

		rv = false;
		ReplicationPairOperations pairOp;
		stringstream replicationPauseMsg;
		replicationPauseMsg << "Replication for " << m_volumename
			<< "is paused due to unrecoverable error("
			<< std::hex << ie.ErrorCode()
			<< ").";

		if (pairOp.PauseReplication(TheConfigurator, m_volumename, INMAGE_HOSTTYPE_TARGET, replicationPauseMsg.str().c_str(), ie.ErrorCode())) {
			DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, replicationPauseMsg.str().c_str());
		}
		else {
			DebugPrintf(SV_LOG_ERROR, "%s: Request '%s' to CS failed", FUNCTION_NAME, replicationPauseMsg.str().c_str());
		}

		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered AzureInvalidOpException %s.\n",
			FUNCTION_NAME, ie.what());

		CDPUtil::QuitRequested(600);
	}
	catch (AzureBadFileException & be)
	{
		// re-throw in case of bad resync file
		//
		DebugPrintf(SV_LOG_ERROR, "%s encountered AzureBadFileException - Corrupt Resync File : %s\n", FUNCTION_NAME, be.what());
        throw be;
	}
	catch (ErrorException & ee)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "%s encountered an ErrorException %s\n", FUNCTION_NAME, ee.what());
	}
	catch (std::exception & e)
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "%s encountered an exception %s\n", FUNCTION_NAME, e.what());
	}
    catch (...)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, "%s encountered an unknown exception\n", FUNCTION_NAME);
    }
	
	INM_TRACE_EXIT_FUNC

	return rv;
}

//===========================================================
// changes for enabling disk based replication for windows
// ==========================================================

void CDPV2Writer::GetDeviceType()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	cdp_volume_t::CDP_VOLUME_TYPE devType;
	if (isAzureTarget()){
		devType = cdp_volume_t::CDP_DUMMY_VOLUME_TYPE;
	}
	else {
		devType = ((VolumeSummary::DISK == m_Configurator->GetDeviceType(m_volumename)) ? cdp_volume_t::CDP_DISK_TYPE : cdp_volume_t::CDP_REAL_VOLUME_TYPE);
	}

	cdp_volume_t::CDP_VOLUME_TYPE effectivetype = (cdp_volume_t::CDP_DISK_TYPE == devType) ? cdp_volume_t::GetCdpVolumeTypeForDisk() : devType;
	m_deviceType = effectivetype;

	DebugPrintf(SV_LOG_DEBUG, "Device %s type = %s\n",
		m_volumename.c_str(), cdp_volume_t::GetStrVolumeType(effectivetype).c_str());


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool CDPV2Writer::GetDeviceProperties()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	GetDeviceType();

	if (cdp_volume_t::CDP_DISK_TYPE != DeviceType()) {
		m_displayName = m_volumename;
		m_nameBasedOn = VolumeSummary::SIGNATURE;
		return true;
	}

	if (!m_pVolumeSummariesCache){
		DebugPrintf(SV_LOG_ERROR, "Volume infocollector information not specified to open disk %s.\n", m_volumename.c_str());
		return false;
	}

	//Get volume report from vic cache
	VolumeReporter::VolumeReport_t vr;
	VolumeReporterImp rptr;
	vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
	rptr.GetVolumeReport(m_volumename, *m_pVolumeSummariesCache, vr);
	rptr.PrintVolumeReport(vr);

	//Verify the disk is reported
	if (!vr.m_bIsReported) {
		DebugPrintf(SV_LOG_ERROR, "Failed to find disk %s in vic cache.\n", m_volumename.c_str());
		return false;
	}


	DebugPrintf(SV_LOG_DEBUG, "disk %s is present in vic cache.\n", m_volumename.c_str());


	if (vr.m_DeviceName.empty()){
		DebugPrintf(SV_LOG_ERROR, "Failed to find display name for disk %s in vic cache.\n", m_volumename.c_str());
		return false;
	}

	m_displayName = vr.m_DeviceName;
	m_nameBasedOn = vr.m_nameBasedOn;


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return true;
}

bool CDPV2Writer::GetdiskHandle()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	int openMode = O_RDWR;
	if (m_useAsyncIOs)	{
		openMode |= FILE_FLAG_OVERLAPPED;
	}

	if (m_useUnBufferedIo) {
		setdirectmode(openMode);
	}

	m_volhandle = ACE_OS::open(getLongPathName(m_displayName.c_str()).c_str(),
		openMode,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);

	if (ACE_INVALID_HANDLE == m_volhandle) {
		
		DebugPrintf(SV_LOG_ERROR,
			"Failed to open disk device name %s for disk %s.\n", 
			m_displayName.c_str(), 
			m_volumename.c_str());

		return false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Opened disk device name %s.\n", m_displayName.c_str());

#ifdef SV_WINDOWS

	std::string id;
	if (VolumeSummary::SCSIID == m_nameBasedOn){
		id = GetDiskScsiId(m_displayName);
	}
	else{
		id = GetDiskGuid(m_volhandle);
	}

	DebugPrintf(SV_LOG_DEBUG, "Got id %s from above disk device name.\n", id.c_str());

	if (id != m_volumename) {

		DebugPrintf(SV_LOG_ERROR, 
			"From disk device name %s, id got %s does not match required id %s.\n",
			m_displayName.c_str(), id.c_str(), 
			m_volumename.c_str());
		close_volume_handle();
		return false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Verified that disk device name %s gives correct id %s.\n", m_displayName.c_str(), id.c_str());
#endif

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return true;
}


/*
* FUNCTION NAME : CDPV2Writer::HaveNewFilesArrivedInCacheFolder
*
* DESCRIPTION : This function lists the diffs in the cache dir and finds if there are any
*               diff available
*
* INPUT PARAMETERS : cacheLocation - path to the diff file being applied

*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
* return value : true if any file is found in cache dir, false otherwise
*
*/
bool CDPV2Writer::HaveNewFilesArrivedInCacheFolder(const std::string& cacheLocation) const
{
	bool rv = false;
	std::vector<std::string> filelist;
	unsigned int limit = 1;

	if (CDPUtil::get_filenames_withpattern(cacheLocation, m_Configurator->getVxAgent().getDiffTargetFilenamePrefix(), filelist, limit)) {
		rv = !filelist.empty();
	}
	DebugPrintf(SV_LOG_DEBUG, "%s %s returning %s", FUNCTION_NAME, cacheLocation.c_str(), (rv) ? "true" : "false");
	return rv;
}
