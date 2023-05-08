#include <functional>
#include "inmpipeline.h"
#include "logger.h"
#include "portablehelpers.h"


void InmPipeline::Stage::SetQuitEvent(void)
{
    m_quit.Signal(true);
}


bool InmPipeline::Stage::IsStopRequested(void)
{
    return m_quit.IsSignalled();
}


bool InmPipeline::Stage::ShouldQuit(long int seconds)
{
    return m_quit(seconds);
}


void InmPipeline::Stage::SetInQFromBackStage(ACE_Shared_MQ inQ)
{
    m_inQFromBackStage = inQ;
}


void InmPipeline::Stage::SetInQFromLastStage(ACE_Shared_MQ inQ)
{
    m_inQFromLastStage = inQ;
}


void InmPipeline::Stage::SetInQFromForwardStage(ACE_Shared_MQ inQ)
{
    m_inQFromForwardStage = inQ;
}


void InmPipeline::Stage::SetOutQToForwardStage(ACE_Shared_MQ outQ)
{
    m_outQToForwardStage = outQ;
}


void InmPipeline::Stage::SetOutQToFirstStage(ACE_Shared_MQ outQ)
{
    m_outQToFirstStage = outQ;
}


void InmPipeline::Stage::SetOutQToBackStage(ACE_Shared_MQ outQ)
{
    m_outQToBackStage = outQ;
}


void InmPipeline::Stage::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "input queue from back stage = %p\n", m_inQFromBackStage);
    DebugPrintf(SV_LOG_DEBUG, "input queue from last stage = %p\n", m_inQFromLastStage);
    DebugPrintf(SV_LOG_DEBUG, "input queue from forward stage = %p\n", m_inQFromForwardStage);
    DebugPrintf(SV_LOG_DEBUG, "output queue to forward stage = %p\n", m_outQToForwardStage);
    DebugPrintf(SV_LOG_DEBUG, "output queue to first stage = %p\n", m_outQToFirstStage);
    DebugPrintf(SV_LOG_DEBUG, "output queue to back stage = %p\n", m_outQToBackStage);
}


bool InmPipeline::Stage::WriteToForwardStage(void *data, const size_t size, const int &waitsecs)
{
    return WriteToQ(m_outQToForwardStage, data, size, waitsecs);
}


bool InmPipeline::Stage::WriteToFirstStage(void *data, const size_t size, const int &waitsecs)
{
    return WriteToQ(m_outQToFirstStage, data, size, waitsecs);
}


bool InmPipeline::Stage::WriteToBackStage(void *data, const size_t size, const int &waitsecs)
{
    return WriteToQ(m_outQToBackStage, data, size, waitsecs);
}


bool InmPipeline::Stage::WriteToQ(ACE_Shared_MQ Q, void *data, const size_t size, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Message_Block *pmb = new ACE_Message_Block((char *)data, size);
    QuitFunction_t qf = std::bind1st(std::mem_fun(&InmPipeline::Stage::ShouldQuit), this);
     
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return EnQ(Q, pmb, waitsecs, qf);
}


void * InmPipeline::Stage::ReadFromBackStage(const int &waitsecs)
{
    return ReadFromQ(m_inQFromBackStage, waitsecs);
}


void * InmPipeline::Stage::ReadFromLastStage(const int &waitsecs)
{
    return ReadFromQ(m_inQFromLastStage, waitsecs);
}


void * InmPipeline::Stage::ReadFromForwardStage(const int &waitsecs)
{
    return ReadFromQ(m_inQFromForwardStage, waitsecs);
}

 
void * InmPipeline::Stage::ReadFromQ(ACE_Shared_MQ Q, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    void *data = 0;

    ACE_Message_Block *pmb = DeQ(Q, waitsecs, m_Profile, &m_TimeForReadWait);
    if (pmb)
    {
        data = pmb->base();
        pmb->release();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return data;
}


State_t InmPipeline::Stage::GetState(void)
{
    return m_State;
}


void InmPipeline::Stage::SetExceptionStatus(const std::string &excmsg)
{
    m_State = STATE_EXCEPTION;
    m_ExceptionMsg = excmsg;
}


std::string InmPipeline::Stage::GetExceptionMsg(void)
{
    return m_ExceptionMsg;
}


WAIT_STATE InmPipeline::Stage::WaitOnQuitEvent(long int seconds)
{
    WAIT_STATE waitRet = m_quit.Wait(seconds);
    return waitRet;
}


ACE_Time_Value InmPipeline::Stage::GetTimeForReadWait(void)
{
	return m_TimeForReadWait;
}

