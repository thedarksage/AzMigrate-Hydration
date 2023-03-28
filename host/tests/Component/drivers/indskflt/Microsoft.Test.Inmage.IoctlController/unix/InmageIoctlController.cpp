//Microsoft.Test.Inmage.IoctlController.cpp: Defines the exported functions for the DLL application.
//
#include <iostream>
#include <string.h>
#include "involflt.h"
#include <sys/ioctl.h>
#include "InmageIoctlController.h"
#include "InmageTestException.h"
#include <vector>
typedef long LONG_PTR, *PLONG_PTR;
#define IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST _IO(FLT_IOCTL, GET_PROTECTED_VOLUME_LIST_CMD)

#define INVALID_HANDLE_VALUE -1
#define PROT_VOL_LIST 3
#define STREAM_REC_TYPE_TAGGUID_TAG 0x02FF
#define INM_PAGESZ          (0x1000)


// Private constructor for CInmageIoctlController
CInmageIoctlController::CInmageIoctlController(IPlatformAPIs *platformApis, ILogger *pLogger) :
m_platformApis(platformApis),
m_pLogger(pLogger)
{

    const char* device = INMAGE_FILTER_DEVICE_NAME;
    std::string deviceName = device;
    s_hInmageCtrlDevice = m_platformApis->OpenDefault(deviceName);

    if (INVALID_HANDLE_VALUE == s_hInmageCtrlDevice)
    {
        throw CAgentException("Failed to open InMage control device err=0x%x",
            m_platformApis->GetLastError());

    }
}


CInmageIoctlController* CInmageIoctlController::GetInstance(
    IPlatformAPIs *platformApis, ILogger *pLogger)
{
    if (NULL == s_instance)
    {
        s_InstaceLock.lock();
        if (NULL == s_instance)
        {
            s_instance = new CInmageIoctlController(platformApis, pLogger);
        }
        s_InstaceLock.unlock();
    }

    return s_instance;
}

void CInmageIoctlController::ServiceShutdownNotify(unsigned long ulFlags)
{

    SHUTDOWN_NOTIFY_INPUT inputData = { 0 };
    m_shutdownNotify.reset(new Event(true,false,std::string()));

    inputData.ulFlags = ulFlags;
    inputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
        &inputData,sizeof(inputData),NULL,0,NULL))
    {
        throw CAgentException("\n%s failed with error=0x%x\n",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::GetDriverVersion()
{
    DRIVER_VERSION DriverVersion = { 0 };

    if (!m_platformApis->DeviceIoControlSync(s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_GET_DRIVER_VERSION,
        &DriverVersion,NULL,NULL,NULL,NULL))
    {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::StartFiltering(std::string deviceId,
    unsigned long long nBlocks, unsigned int blockSize)
{
    //Create structure
    inm_dev_info_t startFilteringInput;

    //Initialize the structure
    memset((void*)&startFilteringInput, 0x00, sizeof(inm_dev_info_t));

    //Set the inm_dev_t d_type member
    startFilteringInput.d_type = FILTER_DEV_HOST_VOLUME;

    //Set the d_guid member of inm_dev_info_t structure
    if (strcpy_s(&startFilteringInput.d_guid[0], GUID_SIZE_IN_CHARS, deviceId.c_str()))
    {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }

    std::string pname = "";
    for(int i = 0; i < deviceId.size(); i++) {
        if (deviceId[i] == '/') {
            pname += '_';
        }
        else {
            pname += deviceId[i];
        }
    }
    //boost::replace_all(pname, "/", "_");
    if (strcpy_s(&startFilteringInput.d_pname[0], GUID_SIZE_IN_CHARS, pname.c_str()))
    {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }

    //Set the number of blocks
    startFilteringInput.d_nblks = nBlocks;
    //MCHECK:Check why we need to right shift 9 times.
    //Probably dividing disk size by 512 which is block size?

    //Set the block size
    startFilteringInput.d_bsize = blockSize;

    //Set the flags member
    //unsigned long long FULL_DISK_FLAG = (unsigned long long)0x0000000000000002;
    startFilteringInput.d_flags = (unsigned long long)0x0000000000000002;;// FULL_DISK_FLAG;

    cout << "Pname is " << pname << endl;
    if (!m_platformApis->DeviceIoControlSync(s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_START_FILTERING_DEVICE_V2,
        &startFilteringInput,
        sizeof(inm_dev_info_t), NULL, NULL, NULL))
    {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::TrimNewLineChars(
    const std::string &strDevs, std::vector<std::string>& vDevices)
{
    string strDev;
    std::string::size_type nPos = 0, nCurPos = 0;
    while (nCurPos < strDevs.length())
    {
        nPos = strDevs.find('\n', nCurPos);
        if (nPos == std::string::npos)
        {
            break;
        }
        strDev = strDevs.substr(nCurPos, nPos - nCurPos);
        if (!strDev.empty())
        {
            vDevices.push_back(strDev);
        }
        nCurPos = nPos + 1;
    }
}

// When StopFiltering ioctl is specified with empty device list
//then, it should query the list of protected devices and then stop
//filtering on each of them.
int CInmageIoctlController::StopFilteringOnAllProtectedDevices()
{
    int ret = 0;
    std::vector<std::string> vstrProtectedDevList;
    GET_VOLUME_LIST vol_list;
    vol_list.buf_len = INITIAL_BUFSZ_FOR_VOL_LIST;
    do
    {
        vol_list.bufp = (char*)(malloc(vol_list.buf_len));
        if (!vol_list.bufp)
        {
            ret = ENOMEM;
            break;
        }
        if (!(ret = m_platformApis->DeviceIoControlSync(s_hInmageCtrlDevice,
            IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST,
            (void*)&vol_list, 0, 0, 0, 0)))
        {
            if( errno == EAGAIN)
            {
                free(vol_list.bufp);
                continue;
            }
            ret = -1;
            free(vol_list.bufp);
            break;
        }
        else
        {
            if (strlen(vol_list.bufp) > 0)
            {
                TrimNewLineChars(std::string(vol_list.bufp), vstrProtectedDevList);
            }
            free(vol_list.bufp);
            ret = 0;
            break;
        }
    } while(ret);

    //Now for every protected device, perform StopFiltering
    std::vector<std::string>::iterator iter = vstrProtectedDevList.begin();
    while(iter != vstrProtectedDevList.end() && (vstrProtectedDevList.size() > 0))
    {
        try
        {
            IssueStopFilteringIoctl((*iter), true, false);
            iter++;
        } catch(CAgentException& ex)
        {
            std::cout<<endl<<ex.what()<<endl;
            return -1;
        }
    }
    return 0;
}

void CInmageIoctlController::IssueStopFilteringIoctl(
    std::string deviceId, bool delBitmapFile, bool bStopAll)
{
    m_pLogger->LogInfo("%s : Issuing stop filtering for device : %s",
        __FUNCTION__, deviceId.c_str());

    VOLUME_GUID stopFilteringInput = { 0 };
    strncpy(stopFilteringInput.volume_guid, deviceId.c_str(), deviceId.size());
    delBitmapFile = true;
    bStopAll = false;

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_STOP_FILTERING_DEVICE,
        &stopFilteringInput, NULL, NULL, NULL, NULL))
    {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }

}

int CInmageIoctlController::StopFiltering(std::string deviceId,bool delBitmapFile, bool bStopAll)
{
    if (deviceId.size() == 0)
    {
        return StopFilteringOnAllProtectedDevices();
    }
    else
    {
        IssueStopFilteringIoctl(deviceId, delBitmapFile, bStopAll);
        return 0;
    }
}

void CInmageIoctlController::GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans,
    PUDIRTY_BLOCK_V2 pDirtyBlock, bool bThowIfResync, std::string deviceId)
{
    strncpy((char *)(pCommitTrans->VolumeGUID), deviceId.c_str(), deviceId.length());
    memset(((void*)pDirtyBlock), 0, sizeof(*pDirtyBlock));
    bThowIfResync = false; 

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
        pDirtyBlock, NULL, NULL, NULL, NULL)) {
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
    {
            cout<<"Got resync required flag\n";
        //throw CAgentResyncRequiredException(pDirtyBlock);
    }
}

int CInmageIoctlController::WaitDB(std::string deviceId)
{
    int err = 0;
    WAIT_FOR_DB_NOTIFY waitDB = {0};

    strncpy((char *)waitDB.VolumeGUID, deviceId.c_str(), deviceId.length());
    waitDB.Seconds = 65;

tryagain:
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_WAIT_FOR_DB_V2,
        &waitDB, NULL, NULL, NULL, NULL))
    {
        err = m_platformApis->GetLastError();
    }

    return err;

}

void CInmageIoctlController::CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction)
{
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
        pCommitTransaction, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
    else
    {
        printf("DB committed");
    }
}

void CInmageIoctlController::ResyncStartNotification(std::string deviceId)
{
    RESYNC_START_V2 resyncStartInput = {0};
    strncpy ((char*)resyncStartInput.VolumeGUID, deviceId.c_str(), deviceId.size());
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_RESYNC_START_NOTIFICATION,
        &resyncStartInput, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::ResyncEndNotification(std::string deviceId)
{
    RESYNC_END_V2 resyncEndInput = {0};
    strncpy ((char*)resyncEndInput.VolumeGUID, deviceId.c_str(), deviceId.size());
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_RESYNC_END_NOTIFICATION,
        &resyncEndInput, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::ClearDifferentials(std::string deviceId)
{
    VOLUME_GUID clearDiffInput = {0};
    strncpy (clearDiffInput.volume_guid, deviceId.c_str(), deviceId.size());
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
        &clearDiffInput, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::HoldWrites(
    std::string tagContext, unsigned long ulFlags, unsigned long ulTimeoutInMS)
{
    flt_barrier_create_t holdWritesIn;
    memset(&holdWritesIn, 0, sizeof(holdWritesIn));

    holdWritesIn.fbc_flags = ulFlags;
    holdWritesIn.fbc_timeout_ms = ulTimeoutInMS;
    strncpy(holdWritesIn.fbc_guid, tagContext.c_str(), tagContext.size());
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_CREATE_BARRIER_ALL,
        &holdWritesIn, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::ReleaseWrites(std::string tagContext, unsigned long ulFlags)
{
    flt_barrier_remove_t removeWritesIn;
    memset(&removeWritesIn, 0, sizeof(removeWritesIn));

    removeWritesIn.fbr_flags = ulFlags;
    strncpy(removeWritesIn.fbr_guid, tagContext.c_str(), tagContext.size());
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_REMOVE_BARRIER_ALL,
        &removeWritesIn, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}

void CInmageIoctlController::CommitTag(std::string tagContext, unsigned long ulFlags)
{
    flt_tag_commit_t commitTagIn;
    memset(&commitTagIn, 0, sizeof(commitTagIn));

    commitTagIn.ftc_flags = TAG_COMMIT;
    strncpy(commitTagIn.ftc_guid, tagContext.c_str(), tagContext.size());
    if (! m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_TAG_COMMIT_V2,
        &commitTagIn, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}
void CInmageIoctlController::InsertDistTag(std::string tagContext,
    unsigned long uiFlags, list<string> taglist)
{
    bool status = true;
    char* pBuffer = GetTagBuffer(tagContext,taglist, uiFlags, 0);

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_TAG_VOLUME_V2,
        pBuffer, NULL, NULL, NULL, NULL))
    {
        printf("errno = %d\n", errno);
        status = false;
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
}
                


bool CInmageIoctlController::LocalCrashConsistentTagIoctl(std::string tagContext,
    list<string> taglist, unsigned long uiFlags, int inTimeoutInMS)
{
    bool status = true;
    char* pBuffer = GetTagBuffer(tagContext, taglist, uiFlags, inTimeoutInMS);
    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_TAG_VOLUME_V2,
        pBuffer, NULL, NULL, NULL, NULL))
    {
        status = false;
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }
    return status;
}

SV_ULONG CInmageIoctlController::GetDataBufferAllocationLength(SV_ULONG ulDataLength)
{
    ULONG pTotalLength = 0;

    if (ulDataLength <= ((ULONG)0xFF - sizeof(STREAM_REC_HDR_4B)))
    {
        pTotalLength = ulDataLength + (ULONG)sizeof(STREAM_REC_HDR_4B);
    }
    else
    {
        pTotalLength = ulDataLength + (ULONG)sizeof(STREAM_REC_HDR_8B);
    }

    return pTotalLength;
}

char *CInmageIoctlController::BuildStreamRecordHeader(char *dataBuffer,
    SV_ULONG &ulPendingBufferLength, SV_UINT usTag, SV_ULONG ulDataLength)
{
    if (ulDataLength <= ((SV_ULONG)0xFF - sizeof(STREAM_REC_HDR_4B)))
    {
        FILL_STREAM_HEADER_4B(&dataBuffer[0], usTag,
            (unsigned char)(sizeof(STREAM_REC_HDR_4B) + ulDataLength));
        dataBuffer += sizeof(STREAM_REC_HDR_4B);
        ulPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);
    }
    else
    {
        FILL_STREAM_HEADER_8B(&dataBuffer[0], usTag, sizeof(STREAM_REC_HDR_8B) + ulDataLength);
        dataBuffer += sizeof(STREAM_REC_HDR_8B);
        ulPendingBufferLength -= sizeof(STREAM_REC_HDR_8B);
    }

    return dataBuffer;
}

char *CInmageIoctlController::BuildStreamRecordData(char *dataBuffer,
    SV_ULONG &ulPendingBufferLength, void *pData, SV_ULONG ulDataLength)
{
    memcpy_s(&dataBuffer[0], ulPendingBufferLength, pData, ulDataLength);
    dataBuffer += ulDataLength;
    ulPendingBufferLength -= ulDataLength;
    return dataBuffer;
}

char *CInmageIoctlController::GetTagBuffer(std::string tagGuid,
    list<string> taglist, unsigned long uiFlags, int inTimeoutInMS)
{
    std::string inputTag = "Tag: CrashTag";
    unsigned short tagId = 301;

    SV_ULONG TotalTagLen = 0;
    SV_ULONG TagLen = 0;
    SV_ULONG TagHeaderLen = 0;
    SV_ULONG TagGuidLen = 0;
    SV_ULONG ulDataLength = 0;
    SV_ULONG buflen = 0;

    TagLen = inputTag.size() + 1;
    if (TagLen <= ((SV_ULONG)0xFF - sizeof(STREAM_REC_HDR_4B)))
    {
        TagHeaderLen = sizeof(STREAM_REC_HDR_4B);
    }
    else
    {
        TagHeaderLen = sizeof(STREAM_REC_HDR_8B);
    }

    TagGuidLen = tagGuid.size() + 2;
    char strTagGuid[TagGuidLen];
    // make sure there is no un-initialized data being sent with the tag
    memset(strTagGuid, 0, TagGuidLen);

    //Copy including '\0'
    memcpy_s(strTagGuid, sizeof(strTagGuid), tagGuid.c_str(), tagGuid.size() + 1);
    printf("Tag : %s\n", strTagGuid);

    ulDataLength = TagGuidLen + TagLen;

    TotalTagLen = GetDataBufferAllocationLength(ulDataLength);
    //since there are two headers, we need to add header size again to the total length.
    //To add the header length again(for the second header)
    //call this function GetDataBufferAllocationLength() again

    buflen = ulDataLength + TagHeaderLen;
    TotalTagLen = GetDataBufferAllocationLength(buflen);

    char *buffer = new char[TotalTagLen];

    char *bufferCurr = buffer;
    SV_ULONG ulPendingBufferLength = TotalTagLen;

    //Generate a Header to store the Tag Guid
    bufferCurr = BuildStreamRecordHeader(bufferCurr, ulPendingBufferLength,
        STREAM_REC_TYPE_TAGGUID_TAG, TagGuidLen);

    //Fill in the Tag Guid in the Data Part of the Stream
    bufferCurr = BuildStreamRecordData(bufferCurr, ulPendingBufferLength,
        (void *)strTagGuid, TagGuidLen);

    ulDataLength = (SV_ULONG)(inputTag.size()) + 1;
    bufferCurr = BuildStreamRecordHeader(bufferCurr, ulPendingBufferLength, tagId, ulDataLength);

    bufferCurr = BuildStreamRecordData(bufferCurr, ulPendingBufferLength,
        (void *)inputTag.c_str(), ulDataLength);

    // by default, we want to issue tags atomically across volumes
    SV_UINT total_buffer_size = sizeof(tag_info_t_v2) + sizeof(tag_names_t);

    char *finalBuffer = new char [total_buffer_size + 1];
    tag_info_t_v2 *tag_info = (tag_info_t_v2 *) finalBuffer;
    tag_info->flags = TAG_ALL_PROTECTED_VOLUME_IOBARRIER;
    tag_info->nr_vols = 0;
    tag_info->nr_tags = 1;
    tag_info->timeout = inTimeoutInMS;
    tag_info->vol_info = NULL;
    tag_names_t *tag_names = (tag_names_t *) (finalBuffer + sizeof(tag_info_t_v2));
    tag_info->tag_names = tag_names;
    strncpy(tag_info->tag_guid, tagGuid.c_str(), tagGuid.length());

    memcpy_s(tag_names->tag_name, TAG_MAX_LENGTH, buffer, TotalTagLen);
    tag_names->tag_len = TotalTagLen;

    return finalBuffer;
}

bool CInmageIoctlController::LocalStandAloneCrashConsistentTagIoctl(std::string tagContext,
    list<string> taglist, unsigned long uiFlags, int inTimeoutInMS)
{
    bool status = true;
    char* pBuffer = GetTagBuffer(tagContext, taglist, uiFlags, inTimeoutInMS);

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_IOBARRIER_TAG_VOLUME,
        pBuffer, NULL, NULL, NULL, NULL))
    {
        status = false;
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }

    return status;
}

bool CInmageIoctlController::GetVolumeStats(std::string &volGuid, std::string &volumeStat)
{
    bool status = true;
    VOLUME_STATS volStats;

    volStats.bufp = (char *)malloc(INM_PAGESZ);
    strncpy(volStats.guid.volume_guid, volGuid.c_str(), GUID_SIZE_IN_CHARS);
    volStats.guid.volume_guid[GUID_SIZE_IN_CHARS - 1] = '\0';
    volStats.buf_len = INM_PAGESZ;

    if (!m_platformApis->DeviceIoControlSync(
        s_hInmageCtrlDevice,
        (unsigned long)IOCTL_INMAGE_GET_VOLUME_STATS,
        (void *)&volStats, NULL, NULL, NULL, NULL))
    {
        status = false;
        throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
    }

    volumeStat = std::string(static_cast<const char*>(volStats.bufp), INM_PAGESZ);
    return status;
}

#ifdef UNIT_TEST
int main(int argc, char* argv[])
{
  
    CInmageIoctlController* m_ioctlController;
    PlatformAPIs* m_platformApis;
    CLogger* m_s2Logger;
    string m_testRoot="/test/test_di/release";
    m_s2Logger = new CLogger(m_testRoot + "/" + "s2.log");
    m_platformApis = new PlatformAPIs();
    m_ioctlController = new CInmageIoctlController(m_platformApis, m_s2Logger);

    if (argc < 2)
    {
        cout << endl << "Usage: \n\tTest_Ioctlcontroller \
            --op=<operation> --dev=<device> --tag=<tag1> \
            \n\t\t Operation=\n\t\t\tServiceShutdownNotify \
            \n\t\t\tProcessStartNotify \
            \n\t\t\tStartFiltering \
            \n\t\t\tStopFiltering \
            \n\t\t\tClearDifferentials \
            \n\t\t\tditest"<<endl;
        return -1;
    }
    std::string strOperation = argv[1];
    std::string strDevice;
    std::string strTag;
    unsigned long ulFlags =0;
    std::string device;

    if ((strOperation.compare("--op=StopFiltering") == 0) ||
        (strOperation.compare("--op=StartFiltering") == 0) ||
        (strOperation.compare("--op=ClearDifferentials") == 0))
    {
        strDevice = argv[2];
        strDevice = strDevice.substr(strDevice.find("--dev=") + 6);
        if (argc > 2)
        {
            strTag = argv[3];
            strTag = strTag.substr(strTag.find("--tag=") + 6);
        }
        if (strOperation.compare("--op=StopFiltering") == 0)
        {
            //Stop Filtering
            cout<<endl<<"Step:StopFiltering"<<endl;
            m_ioctlController->StopFiltering(strDevice,true,false);
        }
        else if (strOperation.compare("--op=StartFiltering") == 0)
        {           
            cout<<endl<<"Step:StartFiltering"<<endl;
            m_ioctlController->StartFiltering(strDevice, 2097152, 512);
        }
        else if (strOperation.compare("--op=ClearDifferentials") == 0)
        {
            cout << endl << "Step:ClearDifferentials" << endl;
            m_ioctlController->ClearDifferentials(strDevice);
        }
        else if (strOperation.compare("--op=InsertTag") == 0)
        {
            if (strTag.empty())
                strTag = "TestTag";
        }
    }

    else if (strOperation.compare("--op=ServiceShutdownNotify") == 0)
    {
        //Service ShutdownNotify
        cout<<endl<<"Step:ServiceShutdownNotify"<<endl;
        m_ioctlController->ServiceShutdownNotify(ulFlags);
    }
    else if (strOperation.compare("--op=ProcessStartNotify") == 0)
    {
        //ProcessStartNotify
        cout<<endl<<"ProcessStartNotify"<<endl;
        m_ioctlController->ProcessStartNotity(ulFlags);
    }
    else if(strOperation.compare("--op=ditest") == 0)
    {
        strDevice = argv[2];
        strDevice = strDevice.substr(strDevice.find("--dev=") + 6);

        //Service ShutdownNotify
        cout<<endl<<"Step:ServiceShutdownNotify"<<endl;
        m_ioctlController->ServiceShutdownNotify(ulFlags);

        cout << endl << "Step:GetDriverVersion" << endl;
        m_ioctlController->GetDriverVersion();

        cout<<endl<<"Step:StartFiltering"<<endl;
        m_ioctlController->StartFiltering(strDevice, 2097152, 512);

        unsigned long ulFlags =0;


        //GetDirtyBlockTransaction
        boost::shared_ptr<UDIRTY_BLOCK_V2> m_DirtyBlock;
        m_DirtyBlock.reset(new UDIRTY_BLOCK_V2);

        cout << endl << "Prestep for GetDirtyBlockTransaction is to get the dirtyblock" << endl;
        PUDIRTY_BLOCK_V2 udbp;
        udbp = m_DirtyBlock.get();
        if (!udbp)
        {
            cout << endl << "Failed to create a Dirty Block." << endl;
            throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
        }
        memset(udbp, 0x00, sizeof(*udbp));

        boost::shared_ptr < COMMIT_TRANSACTION > commitDB;
        commitDB.reset(new COMMIT_TRANSACTION);
        PCOMMIT_TRANSACTION commit;
        commit = commitDB.get();
        if (!commit)
        {
            cout << endl << "Failed to create a commit object." << endl;
            throw CAgentException("%s failed with error=0x%x",
            __FUNCTION__, m_platformApis->GetLastError());
        }
        memset((void*)&(commit->VolumeGUID[0]), 0x00, GUID_SIZE_IN_CHARS);
        cout<<endl<<"Step:GetDirtyBlockTransaction"<<endl;

        m_ioctlController->GetDirtyBlockTransaction(commit, udbp, false, strDevice);
        cout << "\nTransactionid: " <<udbp->uHdr.Hdr.uliTransactionID << "\n";
        cout << "Changes in Stream: " <<udbp->uHdr.Hdr.ulcbChangesInStream<<"\n";

        if (udbp->uHdr.Hdr.ulFlags | UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
        {
            commit->ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
        }

        commit->ulTransactionID = udbp->uHdr.Hdr.uliTransactionID;
        memcpy((void*)&(commit->VolumeGUID[0]), strDevice.c_str(), strDevice.size());

        //CommitTransaction
        cout<<endl<<"CommitTransaction"<<endl;
        m_ioctlController->CommitTransaction(commit);

        //Inserting tags

        list<string> taglist ;
        taglist.push_back("tag111");
        taglist.push_back("tag222");
        taglist.push_back("tag333");
        taglist.push_back("tag444");

        //CrashTag
        cout<<endl<<"Step:LocalCrashConsistentTagIoctl"<<endl;
        m_ioctlController->LocalStandAloneCrashConsistentTagIoctl(
        "ABCDE", taglist, (SV_ULONG)TAG_ALL_PROTECTED_VOLUME_IOBARRIER, 360000);

        //HoldWrites
        cout<<endl<<"Step:HoldWrites."<<endl;
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string mtag = boost::lexical_cast<std::string>(uuid);
        m_ioctlController->HoldWrites(mtag/*"ABCDEF"*/, TAG_ALL_PROTECTED_VOLUME_IOBARRIER, 300);

        //ReleaseWrites
        cout<<endl<<"Step:ReleaseWrites"<<endl;
        m_ioctlController->ReleaseWrites("ABCDEF",TAG_ALL_PROTECTED_VOLUME_IOBARRIER);

        //CommitTag
        cout<<endl<<"Step:CommitTag"<<endl;
        m_ioctlController->CommitTag("ABCDEF",TAG_ALL_PROTECTED_VOLUME_IOBARRIER);

        //InsertDistTag
        cout<<endl<<"Step:InsertDistTag"<<endl;
        m_ioctlController->InsertDistTag("ABCDEF",TAG_ALL_PROTECTED_VOLUME_IOBARRIER,taglist);


        //ResyncStartNotification
        cout << endl << "ResyncStartNotification..." << endl;
        m_ioctlController->ResyncStartNotification(strDevice);

        //ResyncEndNotification
        cout << endl << "ResyncEndNotification..." << endl;
        m_ioctlController->ResyncEndNotification(strDevice);

        //Clear Differentials
        cout << endl << "Step:ClearDifferentials" << endl;
        m_ioctlController->ClearDifferentials(strDevice);

        //Stop Filtering
        cout << endl << "Step:StopFiltering" << endl;
        m_ioctlController->StopFiltering(strDevice, true, false);

    }
}

#endif
