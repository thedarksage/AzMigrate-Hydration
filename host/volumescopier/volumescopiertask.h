#ifndef VOLUMES_COPIER_TASK_H
#define VOLUMES_COPIER_TASK_H

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include <ace/Mutex.h>
#include "volumescopier.h"
#include "volumescopierreader.h"
#include "volumescopierwriter.h"

class VolumesCopierTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    VolumesCopierTask(ACE_Thread_Manager *pthreadmanager,
                      VolumesCopier *pvolumescopier,
                      VolumesCopierReaderStage *preaderstage,
                      VolumesCopierWriterStage *pwriterstage);

    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();

private:
    VolumesCopier *m_pVolumesCopier;
    VolumesCopierReaderStage *m_pReaderStage;
    VolumesCopierWriterStage *m_pWriterStage;
};

#endif /* VOLUMES_COPIER_TASK_H */
