//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dpdrivercomm.cpp
//
// Description: 
//

#include <cstring>
#include <string>
#include <wchar.h>
#include <windows.h>
#include <winioctl.h>
#include "hostagenthelpers.h"

#include <sstream>

#include "dpdrivercomm.h"
#include "dataprotectionexception.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include <ace/OS_NS_unistd.h>

#include "devicefilter.h"
#include "error.h"

#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#include "involfltfeatures.h"
#include "indskfltfeatures.h"

#include "filterdrvifmajor.h"

DpDriverComm::DpDriverComm(std::string const & deviceName, const int &deviceType) 
: m_DeviceName(deviceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::stringstream msg;
    FilterDrvIfMajor fdif;

    // Create device stream
    try {
        m_pDeviceStream.reset(new DeviceStream(fdif.GetDriverName(deviceType)));
        DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
    }
    catch (std::bad_alloc &e) {
        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Memory allocation failed for creating driver stream object with exception " << e.what();
        throw DataProtectionException(msg.str());
    }

    // Open device stream
    if (SV_SUCCESS == m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
        DebugPrintf(SV_LOG_DEBUG, "Opened driver %s.\n", m_pDeviceStream->GetName().c_str());
    else {
        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to open driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }

    // Get source device input name to use in all ioctls
    m_DeviceNameForDriverInput = fdif.GetFormattedDeviceNameForDriverInput(deviceName, deviceType);
    DebugPrintf(SV_LOG_DEBUG, "Source name for driver input is %S\n", std::wstring(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end()).c_str());

    // Get version
    DRIVER_VERSION dv;
    int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_GET_VERSION, NULL, 0, &dv, sizeof dv);
    if (iStatus != SV_SUCCESS)
    {
        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed find version of driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n MinorVersion3:%d\n",
            dv.ulDrMajorVersion, dv.ulDrMinorVersion,
            dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
        // Instantiate driver features
        try {
            //Get features
            DeviceFilterFeatures *pf;
            if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pDeviceStream->GetName())
                pf = new InDskFltFeatures(dv);
            else
                pf = new InVolFltFeatures(dv);
            m_pDeviceFilterFeatures.reset(pf);
            DebugPrintf(SV_LOG_DEBUG, "Created device filter features object.\n");
        }
        catch (std::bad_alloc &e) {
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Memory allocation failed for filter driver features object with exception " << e.what();
            throw DataProtectionException(msg.str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

DpDriverComm::DpDriverComm(std::string const & deviceName, const int &deviceType, std::string const & persistentName) 
:DpDriverComm(deviceName, deviceType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    m_PersistentName = persistentName;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DpDriverComm::Init(const int &deviceType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

DpDriverComm::~DpDriverComm()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DpDriverComm::StartFiltering(const VOLUME_SETTINGS &pVolumeSettings, const VolumeSummaries_t *pvs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    void *pInput;
    long inputLen;
    START_FILTERING_INPUT sfi;

    std::stringstream msg;
    bool issueIoctl;
    //Fill input to start filtering ioctl
    if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pDeviceStream->GetName()) {
        //For disk driver, input is START_FILTERING_INPUT
        memset(&sfi, 0, sizeof sfi);
        FilterDrvIfMajor drvifmajor;
        issueIoctl = drvifmajor.FillStartFilteringInput(m_DeviceNameForDriverInput, m_pDeviceFilterFeatures.get(), pvs, pVolumeSettings, sfi);
        if (issueIoctl) {
            drvifmajor.PrintStartFilteringInput(sfi);
            pInput = &sfi;
            inputLen = sizeof sfi;
        }
        else {
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: unable to fill start filtering input for device " << m_DeviceName
                << " as it is not reported.";
            throw DataProtectionException(msg.str());
        }
    }
    else {
        //For volume driver, it is direct volume name
        pInput = const_cast<wchar_t *>(m_DeviceNameForDriverInput.c_str());
        inputLen = sizeof(wchar_t) * m_DeviceNameForDriverInput.length();
        issueIoctl = true;
    }

    //Issue ioctl
    int iStatus = SV_FAILURE;
    if (issueIoctl) {
        iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_START_FILTERING_DEVICE, pInput, inputLen, NULL, 0);
        if (iStatus != SV_SUCCESS) {
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue start filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
                << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
            throw DataProtectionException(msg.str());
        }
    }

    // only log this locally as there are too many to send to cx
    DebugPrintf(SV_LOG_ALWAYS, "StartFiltering for volume %s\n", m_DeviceName.c_str());
}

void DpDriverComm::StartFiltering(const VolumeReporter::VolumeReport_t &pVolumeReport, const VolumeSummaries_t *pvs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    void *pInput;
    long inputLen;
    START_FILTERING_INPUT sfi;
    std::stringstream msg;
    bool issueIoctl;
    if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pDeviceStream->GetName()) {
        memset(&sfi, 0, sizeof sfi);
        FilterDrvIfMajor drvifmajor;
        issueIoctl = drvifmajor.FillStartFilteringInput(m_DeviceNameForDriverInput, pVolumeReport, sfi);
        if (issueIoctl) {
            drvifmajor.PrintStartFilteringInput(sfi);
            pInput = &sfi;
            inputLen = sizeof sfi;
        }
        else {
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: unable to fill start filtering input for device " << m_DeviceName
                << " as it is not reported.";
            throw DataProtectionException(msg.str());
        }
    }
    else {
        pInput = const_cast<wchar_t *>(m_DeviceNameForDriverInput.c_str());
        inputLen = sizeof(wchar_t) * m_DeviceNameForDriverInput.length();
        issueIoctl = true;
    }
    int iStatus = SV_FAILURE;
    if (issueIoctl) {
        iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_START_FILTERING_DEVICE, pInput, inputLen, NULL, 0);
        if (iStatus != SV_SUCCESS) {
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue start filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
                << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
            throw DataProtectionException(msg.str());
        }
    }
    DebugPrintf(SV_LOG_ALWAYS, "StartFiltering for volume %s\n", m_DeviceName.c_str());
}
void DpDriverComm::StopFiltering()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    STOP_FILTERING_INPUT sfi;
    std::stringstream msg;
    memset(&sfi, 0, sizeof sfi);
    std::wstring device(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end());
    inm_wmemcpy_s(sfi.VolumeGUID, NELEMS(sfi.VolumeGUID), device.c_str(), device.length());
    sfi.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;
    int iStatus = SV_FAILURE;
    iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, &sfi, sizeof(sfi), NULL, 0);
    if (iStatus != SV_SUCCESS) {
        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue stop filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
            << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    DebugPrintf(SV_LOG_ALWAYS, "StopFiltering for volume %s\n", m_DeviceName.c_str());
}

void DpDriverComm::ClearDifferentials()
{
    std::stringstream msg;

    int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_CLEAR_DIFFERENTIALS, const_cast<wchar_t *>(m_DeviceNameForDriverInput.c_str()), sizeof(wchar_t) * m_DeviceNameForDriverInput.length(), NULL, 0);
    if (SV_SUCCESS != iStatus)
    {
        SV_ULONG ulErrorCode = m_pDeviceStream->GetErrorCode();


        if (ERROR_FILE_OFFLINE == ulErrorCode) {
            DebugPrintf(SV_LOG_ALWAYS, "Disk %s is offline. Skipping clear differentials for disk.\n", m_DeviceName.c_str());
            return;
        }

        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue clear differentials for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
            << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }

    // FIX_LOG_LEVEL: note for now we are going to use SV_LOG_ERROR even on success so that we can
    // track this at the cx, eventually a new  SV_LOG_LEVEL will be added that is not an error 
    // but still allows this to be sent to the cx
    DebugPrintf(SV_LOG_ALWAYS, "ClearDifferentials for volume %s\n", m_DeviceName.c_str());
}

bool DpDriverComm::SetClusterFiltering()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::stringstream msg;
    SET_DISK_CLUSTERED_INPUT	setClusterFiltering = { 0 };

    inm_wmemcpy_s(setClusterFiltering.DeviceId, NELEMS(setClusterFiltering.DeviceId), m_DeviceNameForDriverInput.c_str(), m_DeviceNameForDriverInput.length());
    int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_SET_DISK_CLUSTERED, &setClusterFiltering,sizeof(setClusterFiltering), NULL, 0);
    if (SV_SUCCESS != iStatus)
    {
        msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue set cluster filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
            << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    DebugPrintf(SV_LOG_ALWAYS, "SetClusterFiltering for volume %s\n", m_DeviceName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool DpDriverComm::ResyncStartNotify(ResyncTimeSettings& rts)
{
    SV_ULONG	ulErrorCode = 0;
    return ResyncStartNotify(rts, ulErrorCode);
}

bool DpDriverComm::ResyncStartNotify(ResyncTimeSettings & rts, SV_ULONG& ulErrorCode)
{
    bool rv = false;
    int iStatus;
    RESYNC_START_INPUT  Input = {0};

    do {
        //
        // Algorithm:
        //
        // Get driver version
        //  If driver version ioctl suceeds,
        //     if version is >= perio_version
        //         pass RESYNC_START_OUTPUT_V2 as parameter to IOCTL_INMAGE_RESYNC_START_NOTIFICATION ioctl
        //     else
        //       pass RESYNC_START_OUTPUT as parameter to IOCTL_INMAGE_RESYNC_START_NOTIFICATION ioctl
        //     use the ioctl for obtaining resync start time and sequence no
        //  else
        //      get resync time from system
        //  caller will send the resync time to cx
        //
        inm_wmemcpy_s(Input.VolumeGUID, NELEMS(Input.VolumeGUID), m_DeviceNameForDriverInput.c_str(), m_DeviceNameForDriverInput.length());

        if (!m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            RESYNC_START_OUTPUT Output;
            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_START_NOTIFICATION, &Input, sizeof(Input), &Output, sizeof(Output));
            rts.time = Output.TimeInHundNanoSecondsFromJan1601;
            rts.seqNo = Output.ulSequenceNumber;
        }
        else
        {
            RESYNC_START_OUTPUT_V2 Output;
            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_START_NOTIFICATION, &Input, sizeof(Input), &Output, sizeof(Output));
            rts.time = Output.TimeInHundNanoSecondsFromJan1601;
            rts.seqNo = Output.ullSequenceNumber;
        }

        if (SV_SUCCESS == iStatus)
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "Returned Success from resync start notification for volume %s. TimeStamp [" ULLSPEC"] Sequence [" ULLSPEC"] \n",
                m_DeviceName.c_str(), rts.time, rts.seqNo);
            rv = true;
        }
        else
        {
            std::stringstream msg;
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue resync start notify for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
                << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
            ulErrorCode = m_pDeviceStream->GetErrorCode();
            break;
        }
    } while (0);

    return rv;
}

bool DpDriverComm::ResyncEndNotify(ResyncTimeSettings& rts)
{
    SV_ULONG	ulErrorCode = 0;
    return ResyncEndNotify(rts, ulErrorCode);
}

bool DpDriverComm::ResyncEndNotify(ResyncTimeSettings & rts, SV_ULONG&   ulErrorCode)
{
    bool rv = false;
    int iStatus;
    RESYNC_END_INPUT  Input = {0};

    do {

        //
        // Algorithm:
        //  
        // Get driver version
        //  If driver version ioctl suceeds, 
        //    if driver version >= perio_version
        //       pass RESYNC_END_OUTPUT_V2 to IOCTL_INMAGE_RESYNC_END_NOTIFICATION ioctl
        //    else
        //       pass RESYNC_END_OUTPUT to IOCTL_INMAGE_RESYNC_END_NOTIFICATION ioctl
        //       use the ioctl for obtaining resync start time
        //  else
        //      get resync time from system
        // caller will now send resync time to cx
        //
        inm_wmemcpy_s(Input.VolumeGUID, NELEMS(Input.VolumeGUID), m_DeviceNameForDriverInput.c_str(), m_DeviceNameForDriverInput.length());

        if (!m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            RESYNC_END_OUTPUT Output;
            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_END_NOTIFICATION, &Input, sizeof(Input), &Output, sizeof(Output));
            rts.time = Output.TimeInHundNanoSecondsFromJan1601;
            rts.seqNo = Output.ulSequenceNumber;
        }
        else
        {
            RESYNC_END_OUTPUT_V2 Output;
            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_END_NOTIFICATION, &Input, sizeof(Input), &Output, sizeof(Output));
            rts.time = Output.TimeInHundNanoSecondsFromJan1601;
            rts.seqNo = Output.ullSequenceNumber;
        }

        if (SV_SUCCESS == iStatus)
        {
            DebugPrintf(SV_LOG_ALWAYS, "Returned Success from resync end notification for volume %s. TimeStamp " ULLSPEC " Sequence [" ULLSPEC"] \n", m_DeviceName.c_str(), rts.time, rts.seqNo);
            rv = true;
        }
        else
        {
            std::stringstream msg;
            msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue resync end notify for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
                << " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
            ulErrorCode = m_pDeviceStream->GetErrorCode();
            break;
        }

    } while (0);

    return rv;
}


int DpDriverComm::GetFilesystemClusters(const std::string &filename, FileSystem::ClusterRanges_t &clusterranges)
{
    //Do not call lcn ioctl for disk filter
    if (m_pDeviceStream->GetName() == INMAGE_DISK_FILTER_DOS_DEVICE_NAME)
        return SV_SUCCESS;

    GET_LCN l;
    /* this equals 16kb allocation 
     * as each extent has 16 bytes 
     * storage */
    const int NEXTENTSFORIOCTL = 1024;
    RETRIEVAL_POINTERS_BUFFER samplebr;
    size_t brsz;
    INM_SAFE_ARITHMETIC(brsz = sizeof(RETRIEVAL_POINTERS_BUFFER)+((NEXTENTSFORIOCTL-1) * InmSafeInt<size_t>::Type(sizeof(samplebr.Extents[0]))), INMAGE_EX(sizeof(RETRIEVAL_POINTERS_BUFFER))(sizeof(samplebr.Extents[0])))
    RETRIEVAL_POINTERS_BUFFER *br;
    int rval = SV_SUCCESS;
    const std::string GLOBAL_PREFIX = "\\\?\?\\";
    std::string globalfilename = GLOBAL_PREFIX+filename;

    /* since this is wide char, a terminating null has to 
     * be wide char long */
    if (globalfilename.length() > (FILE_NAME_SIZE_LCN-sizeof(wchar_t)))
    {
        rval = ERR_INVALID_PARAMETER;
        return rval;
    }

    br = (RETRIEVAL_POINTERS_BUFFER *)calloc(brsz, 1);
    if (br)
    {
        std::wstring ws(globalfilename.begin(), globalfilename.end());
        bool bexit = false;
        DWORD bytesreturned = 0;
        SV_ULONGLONG start, count;
        int iStatus;
        bool bfill;
    
        l.usFileNameLength = ws.length()*sizeof(wchar_t);
        memmove(l.FileName, ws.c_str(), l.usFileNameLength);
        l.StartingVcn.QuadPart = 0;
        do
        {
            DebugPrintf(SV_LOG_DEBUG, "For file %s, GET LCN INPUT:\n", filename.c_str());
            DebugPrintf(SV_LOG_DEBUG, "FileName: %S\n", l.FileName);
            DebugPrintf(SV_LOG_DEBUG, "usFileNameLength: %hu\n", l.usFileNameLength);
            std::stringstream ss;
            ss << l.StartingVcn.QuadPart;
            DebugPrintf(SV_LOG_DEBUG, "StartingVcn: %s\n", ss.str().c_str());
            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_GET_LCN, &l, sizeof l, br, brsz);
            bfill = false;
    
            if (SV_SUCCESS == iStatus)
            {
                bfill = true;
                bexit = true;
            }
            else
            {
                DWORD err = Error::UserToOS(m_pDeviceStream->GetErrorCode());
                if (ERROR_MORE_DATA == err)
                {
                    bfill = true;
                }
                else
                {
                    bexit = true;
                    rval = m_pDeviceStream->GetErrorCode();
                }
            }
    
            if (bfill)
            {
                for (DWORD i = 0; i < br->ExtentCount; i++)
                {
                    start = br->Extents[i].Lcn.QuadPart;
                    count = ((i>0) ? (br->Extents[i].NextVcn.QuadPart-br->Extents[i-1].NextVcn.QuadPart) : 
                                     (br->Extents[i].NextVcn.QuadPart-br->StartingVcn.QuadPart));
                    clusterranges.push_back(FileSystem::ClusterRange_t(start, count));
                }
                l.StartingVcn.QuadPart = br->Extents[br->ExtentCount-1].NextVcn.QuadPart;
            }

        } while (!bexit);

        free(br);
    }
    else
    {
        rval = ERR_AGAIN;
    }

    return rval;
}
