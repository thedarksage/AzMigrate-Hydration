#include "cdpsnapshot.h"
#include "error.h"
#include "cdpv1database.h"
#include "cdpv1globals.h"
#include "cdputil.h"
#include "cdpplatform.h"

#include "VsnapUser.h"
#include "inmcommand.h"
#include "portablehelpersmajor.h"

#include "inmageex.h"
#include "inmsafeint.h"

#include "configwrapper.h"

#include <WinIoCtl.h>
/*
* FUNCTION NAME :  SnapshotTask::IsValidDestination
*
* DESCRIPTION : verifies the following condition 
*              1. Do not contain any logical partition
*			   2. Not a system/boot volume
*              3. Not a cache volume
*              4. Not a vx install volume
*              5. Not a pagefile configure volume
*
* INPUT PARAMETERS : argc - no. of arguments
*                    argv - arguments based on operation
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/

bool SnapshotTask::IsValidDestination(const std::string & volumename, std::string &errormsg)
{
	bool rv = true;
	do
	{
		//checking for logical partition
		if(!IsLeafDevice(volumename))
		{
			errormsg = "snapshot cannot be taken on ";
			errormsg += volumename;
			errormsg += ". It contains logical partitions.\n";
			rv = false;
			break;
		}

		//checking for boot volume or system volume
		char rgtszSystemDirectory[ MAX_PATH + 1 ];
		if(GetSystemDirectory( rgtszSystemDirectory, MAX_PATH + 1 ) )
		{
			if((IsDrive(volumename)) && 
				(((volumename.size() <= 3) && (rgtszSystemDirectory[0] == volumename[0])) ||
				(('\\' == volumename[0]) && (rgtszSystemDirectory[0] == volumename[4]))))
			{
				errormsg = " the volume ";
				errormsg = volumename;
				errormsg += " is a system volume.\n";
				rv = false;
				break;
			}
		}

		//checking for cache volume
		if(IsCacheVolume(volumename))
		{
			errormsg = "snapshot cannot be taken on ";
			errormsg += volumename;
			errormsg += ". It is a cache volume.\n";
			rv = false;
			break;
		}

		//checking for vx install volume
		if(IsInstallPathVolume(volumename))
		{
			errormsg = "snapshot cannot be taken on ";
			errormsg += volumename;
			errormsg += ". It is a Vx install volume.\n";
			rv = false;
			break;
		}

		//checking for volume contains page file pagefile
		if(IsDrive(volumename))
		{
			DWORD pageDrives = getpagefilevol();
			DWORD dwSystemDrives = 0;
			if('\\' == volumename[0])
				dwSystemDrives = 1 << ( toupper( volumename[4] ) - 'A' );
			else
				dwSystemDrives = 1 << ( toupper( volumename[0] ) - 'A' );

			if(pageDrives & dwSystemDrives)
			{
				errormsg = "snapshot cannot be taken on ";
				errormsg += volumename;
				errormsg += ". The volume contains pagefile.\n";
				rv = false;
				break;
			}
		}

	} while (false);

	return rv;
}

/*
 * Local static function for retreiving block usage information from the give handle provided.
 * 1) Retreives the bytes per cluster or the cluster size using FSCTL_GET_NTFS_VOLUME_DATA
 * 2) For the given offset (cluster number), retreive the number of clusters using FSCTL_GET_VOLUME_BITMAP
 * 3) Allocate required bitmap memory restricted by maximum 1MB to retreive the cluster usage information.
 * 4) Convert the cluster usage map to block usage map (a block is marked used if any of the cluster is used with in the block)
 * 5) Repeat 3 and 4 until all the requested blocks are read or we reach the end of the volume.
 */

//Retrieves the cluster size for the volume provided.
static bool get_cluster_size(ACE_HANDLE vol_handle,SV_ULONGLONG &bytespercluster)
{
	bool rv = false;
	do
	{
		NTFS_VOLUME_DATA_BUFFER ntfsvolumedatabuffer;
		DWORD bytesreturned = 0;
		rv = DeviceIoControl(vol_handle,FSCTL_GET_NTFS_VOLUME_DATA,NULL,0,&ntfsvolumedatabuffer,sizeof(NTFS_VOLUME_DATA_BUFFER),&bytesreturned,					NULL);
		if(!rv)
		{
			std::stringstream sserr;
			sserr << GetLastError();
			DebugPrintf(SV_LOG_ERROR,"FSCTL_GET_NTFS_VOLUME_DATA failed with %s",sserr.str().c_str());
			break;
		}
		bytespercluster = ntfsvolumedatabuffer.BytesPerCluster;
	}
	while(0);

	return rv;
}

//Retreives the total number of clusters from the starting cluster number provided
static bool get_total_number_of_clusters(ACE_HANDLE vol_handle,SV_ULONGLONG startingclusternum, SV_ULONGLONG &totalclusters)
{
	bool rv = false;
	do
	{
		STARTING_LCN_INPUT_BUFFER startingLcn;
		startingLcn.StartingLcn.QuadPart = startingclusternum;

		VOLUME_BITMAP_BUFFER volumebitmapbuf;
		memset(&volumebitmapbuf, 0, sizeof volumebitmapbuf);

		DWORD bytesreturned;

		rv = DeviceIoControl(vol_handle, FSCTL_GET_VOLUME_BITMAP, &startingLcn,sizeof(startingLcn), 
			&volumebitmapbuf, sizeof(volumebitmapbuf),&bytesreturned, NULL);

		DWORD err = GetLastError();
		if (!rv && err != ERROR_MORE_DATA)
		{
			std::stringstream sserr;
			sserr << err;
			DebugPrintf(SV_LOG_ERROR,"FSCTL_GET_VOLUME_BITMAP failed with %s",sserr.str().c_str());
			break;
		}

		totalclusters = volumebitmapbuf.BitmapSize.QuadPart;
		rv = true;
	}while(0);

	return rv;

}

//Retrieves the cluster usage information bitmap from the startingcluster number provided to the 
//max number of clusters that can fit into the memory allocated.
static bool get_usage_bitmap_for_clusters(ACE_HANDLE vol_handle,SV_ULONGLONG startingcluster,SV_ULONGLONG &num_clusters_to_read,
								   VOLUME_BITMAP_BUFFER ** volumebitmapbuf,SV_ULONGLONG bitmapsize)
{
	bool rv = false;
	do
	{
		STARTING_LCN_INPUT_BUFFER startingLcn;
		DWORD bytesreturned;
		SV_ULONGLONG num_of_clusters_returned = 0;

		startingLcn.StartingLcn.QuadPart = startingcluster;
		rv = DeviceIoControl(vol_handle, FSCTL_GET_VOLUME_BITMAP, &startingLcn,sizeof (startingLcn),
			*volumebitmapbuf, bitmapsize, &bytesreturned, NULL);

		DWORD LastError = GetLastError();

		if (!rv && LastError != ERROR_MORE_DATA)
		{
			std::stringstream sserr;
			sserr << LastError;
			DebugPrintf(SV_LOG_ERROR,"FSCTL_GET_VOLUME_BITMAP failed with %s",sserr.str().c_str());
			break;
		}

		if(!rv && LastError == ERROR_MORE_DATA)
		{
			num_of_clusters_returned = ((bytesreturned - sizeof(VOLUME_BITMAP_BUFFER))*NBITSINBYTE);
		}
		else
		{
			num_of_clusters_returned = (*volumebitmapbuf)->BitmapSize.QuadPart;
		}

		if(num_clusters_to_read > num_of_clusters_returned)
		{
			num_clusters_to_read = num_of_clusters_returned;
		}

		rv = true;
	}while(0);

	return rv;
}

//Verifies if all the clusters are unused from the given cluster number to the number of clusters to constitute a block.
static bool is_all_clusters_unused(SV_ULONGLONG clusternum,SV_ULONGLONG num_of_clusters_in_block,VOLUME_BITMAP_BUFFER * p_volumebitmapbuf)
{
	bool cluster_unused = true;

	for(int i = clusternum;i < (clusternum+num_of_clusters_in_block);i++)
	{
		if(((p_volumebitmapbuf->Buffer[INDEX_IN_BITMAP_ARRAY(i)] & (1 << BIT_POSITION_IN_DATA(i))) ? 1 : 0))
		{
			cluster_unused = false;
			break;
		}
	}

	return cluster_unused;
}

//clears the corresponding bit for the block_num provided.
static void unset_bit_for_block(SV_ULONGLONG block_num,BlockUsageInfo &blockusageinfo)
{
	blockusageinfo.bitmap[INDEX_IN_BITMAP_ARRAY(block_num)] &= ~(1 << BIT_POSITION_IN_DATA(block_num));
}

static bool get_bitmap_for_blocks_vsnapvolume(ACE_HANDLE vol_handle,BlockUsageInfo &blockusageinfo)
{
	bool rv = true;
	do
	{
		DWORD bytesreturned = 0;
		SV_ULONGLONG bytespercluster = 0;

		if(!get_cluster_size(vol_handle,bytespercluster))
		{
			rv = false;
			break;
		}

		if((blockusageinfo.block_size % bytespercluster) != 0)
		{
			DebugPrintf(SV_LOG_ERROR,"Block size provided in not a multiple of the cluster size of the volume\n");
			rv = false;
			break;
		}

		SV_ULONGLONG num_of_clusters_in_block = blockusageinfo.block_size/bytespercluster;

		blockusageinfo.startoffset = ALIGN_OFFSET(blockusageinfo.startoffset,blockusageinfo.block_size); //Align the offset to block_size multiples.
		blockusageinfo.starting_blocknum = BLOCK_NUM(blockusageinfo.startoffset,blockusageinfo.block_size);

		SV_ULONGLONG totalclusters = 0;
		SV_ULONGLONG startingclusternum = OFFSET_TO_CLUSTER_NUM(blockusageinfo.startoffset,bytespercluster);

		if(!get_total_number_of_clusters(vol_handle,startingclusternum,totalclusters))
		{
			rv = false;
			break;
		}

		SV_ULONGLONG total_num_of_blocks = (totalclusters * bytespercluster)/blockusageinfo.block_size + 
			((totalclusters * bytespercluster)%blockusageinfo.block_size?1:0);

		assert(total_num_of_blocks >= blockusageinfo.block_count); //Total number of blocks should be greater or equal to requested blocks.
		if(blockusageinfo.block_count > total_num_of_blocks)
		{
			DebugPrintf(SV_LOG_ERROR, "Requested blocks(" ULLSPEC") is greater than the actual number of blocks(" ULLSPEC") returned from volume",blockusageinfo.block_count,total_num_of_blocks);			
			rv = false;
			break;
		}

		//Adjust count value based on the number of clusters remaining.
		SV_ULONGLONG remaining_clusters_to_read = (blockusageinfo.block_count*blockusageinfo.block_size)/bytespercluster;
		if(remaining_clusters_to_read > totalclusters)
		{
			remaining_clusters_to_read = totalclusters;
			blockusageinfo.block_count = (remaining_clusters_to_read*bytespercluster)/blockusageinfo.block_size + 
										((remaining_clusters_to_read*bytespercluster)%blockusageinfo.block_size?1:0);

		}

		SV_UINT blockusage_bitmap_size = blockusageinfo.block_count/NBITSINBYTE + (blockusageinfo.block_count%NBITSINBYTE?1:0);

		blockusageinfo.bitmap = (SV_BYTE *)malloc(blockusage_bitmap_size);

		if( !blockusageinfo.bitmap )
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for bitmap @Line %d and file %s\n",
				LINE_NO,FILE_NAME);
			rv = false;
			break;
		}

		memset(blockusageinfo.bitmap,0xFF,blockusage_bitmap_size); //Initialize the bitmap memory to all 1s to indicate all blocks are in use

		//Restrict max memory to allocate for bitmap to 1M(configuratble). Number of clusters that can fit into 1M is 1048576*8
		SV_ULONGLONG current_blk_num = 0;
		LocalConfigurator localConfigurator;
		SV_ULONGLONG max_memory_for_reading_fsbitmap = localConfigurator.getMaxMemForReadingFSBitmap();
		SV_ULONGLONG max_clusters_for_allocated_memory = max_memory_for_reading_fsbitmap * 8; //Each byte can represent 8 clusters
		SV_ULONGLONG current_cluster = OFFSET_TO_CLUSTER_NUM(blockusageinfo.startoffset,bytespercluster);

		VOLUME_BITMAP_BUFFER * p_volumebitmapbuf;

		SV_ULONGLONG totalbitmapsize;
        INM_SAFE_ARITHMETIC(totalbitmapsize = sizeof (VOLUME_BITMAP_BUFFER) + InmSafeInt<SV_ULONGLONG>::Type(max_memory_for_reading_fsbitmap), INMAGE_EX(sizeof (VOLUME_BITMAP_BUFFER))(max_memory_for_reading_fsbitmap))

		p_volumebitmapbuf = (VOLUME_BITMAP_BUFFER *) malloc (totalbitmapsize);

		if(!p_volumebitmapbuf)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for bitmap @Line %d and file %s\n",
			LINE_NO,FILE_NAME);
			rv = false;
			break;
		}

		while(remaining_clusters_to_read)
		{
			SV_ULONGLONG num_clusters_to_read = std::min(remaining_clusters_to_read,max_clusters_for_allocated_memory);
			SV_ULONGLONG count = 0;

			memset(p_volumebitmapbuf,0xFF,totalbitmapsize); //Initialize the bitmap memory to all 1s to indicate all blocks are in use

			totalbitmapsize = sizeof (VOLUME_BITMAP_BUFFER) + (num_clusters_to_read / NBITSINBYTE) 
			+ (num_clusters_to_read % NBITSINBYTE?1:0);

			if(!get_usage_bitmap_for_clusters(vol_handle,current_cluster,num_clusters_to_read,&p_volumebitmapbuf,totalbitmapsize))
			{
				rv = false;
				break;
			}

			SV_ULONGLONG cluster_consumed = 0;

			while(cluster_consumed < num_clusters_to_read)
			{
				if((cluster_consumed + num_of_clusters_in_block) > num_clusters_to_read)
				{
					//The last block can have less than the max num_of_clusters_in_block.
					num_of_clusters_in_block = num_clusters_to_read - cluster_consumed; 
				}

				if(is_all_clusters_unused(cluster_consumed,num_of_clusters_in_block,p_volumebitmapbuf))
				{
					//All clusters in the current block are unused. Unset the bit to mark block as not in use.
					unset_bit_for_block(current_blk_num,blockusageinfo);
				}

				cluster_consumed += num_of_clusters_in_block;
				current_blk_num++; //Increment the block count
			}

			current_cluster += num_clusters_to_read;
			remaining_clusters_to_read -= num_clusters_to_read;
		}
		
		if(p_volumebitmapbuf)
		{
			free(p_volumebitmapbuf);
		}

	}while(0);

	return rv;
}

/*
 * Description: Retrieves the cluster info for the given volume and creates a map based on the block size provided to indicate the block.
 * blockusageinfo struct is modified according to align the offset provided with the block size provided.
 */

bool SnapshotTask::get_used_block_information(std::string volname,std::string destname, BlockUsageInfo &blockusageinfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
	bool rv = true;

	try
	{
		do
		{
			WinVsnapMgr vsnapMgr;
			boost::shared_ptr<VsnapSourceVolInfo> srcvol(new VsnapSourceVolInfo());
			boost::shared_ptr<VsnapVirtualVolInfo> vsnapvol(new VsnapVirtualVolInfo());

			// fill in the replicated volume details
			srcvol -> VolumeName = volname;
			if ( IsDrive(srcvol -> VolumeName))
			{
				UnformatVolumeNameToDriveLetter(srcvol ->VolumeName);
			}    		


			std::string dbpath = m_request -> dbpath;
			if(!dbpath.empty())
			{
				// convert the path to real path (no symlinks)
				if( !resolvePathAndVerify( dbpath ) )
				{
					DebugPrintf( SV_LOG_DEBUG,"Retention Database Path %s not yet created\n",
						dbpath.c_str() ) ;
					rv = false;
					break;
				}
			}
			srcvol -> RetentionDbPath = dbpath;

			// fill in the virtual volume details

			std::string vsnapdir = m_localConfigurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_CHAR_A + Application_Data_Folder;
			std::string snap_dest = destname;
			FormatVolumeNameForCxReporting(snap_dest);
			std::string virtualvolumename = vsnapdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + GetVsnapVolumeName(volname,snap_dest);
			std::string datalogpath;
			if(dbpath.empty())
				datalogpath = vsnapdir + ACE_DIRECTORY_SEPARATOR_CHAR_A + "DataLog";
			else
				datalogpath = dirname_r(dbpath.c_str());



			if(m_request -> operation == SNAPSHOT_REQUEST::PLAIN_SNAPSHOT)
			{
				vsnapvol->VsnapType = VSNAP_POINT_IN_TIME;
			} else
			{

				vsnapvol->VsnapType = VSNAP_RECOVERY;
				if(!m_request ->recoverypt.empty()) 
				{

					SV_TIME svtime;
					SV_ULONGLONG recoverypt = 0;
					sscanf(m_request ->recoverypt.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
						&svtime.wYear,
						&svtime.wMonth,
						&svtime.wDay,
						&svtime.wHour,
						&svtime.wMinute,
						&svtime.wSecond,
						&svtime.wMilliseconds,
						&svtime.wMicroseconds,
						&svtime.wHundrecNanoseconds);

					if(!CDPUtil::ToFileTime(svtime, recoverypt))	
					{
						DebugPrintf(SV_LOG_INFO, "specified recovery point %s is invalid.\n", m_request -> recoverypt.c_str());
						rv = false;
						break;
					}
					vsnapvol -> TimeToRecover = recoverypt;
				}

				vsnapvol -> EventName = m_request ->recoverytag;
				vsnapvol -> SequenceId = m_request ->sequenceId;
			}

			vsnapvol->VolumeName = virtualvolumename;
			vsnapvol->AccessMode = VSNAP_READ_WRITE;
			vsnapvol->PrivateDataDirectory = datalogpath;
			vsnapvol->ReuseDriveLetter = true;	
			vsnapvol->MountVsnap = false;

			srcvol->VirtualVolumeList.push_back(vsnapvol.get());

			// create the vsnap
			vsnapMgr.Mount(srcvol.get());

			if(vsnapvol->Error.VsnapErrorStatus != VSNAP_SUCCESS)
			{
				std::string errmsg;
				if(vsnapMgr.VsnapQuit())
				{
					DebugPrintf(SV_LOG_INFO, "Vsnap Operation Aborted as Quit Requested\n");
				}
				else
				{
					vsnapMgr.VsnapGetErrorMsg(vsnapvol->Error, errmsg);
					DebugPrintf(SV_LOG_INFO, "Vsnap Failed: %s\n", errmsg.c_str());
				}
				rv = false;
				break;
			}

			// open vsnap device and get used block list
			ACE_HANDLE vol_handle = SVCreateFile(vsnapvol->VolumeGuid.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

			if(vol_handle == ACE_INVALID_HANDLE)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open vsnap volume %s. Error %lu @Line %d in File %s\n",
					virtualvolumename.c_str(),GetLastError(), LINE_NO, FILE_NAME);
				rv = false;
			}

			if(rv)
			{
				rv = get_bitmap_for_blocks_vsnapvolume(vol_handle,blockusageinfo);
			}

			ACE_OS::close(vol_handle);

			// After getting the used blocks, we assign mount point to the vsnap
			// as part of vsnap deletion, all reparse points (mounted volumes beneath vsnap) will
			// be unmounted to avoid issues mentioned in bug report 22960
			std::string volumelink = vsnapvol ->VolumeName + "\\";
			std::string volumeguid = vsnapvol ->VolumeGuid + "\\";
			if(0 == SetVolumeMountPoint(volumelink.c_str(), volumeguid.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Mount point assignment failed for  %s %s. Error %lu\n", 
					volumelink.c_str(), volumeguid.c_str(), GetLastError());
			}

			vsnapvol -> State = VSNAP_UNMOUNT;


			if(!vsnapMgr.Unmount(srcvol ->VirtualVolumeList, true))
			{
				DebugPrintf(SV_LOG_ERROR,"UnMount failed Vsnap %s. Error @LINE %d in FILE %s\n", 
					vsnapvol ->VolumeName.c_str(),  LINE_NO, FILE_NAME);
				rv = false;
				break;
			}
			ACE_OS::rmdir(getLongPathName(virtualvolumename.c_str()).c_str());

		}while(0);
	} catch (...)	{
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",FUNCTION_NAME);
	return rv;
}

