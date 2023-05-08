#ifndef __LOCALDIRECTSYNC_
#define __LOCALDIRECTSYNC_


#include <string>
#include <fstream>
#include <list>

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "dataprotectionsync.h"
#include "cdpapply.h"
#include "dataprotectionutils.h"
#include "svtypes.h"
#include "cdpvolume.h"
#include "inmdefines.h"
#include "volumeapplier.h"
#include "volumescopier.h"

class LocalDirectSync;
typedef boost::shared_ptr<LocalDirectSync> LocalDirectSyncPtr;

class LocalDirectSync: public DataProtectionSync,public ACE_Task<ACE_MT_SYNCH>
{
public:
    LocalDirectSync(ACE_Thread_Manager* threadManager,VOLUME_GROUP_SETTINGS const& volumeGrpSettings, const std::string &sourcename, 
                    const std::string &targetname, const SV_ULONG &blockSizeInKB, const std::string &directoryforinternaloperations);
    LocalDirectSync(VOLUME_GROUP_SETTINGS const& volumeGrpSettings, const std::string &sourcename, 
                    const std::string &targetname, const SV_ULONG &blockSizeInKB, const std::string &directoryforinternaloperations);
    void Start(void);
    void unprotect() ;
    void UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings)
    {
    }
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
    void Run() ;
    void stop() ;
    
    /* do not use pass by reference here since these
     * are provided as call backs to retention applier */
    std::string RetentionApplyFileName(const VolumeWithOffsetToApply_t volumewithoffsettoapply);
    CDPApply *CreateCDPApplier(const std::string deviceName);
    void DestroyCDPApplier(CDPApply *pcdpapply);
    bool UseRetentionBytesApplied(const SV_LONGLONG bytesapplied);
    bool ShouldApplyRetention(const std::string deviceName);
    
	bool ActionOnNoBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);
    bool ActionOnAllBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);
    bool ActionOnBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);

	VolumeApplier *CreateRetentionVolumeApplier(VolumeApplier::VolumeApplierFormationInputs_t inputs);
    void DestroyRetentionVolumeApplier(VolumeApplier *pvolumeapplier);

	const CDP_SETTINGS & CdpSettings() { return m_cdpsettings;}
	cdp_resync_txn * CdpResyncTxnPtr() { return m_cdpresynctxn_ptr.get(); }

private:
    int UpdateProgressToCX(const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes);
    bool DoPreCopyOperations(void);
	void RecordProfileData(const char *data);

private:
    SV_UINT m_maxRWSize;
    std::string m_tgtVolName;
    std::string m_srcVolName;
	std::string m_DirectoryForInternalOperations;
	CDP_SETTINGS m_cdpsettings;
	cdp_resync_txn::ptr m_cdpresynctxn_ptr;
};
			
#endif //For __LOCALDIRECTSYNC_			
