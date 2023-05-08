#ifndef VIRTUAL_VOLUME_H
#define VIRTUAL_VOLUME_H



//#include "virtualvolumewin.h"
#include <vector>
//#include "drvstatus.h"
#include "svtypes.h"
#include "configurator2.h"
#include "volinfocollector.h"
#include "cdpplatform.h"
#define VVOLUME_UNIQUE_ID_PREFIX			"VV_"

#include <limits.h>

#define MAX_STRING_SIZE_FOR_VOLPACK  MAXPATHLEN

#include<map>

#include "localconfigurator.h"
#include "VsnapShared.h"



class VirVolume
{
public:
    VirVolume();
    ~VirVolume();
    bool mountsparsevolume( const char *,const char*,std::string &);
    // Bug #3549 added deletesparsefile to not delete the sparse files and the entries from drscout.conf
    bool DeletePersistVirtualVolumes(std::string,std::string, bool ifExists=false, bool deletepersistinfo = true,bool deletesparse = true);
    //size_t wcstotcs(wchar_t * tstring, unsigned short* string);
    ACE_HANDLE OpenVirtualVolumeControlDevice();
    bool createsparsefile(const std::string& filename , const SV_ULONGLONG& size, InmSparseAttributeState_t sparseattr_state);
    bool resizesparsefile(const std::string& filename , const SV_ULONGLONG& size, InmSparseAttributeState_t sparseattr_state);
    ACE_HANDLE createvirtualvolume();
    void sendInformationtoCX();
    bool VolumeSupportsReparsePoints(wchar_t *);
    bool VirVolumeCheckForRemount(std::string &list,bool createlist=false);//Used for remounting of Virtual Volumes
    bool Unmount(std::string,bool checkfortarget=true,bool bypassdriver = false);
    bool UnmountFileSystem(const wchar_t *);
    void DeleteVolumeMountPointVolume(const wchar_t *);
    bool DeleteDrivesForTargetVolume(const wchar_t *);
    bool DeleteMountPoints(const wchar_t *);
    bool DeleteMountPointsForTargetVolume(wchar_t *, wchar_t *);
    //bool TempHelp();
    bool UnmountVirtualVolume(ACE_HANDLE,std::string,bool bypassdriver = false );
    bool UnmountAllVirtualVolumes(bool checkfortarget=true,bool deletepersistinfo =true,bool deletesparsefile = true,bool bypassdriver = false); // Bug 3549
    void deletesparsefile(const char*);
    void process_proc_mounts(std::vector<volinfo> &fslist);	
    bool unmountmountpoint(std::string &strvolpack);
    bool UnLinkDskAndRDskIfRequired(const std::string dskname);
    bool IsNewSparseFileFormat(const char * filename);
    bool createsparsefileonresize(std::string filename, SV_ULONGLONG size);
    bool resize_new_version_sparse_file(std::string filename, SV_ULONGLONG size);
    SV_ULONGLONG getMaxSizeToResizePartFile(const std::string & filename);
    bool IsVolpackUsedAsTarget(const std::string & volume);
private:
    Configurator* m_TheConfigurator; // singleton
    LocalConfigurator m_TheLocalConfigurator;
};
#endif
