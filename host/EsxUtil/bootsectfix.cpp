#include "boostsectfix.h"
#include "inmsafecapis.h"

int bootSectFix::fixBootSector(int argc, char *argv[])
{
	struct info i;
	SVERROR sve = SVS_OK;
	int ret_val = 0, errs = 0;
	int quiet = 0, verbose = 0;

	memset(&i, 0, sizeof(struct info));
	ret_val = process_options(argc, argv, &i);
	if(ret_val)
		return ret_val;

	quiet = (i.cli_flags & FLAG_QUIET_MASK);
	verbose = (i.cli_flags & FLAG_VERBOSE_MASK);

	if(i.cli_flags & FLAG_HELP_MASK) {
		print_help();
		return 0;
	}

	sve = OpenVolumeExtended(&i.hdl, i.szVolume, O_RDWR);
	if(sve.failed() || (ACE_INVALID_HANDLE == i.hdl)) {
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: OpenVolumeExtended failed for %s, err = %s\n", i.szVolume, 
            sve.toString());
		return -EINVAL;
	}

	if(gather_info(&i)){
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: Failed to gather all the required information "
				"to validate boot sector\n");
		ret_val = 1;
		goto close_vol;
	}

	if(verbose) {
		DebugPrintf(SV_LOG_INFO,"----------\n");
		DebugPrintf(SV_LOG_INFO,"VOLUME %s\n", i.szVolume);
		DebugPrintf(SV_LOG_INFO,"----------\n");
		dump_bpb(&i.vbs, i.szVolume);
		dump_drive_geometry(&i.disk_geometry, i.szVolume);
		dump_partition_info(&i.part_info, i.szVolume);
		dump_ntfs_info(&i.ntfs_info, i.szVolume);
	}

	if(!quiet)
		errs = print_diag_results(&i);
	
	if(shouldFixCorruptedBootSector) {
		if(i.cli_flags & FLAG_FIX_MASK) {
			if(!errs && !quiet)
				DebugPrintf(SV_LOG_INFO,"\nVolume %s Boot Sector looks good\n", i.szVolume);
			else {
				sve = fix_bpb(&i);
				if(sve.failed())
					ret_val = 1;
				else
					ret_val =0;
			}
		}
	}
	
close_vol:

	if(i.hdl)
		CloseVolume(i.hdl);
	return ret_val;
}


SVERROR bootSectFix::fix_bpb(struct info *i)
{
	int mismatch = 0;
	SVERROR sve = SVS_OK;
	int quiet = (i->cli_flags & FLAG_QUIET_MASK);

	if(i->vbs.bytes_per_sector != i->disk_geometry.BytesPerSector) {
		*(unsigned short *)&i->boot_sector[0x0B] = 
				(unsigned short)i->disk_geometry.BytesPerSector;
		mismatch++;
	}

	if(i->vbs.hidden_sectors != i->part_info.HiddenSectors) {
		*(unsigned int *)&i->boot_sector[0x1C] = i->part_info.HiddenSectors;
		mismatch++;
	}

	if(((i->vbs.total_sectors+1)*i->vbs.bytes_per_sector) != 
			i->part_info.PartitionLength.QuadPart) {
		*(unsigned long long *)&i->boot_sector[0x28] =
				((i->part_info.PartitionLength.QuadPart/i->disk_geometry.BytesPerSector) - 1);
		mismatch++;
	}

	/*
	*	Disk Size				Number of Heads	Sectors per track
	------------------------------------------------------------------------
		< 1GB					64					32
		>= 1GB but < 2GB		128					32
		>= 2GB					255					63

	* Assumption: Since OS volume is goona to be mini 3 GB,we are hardcoding the  Head size to 255
	*/
	if(i->vbs.no_heads != 255)
	{
		*(unsigned short *)&i->boot_sector[0x1A] = 255;
		mismatch++;
	}
	/*
	if(i->vbs.mft_start_cluster != i->ntfs_info.MftStartLcn.QuadPart) {
		*(unsigned long long *)&i->boot_sector[0x30] =
			i->ntfs_info.MftStartLcn;
		mismatch++;
	}

	if(i->vbs.mft2_start_cluster != i->ntfs_info.Mft2StartLcn.QuadPart) {
		*(unsigned long long *)&i->boot_sector[0x38] =
			i->ntfs_info.Mft2StartLcn;
		mismatch++;
	}	
	*/

	if(i->vbs.sectors_per_track != i->disk_geometry.SectorsPerTrack) {
		*(unsigned short *)&i->boot_sector[0x18] =
			(unsigned short)i->disk_geometry.SectorsPerTrack;
		mismatch++;
	}
	
	int disk_sector_per_cluster = (i->ntfs_info.BytesPerCluster/i->disk_geometry.BytesPerSector);
	if( disk_sector_per_cluster !=	i->vbs.sectors_per_cluster) {
		*(unsigned char *)&i->boot_sector[0x0D] = disk_sector_per_cluster;
		mismatch++;
	}
	
	if(!mismatch) {
		if(!quiet)
			DebugPrintf(SV_LOG_INFO,"\nFix: Nothing changed as the volume boot record looks good.\n"); 
		return 0;
	}
	
	do {
	
#ifndef LLSEEK_NOT_SUPPORTED
		if (ACE_OS::llseek(i->hdl, (ACE_LOFF_T)0LL, SEEK_SET) < 0) {
#else
		if (ACE_OS::lseek(i->hdl, (off_t)0, SEEK_SET) < 0) {
#endif
			sve = ACE_OS::last_error();
			if(!quiet)
				DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::llseek failed: vol %s, offset 0, err = %s\n", i->szVolume,
                sve.toString());
			break;
		}
		
		ssize_t bytes_written = 0;
		size_t bytes_to_write = NTFS_BOOT_SECTOR_LENGTH;

		bytes_written = ACE_OS::write(i->hdl, &i->boot_sector[0], bytes_to_write);
		if(bytes_written != bytes_to_write) {
			sve = ACE_OS::last_error();
			if(!quiet)
				DebugPrintf(SV_LOG_ERROR,"Error: boot sector read failed: vol %s, err = %s\n", i->szVolume, \
                sve.toString());
			break;
		} else {
			if(!quiet)
				DebugPrintf(SV_LOG_INFO,"FIx done\n");
		}

		ACE_OS::fsync(i->hdl);

	} while (0);

	return sve;
}

int bootSectFix::print_diag_results(struct info *i)
{
	int mismatch = 0;

	DebugPrintf(SV_LOG_INFO,"\nDiagnosis Results:\n\n");

	if(i->vbs.bytes_per_sector != i->disk_geometry.BytesPerSector) {
		DebugPrintf(SV_LOG_ERROR,"\"Bytes Per Sector\" Mismatch(vbs = %u (0x%x), disk = %lu (0x%lx))\n",
				i->vbs.bytes_per_sector, i->vbs.bytes_per_sector,
                i->disk_geometry.BytesPerSector, i->disk_geometry.BytesPerSector);
		mismatch++;
	}

	if(i->vbs.hidden_sectors != i->part_info.HiddenSectors) {
		DebugPrintf(SV_LOG_ERROR,"\"Hidden Sectors\" Mismatch(vbs = %u (0x%x), disk = %lu (0x%lx))\n",
				i->vbs.hidden_sectors, i->vbs.hidden_sectors, 
                i->part_info.HiddenSectors, i->part_info.HiddenSectors);
		mismatch++;
	}

	if(((i->vbs.total_sectors+1)*i->vbs.bytes_per_sector) != 
			i->part_info.PartitionLength.QuadPart) {
		DebugPrintf(SV_LOG_ERROR,"\"Total Sectors\" Mismatch(vbs = %llu (0x%llx), part = %llu (0x%llx))\n",
				(i->vbs.total_sectors*i->vbs.bytes_per_sector), (i->vbs.total_sectors*i->vbs.bytes_per_sector), 
				i->part_info.PartitionLength.QuadPart, i->part_info.PartitionLength.QuadPart);
	}

	if(i->vbs.mft_start_cluster != i->ntfs_info.MftStartLcn.QuadPart) {
		DebugPrintf(SV_LOG_ERROR,"\"MFTmirr Start\" Mismatch(vbs = %llu (0x%llx), part = %llu (0x%llx))\n",
				i->vbs.mft_start_cluster, i->vbs.mft_start_cluster, 
                i->ntfs_info.MftStartLcn.QuadPart, i->ntfs_info.MftStartLcn.QuadPart);
		mismatch++;
	}

	if(i->vbs.mft2_start_cluster != i->ntfs_info.Mft2StartLcn.QuadPart) {
		DebugPrintf(SV_LOG_ERROR,"\"MFTmirr Start\" Mismatch(vbs = %llu (0x%llx), part = %llu (0x%llx))\n",
				i->vbs.mft2_start_cluster, i->vbs.mft2_start_cluster,
                i->ntfs_info.Mft2StartLcn.QuadPart, i->ntfs_info.Mft2StartLcn.QuadPart);
		mismatch++;
	}	

	if(i->vbs.sectors_per_track != i->disk_geometry.SectorsPerTrack) {
		DebugPrintf(SV_LOG_ERROR,"\"Sectors Per Track\" Mismatch(vbs = %u (0x%x), disk = %lu (0x%lx))\n",
				i->vbs.sectors_per_track, i->vbs.sectors_per_track,
                i->disk_geometry.SectorsPerTrack, i->disk_geometry.SectorsPerTrack);
		mismatch++;
	}
	
	
	int disk_sector_per_cluster = (i->ntfs_info.BytesPerCluster/i->disk_geometry.BytesPerSector);
	if( disk_sector_per_cluster !=	i->vbs.sectors_per_cluster) {
		DebugPrintf(SV_LOG_ERROR,"\"Sectors Per Cluster\" Mismatch(vbs = %u (0x%x), disk = %d (0x%x))\n",
				i->vbs.sectors_per_cluster, i->vbs.sectors_per_cluster, 
                disk_sector_per_cluster, disk_sector_per_cluster);
		mismatch++;
	}

    if( i->vbs.no_heads != i->disk_geometry.TracksPerCylinder ) {
        DebugPrintf(SV_LOG_ERROR,"\"Heads Per Cylinder\" Mismatch(vbs = %u (0x%x), disk = %lu (0x%lx))\n",
            i->vbs.no_heads, i->vbs.no_heads,
            i->disk_geometry.TracksPerCylinder, i->disk_geometry.TracksPerCylinder                       
            );
        mismatch++;
    }
	
	if(!mismatch) {
		DebugPrintf(SV_LOG_INFO,"No Errors found in volume boot sector of %s\n", i->szVolume);
	}

	return mismatch;
}


int bootSectFix::gather_info(struct info *i)
{
	int quiet = (i->cli_flags & FLAG_QUIET_MASK);

	if(get_bpb(i->hdl, i->szVolume, i->boot_sector, &i->vbs, quiet)) {
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: Error dumping boot parameter block of %s\n", i->szVolume);
		return 1;
	}

	if(get_drive_geometry(i->hdl, i->szVolume, &i->disk_geometry, quiet)) {
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: Failed to get driver geometry of %s\n", i->szVolume);
		return 1;
	}

	if(get_partition_info(i->hdl, i->szVolume, &i->part_info, quiet)) {
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: Failed to get partition info of %s\n", i->szVolume);
		return 1;
	}

	if(get_ntfs_info(i->hdl, i->szVolume, &i->ntfs_info, quiet)) {
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: Failed to dump NTFS info for %s\n", i->szVolume);
		return 1;
	}

	return 0;
}

int bootSectFix::process_options(int argc, char *argv[], struct info *i)
{
	int idx;

	memset(i->szVolume, 0, MAX_PATH);
	if(argc == 0) {
		i->cli_flags |= FLAG_HELP_MASK;
		return 0;
	}

	idx = 0;
	while(idx < argc) {
		if(argv[idx][0] != '-') {
			if(i->szVolume[0] == '\0') {
                inm_strcpy_s(i->szVolume, ARRAYSIZE(i->szVolume), argv[idx]);
			} else {
				DebugPrintf(SV_LOG_ERROR,"Invalid Argument, use -h for help\n");
				return -EINVAL;
			}
		} else if(argv[idx][1] == 'v') {
			i->cli_flags |= FLAG_VERBOSE_MASK;
		} else if(argv[idx][1] == 'f') {
			i->cli_flags |= FLAG_FIX_MASK;
		} else if(argv[idx][1] == 'q') {
			i->cli_flags |= FLAG_QUIET_MASK;
		} else if(argv[idx][1] == 'h') {
			i->cli_flags |= FLAG_HELP_MASK;
			return 0;
		} else {
			DebugPrintf(SV_LOG_ERROR,"Invalid Argument, use -h for help\n");
			return -EINVAL;
		}
		idx++;
	}

	if( (i->cli_flags & FLAG_QUIET_MASK ) &&
		(i->cli_flags & FLAG_VERBOSE_MASK) ) {
		DebugPrintf(SV_LOG_ERROR,"Error: Can't use -q and -v simultaneously\n");
		return -EINVAL;
	}

	if(i->szVolume[0] == '\0') {
		DebugPrintf(SV_LOG_ERROR,"Error: Volume is not specified. use -h for help\n");
		return -EINVAL;
	}

	if(validate_volume(i->szVolume)) {
		if(!(i->cli_flags & FLAG_QUIET_MASK))
			DebugPrintf(SV_LOG_ERROR,"Error: Invalid volume specified\n");
		return -EINVAL;
	}

	return 0;
}

int  bootSectFix::validate_volume(char *szVol)
{
	if(!IsDrive(szVol)) {
		if(!IsMountPoint(szVol)) {
			return true;
		}
		else
		{
			return false;
		}
		return true;
	}
	return false;
}

int bootSectFix::dump_drive_geometry(DISK_GEOMETRY *disk_geometry, char *szVol)
{
	DebugPrintf(SV_LOG_INFO,"\nDrive Geometry of %s\n", szVol);
	DebugPrintf(SV_LOG_INFO,"---------------------\n\n");
	DebugPrintf(SV_LOG_INFO,"Cylinders     : %llu (0x%llx)\n"
		   "Media Type    : %u (0x%x)\n"
		   "Heads/Cyl     : %lu (0x%lx)\n" //Tracks Per Cylinder = Heads Per Cylinder
		   "Sectors/track : %lu (0x%lx)\n"
		   "Bytes/Sector  : %lu (0x%lx)\n",
			disk_geometry->Cylinders.QuadPart, disk_geometry->Cylinders.QuadPart,
            disk_geometry->MediaType, disk_geometry->MediaType, 
			disk_geometry->TracksPerCylinder, disk_geometry->TracksPerCylinder,
            disk_geometry->SectorsPerTrack, disk_geometry->SectorsPerTrack,
			disk_geometry->BytesPerSector, disk_geometry->BytesPerSector);

	return 0;
}


int bootSectFix::get_drive_geometry(ACE_HANDLE hdl, char *szVol, DISK_GEOMETRY *disk_geometry,
					   int quiet)
{
	SVERROR sve = SVS_OK;
	DWORD cbReturned = 0;
	
	if( 0 == DeviceIoControl( hdl, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL,
				0, disk_geometry, sizeof( DISK_GEOMETRY ), &cbReturned, NULL ) ) {
		sve = ACE_OS::last_error();
		HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: FAILED DeviceIoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY)... hr = %08X\n", hr );
		return 1;
	}
	return 0;
}

int bootSectFix::dump_ntfs_info(NTFS_VOLUME_DATA_BUFFER *ntfs_info, char *szVol)
{
	DebugPrintf(SV_LOG_INFO,"\nNTFS Info of %s\n", szVol);
	DebugPrintf(SV_LOG_INFO,"---------------------\n");
	DebugPrintf(SV_LOG_INFO,"Serial No          : %llu (0x%llx)\n"
		   "No. Of Sectors     : %llu (0x%llx)\n"
		   "Total Clusters     : %llu (0x%llx)\n"
		   "Free Clusters      : %llu (0x%llx)\n"
		   "Total Rsvd         : %llu (0x%llx)\n"
		   "Bytes Per Sector   : %lu (0x%lx)\n"
		   "Bytes Per Cluster  : %lu (0x%lx)\n"
		   "ByterPerFRS        : %lu (0x%lx)\n"
		   "ClusterPerFRS      : %lu (0x%lx)\n"
		   "MFTValidDataLength : %llu (0x%llx)\n"
		   "MFTStartLen        : %llu (0x%llx)\n"
		   "MFT2StartLen       : %llu (0x%llx)\n"
		   "MFTZoneStart       : %llu (0x%llx)\n"
		   "MFTZoneEnd         : %llu (0x%llx)\n",
			ntfs_info->VolumeSerialNumber, ntfs_info->VolumeSerialNumber, 
            ntfs_info->NumberSectors, ntfs_info->NumberSectors, 
            ntfs_info->TotalClusters, ntfs_info->TotalClusters,
			ntfs_info->FreeClusters, ntfs_info->FreeClusters, 
            ntfs_info->TotalReserved, ntfs_info->TotalReserved, 
            ntfs_info->BytesPerSector, ntfs_info->BytesPerSector,
			ntfs_info->BytesPerCluster, ntfs_info->BytesPerCluster, 
            ntfs_info->BytesPerFileRecordSegment, ntfs_info->BytesPerFileRecordSegment, 
            ntfs_info->ClustersPerFileRecordSegment, ntfs_info->ClustersPerFileRecordSegment,
            ntfs_info->MftValidDataLength.QuadPart, ntfs_info->MftValidDataLength.QuadPart, 
            ntfs_info->MftStartLcn.QuadPart, ntfs_info->MftStartLcn.QuadPart, 
            ntfs_info->Mft2StartLcn.QuadPart, ntfs_info->Mft2StartLcn.QuadPart,
			ntfs_info->MftZoneStart.QuadPart, ntfs_info->MftZoneStart.QuadPart, 
            ntfs_info->MftZoneEnd.QuadPart, ntfs_info->MftZoneEnd.QuadPart);

	return 0;
}

void bootSectFix::print_help()
{
	DebugPrintf(SV_LOG_INFO,"Fixes \"Boot Parameter Block\" of the boot sector of a NTFS volume\n\n");
	DebugPrintf(SV_LOG_INFO,"USAGE: [-info] [-f] [-q] volume\n\n");
	DebugPrintf(SV_LOG_INFO,"  -info\t Verbose Mode\n");
	DebugPrintf(SV_LOG_INFO,"  -f\t Fixes boot parameter block if any mismatch found\n");
	DebugPrintf(SV_LOG_INFO,"  -q\t Quiet mode(for use in scripts, returns 0 on success and non-zero\n"
		   "    \t otherwise)\n");
	DebugPrintf(SV_LOG_INFO,"  -h\t Print this help message\n");
}

//bool IsDrive(const std::string& sDrive)
//{
//    if ( (sDrive.length() <= 3 ) && ( isalpha(sDrive[0])) )
//        return true;
//    else if ( sDrive.length() > 3 ) 
//    {
//        if (    ('\\' == sDrive[0])   && 
//            ('\\' == sDrive[1])    && 
//            ( ( sDrive[2] == '?') || ( sDrive[2] == '.')) && 
//            (sDrive[5] == ':' ) && 
//            (isalpha(sDrive[4])))
//            return true;                   
//    }
//    return false;
//}
//
//
/** PARTED */
//bool FormatVolumeNameToGuid(std::string& sVolumeName)
//{
//    //
//    // Given a mount point, return \\?\Volume{guid} as the "unformatted" name
//    //
//    std::string sName = sVolumeName;
//   // if (!IsVolumeNameInGuidFormat(sName))
// //   {
//        char szVolumeName[ SV_MAX_PATH ];
//        std::string mountPoint = "";
//        if ( IsMountPoint(sName) )
//        {
//            if ( sName[sName.length() - 1] == '\\')
//                sName.erase(sName.end() - 1);
//        }
//        else if (IsDrive(sName))
//        {
//            UnformatVolumeNameToDriveLetter(sName);
//            sName += ":";
//        }
//        else
//        {
////           DebugPrintf(SV_LOG_ERROR,"volume name %s is neither drive nor a mountpoint.\n",sVolumeName.c_str());
//            return false;
//        }
//
//        mountPoint = sName + "\\";
//        if( GetVolumeNameForVolumeMountPointA( mountPoint.c_str(), szVolumeName, sizeof( szVolumeName ) ) )
//        {
//            sName = std::string( szVolumeName );
//
//            if( sName.length() > 0 )
//            {
//                //
//                // Remove trailing \ because CreateFile() expects it in that format
//                //
//                if ( sName[sName.length() - 1] == '\\')
//                    sName.erase( sName.end() - 1 );
//            }
//            sVolumeName = sName;
//        }
//        else
//        {
//            szVolumeName[0] = 0; 
//            return false;   
//        }
////    }        
//    
//    return true;
//}
//
//
/* This function retrieves/dumps boot parameter block of the boot sector
 */
int bootSectFix::get_bpb(ACE_HANDLE hdl, char *szVol, char *boot_sector, struct vbs_info *vbs, int quiet)
{
	SVERROR sve = SVS_OK;
	int bytes_to_read = NTFS_BOOT_SECTOR_LENGTH, bytes_read = 0, cur_offset = 0;
		
	do {
	
#ifndef LLSEEK_NOT_SUPPORTED
		if (ACE_OS::llseek(hdl, (ACE_LOFF_T)0LL, SEEK_SET) < 0) {
#else
		if (ACE_OS::lseek(hdl, (off_t)0, SEEK_SET) < 0) {
#endif
			sve = ACE_OS::last_error();
			if(!quiet)
                DebugPrintf(SV_LOG_ERROR, "Error: ACE_OS::llseek failed: vol %s, offset 0, err = %s\n", szVol, sve.toString());
			break;
		}

		bytes_read = ACE_OS::read(hdl, boot_sector, bytes_to_read);
		if(bytes_read != bytes_to_read) {
			sve = ACE_OS::last_error();
			if(!quiet)
                DebugPrintf(SV_LOG_ERROR, "Error: boot sector read failed: vol %s, err = %s\n", szVol, sve.toString());
			break;
		}
		
		// Check if this is NTFS boot sector
		if(!(boot_sector[3] == 'N' && boot_sector[4] == 'T' && 
				boot_sector[5] == 'F' && boot_sector[6] == 'S')) {
				if(!quiet)
					DebugPrintf(SV_LOG_ERROR,"Error: Vol %s is not an NTFS volume\n", szVol);
				break;
		}

		vbs->bytes_per_sector = *(unsigned short *)&boot_sector[0x0B];
		vbs->sectors_per_cluster = *(unsigned char *)&boot_sector[0x0D];
		vbs->sectors_per_track = *(unsigned short *)&boot_sector[0x18];
		vbs->no_heads = *(unsigned short *)&boot_sector[0x1A];
		vbs->hidden_sectors = *(unsigned int *)&boot_sector[0x1C];
		vbs->total_sectors = *(unsigned long long *)&boot_sector[0x28];
		vbs->mft_start_cluster = *(unsigned long long *)&boot_sector[0x30];
		vbs->mft2_start_cluster = *(unsigned long long *)&boot_sector[0x38];
		vbs->cluster_per_frs = *(unsigned int *)&boot_sector[0x40];
		vbs->cluster_per_idx = *(unsigned int *)&boot_sector[0x44];
		vbs->serial_number = *(unsigned long long *)&boot_sector[0x48];
	} while(0);

	if(sve.failed())
		return 1;

	return 0;
}
//SVERROR OpenVolumeExtended( /* out */ ACE_HANDLE *pHandleVolume,
//                           /* in */  const char   *pch_VolumeName,
//                           /* in */  int  openMode
//                           )
//{
//    SVERROR sve = SVS_OK;
//    char *pch_DestVolume = NULL;
//
//    do
//    {
//       /* DebugPrintf( SV_LOG_DEBUG, "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
//        DebugPrintf( SV_LOG_DEBUG, "ENTERED OpenVolumeExtended()...\n" );
//        DebugPrintf( SV_LOG_DEBUG, "OpenVolumeExtended::Volume is %s.\n",pch_VolumeName );
//*/
//        if( ( NULL == pHandleVolume ) ||
//            ( NULL == pch_VolumeName ) )
//        {
//            sve = EINVAL;
//         //   DebugPrintf( SV_LOG_DEBUG, "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
//         //   DebugPrintf( SV_LOG_ERROR, "FAILED OpenVolumeExtended()... sve = EINVAL\n");
//            break;
//        }
//
//        if (ACE_BIT_ENABLED (openMode, O_WRONLY))
//        {
//            TCHAR rgtszSystemDirectory[ MAX_PATH + 1 ];
//            if( 0 == GetSystemDirectory( rgtszSystemDirectory, MAX_PATH + 1 ) )
//            {
//                sve = HRESULT_FROM_WIN32( GetLastError() );
//              //  DebugPrintf( SV_LOG_DEBUG, "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
//            //    DebugPrintf( SV_LOG_ERROR, "FAILED GetSystemDirectory()... err = %s\n", sve.toString() );
//                break;
//            }
//            if(IsDrive(pch_VolumeName))
//            {
//                if( toupper( pch_VolumeName[ 0 ] ) == toupper( rgtszSystemDirectory[ 0 ] ) )
//                {
//                    sve = EINVAL;
//              //      DebugPrintf( SV_LOG_DEBUG, "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
//              //      DebugPrintf( SV_LOG_ERROR, "FAILED: Attempted to Write to System Directory... sve = EINVAL\n");
//                    break;
//                }
//            }
//        }
//
//        std::string DestVolName = pch_VolumeName;
//        FormatVolumeNameToGuid(DestVolName);		    
//
//      //  DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
//      //  DebugPrintf( "CALLING CreateFile()...\n" );
//
//		ACE_OS::last_error(0);
//		// PR#10815: Long Path support
//		*pHandleVolume = ACE_OS::open(DestVolName.c_str(), openMode, ACE_DEFAULT_OPEN_PERMS); 
//
//		std::ostringstream errmsg;
//		std::string errstr;
//        if( ACE_INVALID_HANDLE == *pHandleVolume )
//        {
//        	sve = EINVAL;
//            errstr = ACE_OS::strerror(ACE_OS::last_error());
//			errmsg.clear();
//			errmsg << DestVolName << " - " << errstr << std::endl;
//		//	DebugPrintf(SV_LOG_DEBUG, "%s\n", errmsg.str().c_str());
//            break;
//        }
//
//    }
//    while( FALSE );
//
//    return( sve );
//}
//
int bootSectFix::dump_bpb(struct vbs_info *vbs, char *szVol)
{
	DebugPrintf(SV_LOG_INFO,"\nVolume Boot Sector Dump for %s\n", szVol);
	DebugPrintf(SV_LOG_INFO,"--------------------------------\n\n");
	DebugPrintf(SV_LOG_INFO,"bytes per sector      = %u (0x%x)\n", vbs->bytes_per_sector, vbs->bytes_per_sector);
	DebugPrintf(SV_LOG_INFO,"sectors per cluster   = %u (0x%x)\n", vbs->sectors_per_cluster, vbs->sectors_per_cluster);
	DebugPrintf(SV_LOG_INFO,"Sectors per track     = %u (0x%x)\n", vbs->sectors_per_track, vbs->sectors_per_track);
	DebugPrintf(SV_LOG_INFO,"Heads per Cylinder    = %u (0x%x)\n", vbs->no_heads, vbs->no_heads);
	DebugPrintf(SV_LOG_INFO,"Hidden Sectors        = %u (0x%x)\n", vbs->hidden_sectors, vbs->hidden_sectors);
	DebugPrintf(SV_LOG_INFO,"Total Sectors         = %llu (0x%llx)\n", vbs->total_sectors, vbs->total_sectors);
	DebugPrintf(SV_LOG_INFO,"MFT Start Cluster     = %llu (0x%llx)\n", vbs->mft_start_cluster, vbs->mft_start_cluster);
	DebugPrintf(SV_LOG_INFO,"MFTmirr Start Cluster = %llu (0x%llx)\n", vbs->mft2_start_cluster, vbs->mft2_start_cluster);
	DebugPrintf(SV_LOG_INFO,"Cluster Per FRS       = %u (0x%x)\n", vbs->cluster_per_frs, vbs->cluster_per_frs);
	DebugPrintf(SV_LOG_INFO,"Cluster Per Index     = %u (0x%x)\n", vbs->cluster_per_idx, vbs->cluster_per_idx);
	DebugPrintf(SV_LOG_INFO,"Volume Serial Number  = %llu (0x%llx)\n", vbs->serial_number, vbs->serial_number);

	return 0;
}

int bootSectFix::get_partition_info(ACE_HANDLE hdl, char *szVol, PARTITION_INFORMATION *part_info,
					   int quiet)
{
	SVERROR sve = SVS_OK;
	DWORD cbReturned = 0;
	
	if( 0 == DeviceIoControl( hdl, IOCTL_DISK_GET_PARTITION_INFO, NULL,
				0, part_info, sizeof( PARTITION_INFORMATION), &cbReturned, NULL ) ) {
		sve = ACE_OS::last_error();
		HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: FAILED DeviceIoControl(IOCTL_DISK_GET_PARTITION_INFO)... hr = %08X\n", hr );
		return 1;
	}

	return 0;
}

int bootSectFix::dump_partition_info(PARTITION_INFORMATION *part_info, char *szVol)
{
	DebugPrintf(SV_LOG_INFO,"\nPartition Info of %s\n", szVol);
	DebugPrintf(SV_LOG_INFO,"---------------------\n");
	DebugPrintf(SV_LOG_INFO,"Starting Offset  : %llu (0x%llx)\n"
		   "Partition Length : %llu (0x%llx)\n"
		   "Hidden Sectors   : %lu (0x%lx)\n"
		   "Part No          : %lu (0x%lx)\n"
		   "Part Type        : %u (0x%x)\n" 
		   "BI               : %d\n"
		   "RP               : %d\n"
		   "RWP              : %d\n",
		part_info->StartingOffset.QuadPart, part_info->StartingOffset.QuadPart, 
        part_info->PartitionLength.QuadPart, part_info->PartitionLength.QuadPart, 
        part_info->HiddenSectors, part_info->HiddenSectors,
		part_info->PartitionNumber, part_info->PartitionNumber, 
        part_info->PartitionType, part_info->PartitionType, 
        part_info->BootIndicator,
		part_info->RecognizedPartition, 
        part_info->RewritePartition);

	return 0;
}

int bootSectFix::get_ntfs_info(ACE_HANDLE hdl, char *szVol, NTFS_VOLUME_DATA_BUFFER *ntfs_info,
				  int quiet)
{
	SVERROR sve = SVS_OK;
	DWORD cbReturned = 0;
	
	if( 0 == DeviceIoControl( hdl, FSCTL_GET_NTFS_VOLUME_DATA, NULL,
				0, ntfs_info, sizeof( NTFS_VOLUME_DATA_BUFFER), &cbReturned, NULL ) ) {
		sve = ACE_OS::last_error();
		HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
		if(!quiet)
			DebugPrintf(SV_LOG_ERROR,"Error: FAILED DeviceIoControl(FSCTL_GET_NTFS_VOLUME_DATA)... hr = %08X\n", hr );
		return 1;
	}



	return 0;
}