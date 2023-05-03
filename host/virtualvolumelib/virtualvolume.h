#ifndef VIRTUAL_VOLUME_H
#define VIRTUAL_VOLUME_H



#include "virtualvolumewin.h"
#include <vector>
#include "drvstatus.h"
#include "svtypes.h"
#include "configurator2.h"

#define VVOLUME_UNIQUE_ID_PREFIX			"VV_"
#define MAX_STRING_SIZE    256
#include<map>

#include "localconfigurator.h"

const size_t MAX_DRIVER_PATH_LENGTH = 128;

class VirVolume:public VirVolumeWin
{
public:
    VirVolume();
    ~VirVolume();
    bool mountsparsevolume( WCHAR *,TCHAR volumename[256]);
    // Bug #3549 added deletesparsefile to not delete the sparse files and the entries from drscout.conf
    bool DeletePersistVirtualVolumes(std::string,std::string, bool ifExists=false, bool deletepersistinfo = true,bool deletesparse = true);
    size_t wcstotcs(PTCHAR tstring, const size_t tstringlen, PCWCH wcstring);
    HANDLE OpenVirtualVolumeControlDevice();
    //VOID DeleteAutoAssignedDriveLetter(HANDLE hDevice, PTCHAR MountPoint);
    bool createsparsefile(const std::string& filename , const SV_ULONGLONG& size, InmSparseAttributeState_t sparseattr_state);
    bool resizesparsefile(const std::string& filename , const SV_ULONGLONG& size, InmSparseAttributeState_t sparseattr_state);

    HANDLE createvirtualvolume();
    void sendInformationtoCX();
    BOOL VolumeSupportsReparsePoints(PTCHAR);
    BOOL VolumeSupportsSparseFiles(PTCHAR);
    void VirVolumeCheckForRemount(std::string &list,bool createlist=false);//Used for remounting of Virtual Volumes
    bool Unmount(std::string,bool checkfortarget=true,bool bypassdriver = false);
    bool UnmountFileSystem(const PTCHAR);
    VOID DeleteVolumeMountPointVolume(const PTCHAR);
    //BOOLEAN FileDiskUmount(PTCHAR);
    bool DeleteDrivesForTargetVolume(const PTCHAR);
    bool DeleteMountPoints(const PTCHAR);
    bool DeleteMountPointsForTargetVolume(PTCHAR, PTCHAR);

    bool UnmountVirtualVolume(HANDLE,TCHAR[MAX_STRING_SIZE]);
    bool UnmountAllVirtualVolumes(bool checkfortarget=true,bool deletepersistinfo =true,bool deletesparsefile = true,bool bypassdriver = false); // Bug 3549
    void deletesparsefile(char*);
    bool IsNewSparseFileFormat(const char * filename);
    bool createsparsefileonresize(const std::string & filename, const SV_ULONGLONG & size,
        bool iscompressioenabled, bool  enable_sparseattribute);
    bool resize_new_version_sparse_file(std::string filename, SV_ULONGLONG size,
        bool enable_sparseattribute);
	SV_ULONGLONG getMaxSizeToResizePartFile(const std::string & filename);
	bool IsCompressonEnabledOnFile(const std::string & filename);
    bool IsVolpackUsedAsTarget(const std::string & volume);
private:
    Configurator* m_TheConfigurator; // singleton
    LocalConfigurator m_TheLocalConfigurator;


};
#endif
