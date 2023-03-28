#include "dlmdiskinfo.h"

DlmDiskInfo* DlmDiskInfo::theDlmDiskInfo = NULL;
boost::mutex DlmDiskInfo::m_DlmDiskInfoLock;
std::map<std::string, std::string>  DlmDiskInfo::m_DisksHavingUEFI;
time_t DlmDiskInfo::m_dlmLastRefreshTime;
DlmPartitionInfoSubMap_t DlmDiskInfo::m_DiskPartitionInfo;

/*
 * FUNCTION NAME : DlmDiskInfo
 *
 * DESCRIPTION : constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
DlmDiskInfo::DlmDiskInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : ~DlmDiskInfo
 *
 * DESCRIPTION : Destructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
DlmDiskInfo::~DlmDiskInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : HasUEFI
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool DlmDiskInfo::HasUEFI(const std::string &diskID)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    boost::mutex::scoped_lock guard(m_DlmDiskInfoLock);

	DebugPrintf(SV_LOG_INFO,"Disk ID to Check EFI or not is : %s\n", diskID.c_str());

	//need to get current time.
	time_t curTime;
	curTime = time(NULL);

	double diffInTime = difftime(curTime, m_dlmLastRefreshTime); //This will get the difference in time between the last refresh to the current one(return in seconds)

    if ( (diffInTime/3600) > 4 )
    {
		DebugPrintf(SV_LOG_INFO,"Refreshing EFI disks list...\n");
		if(RefreshDiskInfo(false))  // need to dicover only EFI disk information not the partition information
			DebugPrintf(SV_LOG_INFO,"successfully Processed the EFI disk information\n");
		else
			DebugPrintf(SV_LOG_ERROR,"Failed to process the EFI disk information\n");
    }
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_DisksHavingUEFI.end() != m_DisksHavingUEFI.find(diskID));
}


/*
 * FUNCTION NAME : GetPartitionInfo
 *
 * DESCRIPTION : Gets the partition details fot the required disk(partitions are like EFI, Recovery, MSR)
 *
 * INPUT PARAMETERS : Disk Name
 *
 * OUTPUT PARAMETERS : map of disk to the partition information
 *
 * NOTES :
 *
 * return value : 
 *
 */
void DlmDiskInfo::GetPartitionInfo(const std::string& diskName, DlmPartitionInfoSubVec_t& vecPartitions)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_INFO,"Disk to Get Partition details is : %s\n", diskName.c_str());

	DlmPartitionInfoSubIterMap_t iterPart = m_DiskPartitionInfo.find(diskName);

	if(iterPart != m_DiskPartitionInfo.end())
	{
		DebugPrintf(SV_LOG_INFO,"Got special partitions information for Disk %s\n", diskName.c_str());
		vecPartitions = iterPart->second;
	}
	else
		DebugPrintf(SV_LOG_INFO,"Disk %s is not having any special partiton information\n", diskName.c_str());
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME : Init
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool DlmDiskInfo::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	bool bRet = true;

	if(RefreshDiskInfo())
		DebugPrintf(SV_LOG_INFO,"successfully Processed the EFI disk information\n");
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the EFI disk information\n");
		bRet = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return bRet;
}


/*
 * FUNCTION NAME : Destroy
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool DlmDiskInfo::Destroy()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    boost::mutex::scoped_lock guard(m_DlmDiskInfoLock);

    if ( NULL != theDlmDiskInfo)
    {
        delete theDlmDiskInfo;
        theDlmDiskInfo = NULL;
    }

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return true;
}

/*
 * FUNCTION NAME : GetInstance
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
DlmDiskInfo& DlmDiskInfo::GetInstance()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    boost::mutex::scoped_lock guard(m_DlmDiskInfoLock);

    if ( NULL == theDlmDiskInfo )
    {
		theDlmDiskInfo = new DlmDiskInfo;
        theDlmDiskInfo->Init();
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return *theDlmDiskInfo;
	
}


/*
 * FUNCTION NAME : RefreshDiskInfo
 *
 * DESCRIPTION : will generate the EFI disks list again
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool DlmDiskInfo::RefreshDiskInfo(bool bNotUefi)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	bool bResult = true;

	if(InitilizeVDS())
	{
		m_DisksHavingUEFI.clear();
		FindEfiPartitionDisk(m_DisksHavingUEFI); //Finds out all the EFI partitioned disks

		if(bNotUefi)
			GetDiskPartitionInfo(m_DiskPartitionInfo);  //Gets the special partition information for the disks

		m_dlmLastRefreshTime = time(NULL);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out the EFI deatils for disks\n");
		bResult = false;
	}
	UnInitialize();

	if(m_DisksHavingUEFI.empty())
	{
		DebugPrintf(SV_LOG_INFO,"EFI Partitioned disks are not available\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return bResult;
}


/*
 * FUNCTION NAME : RefreshDiskLayoutManager
 *
 * DESCRIPTION : Refreshes the EFI information. This is a force call 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void DlmDiskInfo::RefreshDiskLayoutManager()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	boost::mutex::scoped_lock guard(m_DlmDiskInfoLock);

	DebugPrintf(SV_LOG_INFO,"Refreshing EFI disks list due to on demand call...\n");

	if(RefreshDiskInfo(false))  //only refreshes the EFI disk information
		DebugPrintf(SV_LOG_INFO,"successfully Processed the EFI disk information\n");
	else
		DebugPrintf(SV_LOG_ERROR,"Failed to process the EFI disk information\n");

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}