#ifndef SIMPLE_VOLUMES_COPIER_APP
#define SIMPLE_VOLUMES_COPIER_APP

#include "svtypes.h"
#include "event.h"
#include "volumescopier.h"

class SimpleVolumesCopierApp
{
public:
    SimpleVolumesCopierApp();
    ~SimpleVolumesCopierApp();
	bool ActionOnNoBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);
    bool ActionOnBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);
    bool ActionOnAllBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered);
    void SignalQuit(void);
    bool ShouldQuit(int secs);
    SV_ULONGLONG GetBytesCovered(void);
	SV_ULONGLONG GetFilesystemUnusedBytes(void);

private:
    Event m_QuitEvent;
    SV_ULONGLONG m_BytesCovered;
	SV_ULONGLONG m_FilesystemUnusedBytes;
};

#endif /* SIMPLE_VOLUMES_COPIER_APP */
