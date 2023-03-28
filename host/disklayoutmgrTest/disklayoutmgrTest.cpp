#include <iostream>
using namespace std;

#include <string>
#include <map>
#include <vector>

#include <ace/OS.h>
#include <ace/OS_main.h>
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Process.h>
#include <ace/Process_Manager.h>

#include "svtypes.h"
#include "portablehelpers.h"
#include "localconfigurator.h"
#include "logger.h"
#include "inmsafecapis.h"

#include "dlmapi.h"

#ifndef WIN32
#define Sleep(n) sleep(n/1000)
#endif

#ifdef WIN32

    #include <windows.h>
    #include <Winioctl.h>
    #include <atlconv.h>
    #include <Wbemidl.h>

    #pragma comment(lib, "wbemuuid.lib")

    HRESULT InitCOM()
    {
	    printf("Entering %s\n",__FUNCTION__);
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED); 
	    if (FAILED(hres))
        {
	       printf("Failed to initialize COM library. Error Code %0X\n",hres);
           return S_FALSE;        
        }
         hres = CoInitializeSecurity(
			    NULL, 
			    -1,                          // COM authentication
			    NULL,                        // Authentication services
			    NULL,                        // Reserved
			    RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
			    RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
			    NULL,                        // Authentication info
			    EOAC_NONE,                   // Additional capabilities 
			    NULL                         // Reserved
			    );

         if (hres == RPC_E_TOO_LATE)
         {
             hres = S_OK;
         }
        if (FAILED(hres))
        {
            printf("Failed to initialize security  library. Error Code %0X\n",hres);
    //        CoUninitialize();
		    printf("Exiting %s\n",__FUNCTION__);
            return S_FALSE;                    // Program has failed.
        }
        printf("Exiting %s\n",__FUNCTION__);
        return S_OK;
    }

#endif  // WIN32

//*************************************************************************************************
// This function read all disks info to a file in binary format 
// and generate map of disknames to diskinfo
// And display the info on screen.
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE ReadDiskInfoMapFromFileAndDisplay(const char * FileName, DisksInfoMap_t & d)
{   
    printf("Entering %s\n",__FUNCTION__);     
    d.clear();

    //open the file in binary mode, read mode
    FILE *pFileRead = fopen(FileName,"rb");
    if(!pFileRead)
	{
        printf("Failed to open the file - %s.\n", FileName);
		printf("Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }      

    //Read Number of disks first.
    SV_UINT DiskCount = 0;
    if(! fread(&(DiskCount), sizeof(DiskCount), 1, pFileRead))
    {
        fclose(pFileRead);
        printf("Failed to read DiskCount data from binary file-%s\n", FileName);
	    printf("Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_READ;
    }

    printf("Disk Count = %u\n", DiskCount);
    //Read all disks info and generate the map.
    for(SV_UINT i = 0; i < DiskCount; i++)
    {
        printf("--------------------------------------------------------------\n");

        DISK_INFO DiObj;
        DISK_INFO_SUB DiSubObj;
        
        if(! fread(&(DiSubObj), sizeof(DiSubObj), 1, pFileRead))
        {
            fclose(pFileRead);
            printf("Failed to read DISK_INFO_SUB data of a disk from binary file-%s\n", FileName);
		    printf("Exiting %s\n",__FUNCTION__);
            return DLM_ERR_FILE_READ;
        }       
        DiObj.DiskInfoSub = DiSubObj; //Add DiskInfoSub to DISK_INFO obj

        printf("Name            = %s\n", DiSubObj.Name);
        printf("BytesPerSector  = %llu\n", DiSubObj.BytesPerSector);
        printf("EbrCount        = %llu\n", DiSubObj.EbrCount);
        printf("Flag            = %llu\n", DiSubObj.Flag);
        printf("FormatType      = %u\n", DiSubObj.FormatType);
        printf("ScsiId          = %u:%u:%u:%u\n", DiSubObj.ScsiId.Host, DiSubObj.ScsiId.Channel, DiSubObj.ScsiId.Target, DiSubObj.ScsiId.Lun );
        printf("Size            = %lld\n", DiSubObj.Size);
        printf("Type            = %u\n", DiSubObj.Type);
        printf("VolumeCount     = %llu\n", DiSubObj.VolumeCount);

        if(RAWDISK != DiSubObj.FormatType)
        {  
            if(0 != DiSubObj.VolumeCount)
            {
                //read vol info of all  volumes
                VolumesInfoVec_t VolumesInfo;
                for(SV_ULONGLONG i = 0; i < DiSubObj.VolumeCount; i++)
                {
                    VOLUME_INFO VolumeInfo;
                    if(! fread(&(VolumeInfo), sizeof(VolumeInfo), 1, pFileRead))
                    {
                        fclose(pFileRead);
                        printf("Failed to read VOLUME_INFO data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                        printf("Exiting %s\n",__FUNCTION__);
                        return DLM_ERR_FILE_READ;
                    }
                    VolumesInfo.push_back(VolumeInfo);
                    printf("-----------------------------\n");                    
                    printf("VolumeName      = %s\n",VolumeInfo.VolumeName);
                    printf("VolumeLength    = %lld\n",VolumeInfo.VolumeLength);
                    printf("StartingOffset  = %lld\n",VolumeInfo.StartingOffset);
                    printf("EndingOffset    = %lld\n",VolumeInfo.EndingOffset);
                }
                DiObj.VolumesInfo = VolumesInfo; //Add VolumesInfo to DISK_INFO obj
            } 

            if(MBR == DiSubObj.FormatType)
            {
                SV_UCHAR MbrSector[MBR_BOOT_SECTOR_LENGTH];
                if(! fread(&(MbrSector), sizeof(MbrSector), 1, pFileRead))
                {
                    fclose(pFileRead);
                    printf("Failed to read MBR data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                printf("Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
                }
                //Add MbrSector to DISK_INFO obj
                for(int i=0; i<MBR_BOOT_SECTOR_LENGTH; i++)
                {
                    DiObj.MbrSector[i] = MbrSector[i];
                }

                if(0 != DiSubObj.EbrCount)
                {
                    //read all EBRs
                    std::vector<EBR_SECTOR> EbrSectors;                
                    for(SV_ULONGLONG i = 0; i < DiSubObj.EbrCount; i++)
                    {  
                        //SV_UCHAR EbrSector[MBR_BOOT_SECTOR_LENGTH];
                        EBR_SECTOR es;
                        if(! fread(&(es.EbrSector), sizeof(es.EbrSector), 1, pFileRead))
                        {
                            fclose(pFileRead);
                            printf("Failed to read EBR data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                            printf("Exiting %s\n",__FUNCTION__);
                            return DLM_ERR_FILE_READ;
                        }
                        //EbrSectors.push_back(EbrSector); 
                        EbrSectors.push_back(es); 
                    }
                    DiObj.EbrSectors = EbrSectors; //Add EbrSectors to DISK_INFO obj
                }
            }
            else if(GPT == DiSubObj.FormatType)
            {
                SV_UCHAR GptSector[GPT_BOOT_SECTOR_LENGTH];
                if(! fread(&(GptSector), sizeof(GptSector), 1, pFileRead))
                {
                    fclose(pFileRead);
                    printf("Failed to read GPT data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                printf("Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
                }
                //Add GptSector to DISK_INFO obj
                for(int i=0; i<GPT_BOOT_SECTOR_LENGTH; i++)
                {
                    DiObj.GptSector[i] = GptSector[i];
                }
            }   
        }
        
        //char * DiskName;
        //DiskName = new char [MAX_PATH_COMMON + 1];
        //DiskName[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1

        //Add the diskinfo obj to map of disknames to diskinfo
        d[std::string(DiSubObj.Name)] = DiObj;
    }
    fclose(pFileRead);

    printf("Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

int main(int argc, char *argv[])
{
    init_inm_safe_c_apis();
    DisksInfoMap_t d;
    char filename[] = "DiskInfo.bin";

    if(argc == 1)
    {
        cout<<"Usage - "<<argv[0]<<"[store | restore]"<<endl;
        return 0;
    }
 
    if(strcmp(argv[1],"store") == 0)
    {
        #ifdef WIN32
            if(S_FALSE == InitCOM())
	        {
		        printf("Failed to initialize COM.\n");
		        printf("Exiting %s\n",__FUNCTION__);
		        return false; 
	        }
        #endif
        // Call this first time to store the diskinfo file..and use this for remanining apis.
        // Hence comment this binary when using for other apis.
        // API - 1 : StoreDisksInfo
        cout<<"\n**************API - 1 : StoreDisksInfo**************\n";
        DeleteFile(filename);

        std::list<std::string> erraticVolumeGuids;
        //StoreDisksInfo(filename, erraticVolumeGuids);
		std::list<std::string> uploadFiles;
		DLM_ERROR_CODE RetVal = StoreDisksInfo(filename, uploadFiles, erraticVolumeGuids, "dlm_normal");
		if(!erraticVolumeGuids.empty())
		{
			DebugPrintf(SV_LOG_INFO, "Some Failed state Volumes are : \n");
			std::list<std::string>::iterator iter = erraticVolumeGuids.begin();
			for(; iter != erraticVolumeGuids.end(); iter++)
			{
				DebugPrintf(SV_LOG_INFO, "\t%s\n", iter->c_str());
			}
		}
		if((DLM_FILE_CREATED == RetVal) || (DLM_FILE_NOT_CREATED == RetVal))
		{
			std::list<std::string>::iterator iter = uploadFiles.begin();
			DebugPrintf(SV_LOG_INFO, "File need to upload in CS are : \n");
			for(; iter != uploadFiles.end(); iter++)
			{
				DebugPrintf(SV_LOG_INFO, "\t%s\n", iter->c_str());
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to collect the DLM MBR file %s. Error Code - %u\n", filename, RetVal);
		}

        ReadDiskInfoMapFromFileAndDisplay(filename, d);
    }
    else if(strcmp(argv[1],"restore") == 0)
    {
        #ifdef WIN32
            if(S_FALSE == InitCOM())
	        {
		        printf("Failed to initialize COM.\n");
		        printf("Exiting %s\n",__FUNCTION__);
		        return false; 
	        }
        #endif

        //API - 2 : CollectDisksInfoAndConvert
        cout<<"\n\n\n**************API - 2 : CollectDisksInfoAndConvert**************\n";
        map<DiskName_t, DISK_INFO> SrcMap;
        map<DiskName_t, DISK_INFO> TgtMap;
        CollectDisksInfoAndConvert(filename, SrcMap, TgtMap);
        cout<<"SrcMap Count = "<<SrcMap.size()<<endl;
        cout<<"TgtMap Count = "<<TgtMap.size()<<endl;
        map<std::string, DISK_INFO>::iterator iter;    
        cout<<"Source Map Info - \n";
        for(iter = SrcMap.begin();iter!=SrcMap.end();iter++)
        {
            cout<<iter->first<<" ==> ";
            vector<VOLUME_INFO>::iterator temp_iter = iter->second.VolumesInfo.begin();
            for(; temp_iter != iter->second.VolumesInfo.end(); temp_iter++)
                cout<<temp_iter->VolumeName<<" "; 
            cout<<endl;
        }
        cout<<endl;    
        cout<<"Target Map Info - \n";
        for(iter = TgtMap.begin();iter!=TgtMap.end();iter++)
        {
            cout<<iter->first<<" ==> ";
            vector<VOLUME_INFO>::iterator temp_iter = iter->second.VolumesInfo.begin();
            for(; temp_iter != iter->second.VolumesInfo.end(); temp_iter++)
                cout<<temp_iter->VolumeName<<" ";        
            cout<<endl;
        }
        cout<<endl;


        //API - 3 : GetCorruptedDisks
        cout<<"\n\n\n**************API - 3 : GetCorruptedDisks**************\n";
        vector<DiskName_t> CorruptedSrcDiskNames;
        map<DiskName_t, vector<DiskName_t> > MapSrcDisksToTgtDisks;
        GetCorruptedDisks (SrcMap, TgtMap, CorruptedSrcDiskNames, MapSrcDisksToTgtDisks);
        cout<<"Corrupted Source Disk Names - \n";
        vector<DiskName_t>::iterator temp_iter = CorruptedSrcDiskNames.begin();
        for(; temp_iter != CorruptedSrcDiskNames.end(); temp_iter++)
        {
            cout<<*temp_iter<<endl;
        }
        cout<<endl;
        cout<<"Map of Source disks to its corresponding possible target disks - \n";
        map<DiskName_t, vector<DiskName_t> >::iterator map_iter = MapSrcDisksToTgtDisks.begin();
        for(; map_iter != MapSrcDisksToTgtDisks.end(); map_iter++)
        {
            cout<<map_iter->first<<" ==> ";
            vector<DiskName_t>::iterator vec_iter = map_iter->second.begin();
            for(; vec_iter != map_iter->second.end(); vec_iter++)
            {
                cout<<*vec_iter<<";";
            }
            cout<<endl;
        }
        cout<<endl;
       

        ////API-4 : RestoreDiskStructure
        cout<<"\n\n\n**************API-4 : RestoreDiskStructure**************\n";  
        map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames;
        //MapSrcToTgtDiskNames["\\\\.\\PHYSICALDRIVE0"] = string("\\\\.\\PHYSICALDRIVE0");
        MapSrcToTgtDiskNames["\\\\.\\PHYSICALDRIVE1"] = string("\\\\.\\PHYSICALDRIVE1");
        MapSrcToTgtDiskNames["\\\\.\\PHYSICALDRIVE2"] = string("\\\\.\\PHYSICALDRIVE2");
        MapSrcToTgtDiskNames["\\\\.\\PHYSICALDRIVE3"] = string("\\\\.\\PHYSICALDRIVE3");
        MapSrcToTgtDiskNames["\\\\.\\PHYSICALDRIVE4"] = string("\\\\.\\PHYSICALDRIVE4");
        /*vector<DiskName_t> RestoredSrcDiskNames;
        RestoreDiskStructure (SrcMap, TgtMap, MapSrcToTgtDiskNames, RestoredSrcDiskNames);                                   
        cout<<"Source disks that are restored successfully - \n";
        vector<DiskName_t>::iterator vec_iter = RestoredSrcDiskNames.begin();
        for(; vec_iter != RestoredSrcDiskNames.end(); vec_iter++)
        {
            cout<<*vec_iter<<"\n";
        }
        cout<<endl;


        cout<<"\n\nSleeping 10 seconds so that volumes will be updated with OS.\n";
        Sleep(10000);*/

        ////Build a map of source and target disks...we know for now only 1 disk per source is there. so going with it.
        //map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames;
        //map_iter = MapSrcDisksToTgtDisks.begin();
        //for(; map_iter != MapSrcDisksToTgtDisks.end(); map_iter++)
        //{        
        //    vector<DiskName_t>::iterator vec_iter = map_iter->second.begin();     
        //    MapSrcToTgtDiskNames[map_iter->first] = *vec_iter;
        //}

        cout<<"RestoreVolumeMountPoints = "<<RestoreVolumeMountPoints(SrcMap, MapSrcToTgtDiskNames)<<endl;

        //API-5 : GetMapSrcToTgtVol
        cout<<"\n\n\n**************API-5 : GetMapSrcToTgtVol**************\n";
        map<VolumeName_t, VolumeName_t> MapSrcToTgtVolumeNames;
        GetMapSrcToTgtVol (SrcMap, MapSrcToTgtDiskNames, MapSrcToTgtVolumeNames);                                 
        cout<<"Map of source to target volumes -\n";
        map<VolumeName_t, VolumeName_t>::iterator tempiter = MapSrcToTgtVolumeNames.begin();
        for(; tempiter != MapSrcToTgtVolumeNames.end(); tempiter++)
        {
            cout<<tempiter->first<<" ==> "<<tempiter->second<<endl;
        }
        cout<<endl;
    }
    else
    {
        cout<<"Invalid Arguments.\n";
        cout<<"Usage - "<<argv[0]<<"[store | restore]"<<endl;
    }

    return 0;
}
