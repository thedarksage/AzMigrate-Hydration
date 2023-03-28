#include <functional>
#include "inmdistributor.h"
#include "logger.h"
#include "portablehelpers.h"


void InmDistributor::Worker::SetQuitEvent(void)
{
    m_quit.Signal(true);
}


bool InmDistributor::Worker::IsStopRequested(void)
{
    return m_quit.IsSignalled();
}


bool InmDistributor::Worker::ShouldQuit(long int seconds)
{
    return m_quit(seconds);
}


void InmDistributor::Worker::SetInQFromDistributor(ACE_Shared_MQ inQ)
{
    m_inQFromDistributor = inQ;
}


void InmDistributor::Worker::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "input queue from distributor = %p\n", m_inQFromDistributor);
}


bool InmDistributor::Worker::WriteToQ(ACE_Shared_MQ Q, void *data, const size_t size, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Message_Block *pmb = new ACE_Message_Block();
    QuitFunction_t qf = std::bind1st(std::mem_fun(&InmDistributor::Worker::ShouldQuit), this);
     
    pmb->base((char *)data, size);
     
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return EnQ(Q, pmb, waitsecs, qf);
}


void * InmDistributor::Worker::ReadFromDistributor(const int &waitsecs)
{
    return ReadFromQ(m_inQFromDistributor, waitsecs);
}


void * InmDistributor::Worker::ReadFromQ(ACE_Shared_MQ Q, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    void *data = 0;

    ACE_Message_Block *pmb = DeQ(Q, waitsecs);
    if (pmb)
    {
        data = pmb->base();
        pmb->release();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return data;
}


State_t InmDistributor::Worker::GetState(void)
{
    return m_State;
}


void InmDistributor::Worker::SetExceptionStatus(const std::string &excmsg)
{
    m_State = STATE_EXCEPTION;
    m_ExceptionMsg = excmsg;
}


std::string InmDistributor::Worker::GetExceptionMsg(void)
{
    return m_ExceptionMsg;
}


WAIT_STATE InmDistributor::Worker::WaitOnQuitEvent(long int seconds)
{
    WAIT_STATE waitRet = m_quit.Wait(seconds);
    return waitRet;
}
