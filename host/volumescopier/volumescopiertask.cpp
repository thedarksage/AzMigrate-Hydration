#include "volumescopiertask.h"
#include "logger.h"
#include "portablehelpers.h"
#include "inmageex.h"


VolumesCopierTask::VolumesCopierTask(ACE_Thread_Manager *pthreadmanager,
                                     VolumesCopier *pvolumescopier,
                                     VolumesCopierReaderStage *preaderstage,
                                     VolumesCopierWriterStage *pwriterstage)
: ACE_Task<ACE_MT_SYNCH>(pthreadmanager),
  m_pVolumesCopier(pvolumescopier),
  m_pReaderStage(preaderstage),
  m_pWriterStage(pwriterstage)
{
}


int VolumesCopierTask::open( void *args )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return this->activate(THR_BOUND);
}


int VolumesCopierTask::close( SV_ULONG flags )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return 0 ;
}


int VolumesCopierTask::svc()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    int rval = -1;

    try
    {
        std::stringstream excmsg;
        if (!m_pWriterStage->Init(excmsg))
        {
            throw INMAGE_EX(excmsg.str());
        }

        m_pReaderStage->SetWriter(m_pWriterStage);
        rval = m_pReaderStage->svc();
        m_pWriterStage->Flush();
    }
	catch (ContextualException& ce)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", ce.what());
        m_pVolumesCopier->InformError(ce.what(), WAITSECS);
	}
	catch (std::exception& e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", e.what());
        m_pVolumesCopier->InformError(e.what(), WAITSECS);
	}

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return rval;
}
