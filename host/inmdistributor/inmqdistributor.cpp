#include "inmqdistributor.h"
#include "logger.h"
#include "portablehelpers.h"


int InmQDistributor::WorkerImp::open(void *args)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return this->activate(THR_BOUND);
}


int InmQDistributor::WorkerImp::close(unsigned long flags)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return 0;
}

int InmQDistributor::WorkerImp::svc()
{
    return m_pWorker->svc();
}


void InmQDistributor::WorkerImp::SetQuitEvent(void)
{
    m_pWorker->SetQuitEvent();
}


State_t InmQDistributor::WorkerImp::GetState(void)
{
    return m_pWorker->GetState();
}

std::string InmQDistributor::WorkerImp::GetExceptionMsg(void)
{
    return m_pWorker->GetExceptionMsg();
}

void InmQDistributor::WorkerImp::SetInQFromDistributor(ACE_Shared_MQ inQ)
{
    m_pWorker->SetInQFromDistributor(inQ);
}


InmQDistributor::InmQDistributor() :
m_State(STATE_NORMAL),
m_Q(0)
{
}


InmQDistributor::~InmQDistributor()
{
    Destroy();
}


bool InmQDistributor::Create(const Workers_t &workers)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bcreated = false;

    if (workers.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "number of workers is empty\n");
    }
    else 
    {
        bcreated = FormWorkers(workers);
        if (bcreated)
        {
            DebugPrintf(SV_LOG_DEBUG, "workers formed successfully\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "workers could not be formed\n");
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bcreated;
}


bool InmQDistributor::FormWorkers(const Workers_t &workers)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed;

    bformed = CreateWorkers(workers);
    if (bformed)
    {
        bformed = Connect();
    }

    if (bformed)
    {
        bformed = SpawnWorkers();
    }

    if (!bformed)
    {
        DebugPrintf(SV_LOG_ERROR, "could not form, connect workers\n");
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmQDistributor::Destroy(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bdestroyed = true;

    SetQuitOnAllWorkers();
    
    DebugPrintf(SV_LOG_DEBUG, "Waiting for workers to quit\n");
    m_ThreadManager.wait();

    WorkerImp *pwi;
    for (WorkersImpIter_t it = m_WorkersImp.begin(); it != m_WorkersImp.end(); it++)
    {
        pwi = *it;
        delete pwi;
    }
    m_WorkersImp.clear();

    if (m_Q)
    {
        delete m_Q;
        m_Q = 0;
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bdestroyed;
}


bool InmQDistributor::SetQuitOnAllWorkers(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bset = true;

    WorkerImp *pwi;
    for (WorkersImpIter_t it = m_WorkersImp.begin(); it != m_WorkersImp.end(); it++)
    {
        pwi = *it;
        pwi->SetQuitEvent();
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bset;
}


State_t InmQDistributor::GetState(void)
{
    WorkerImp *pwi;
    for (WorkersImpIter_t it = m_WorkersImp.begin(); it != m_WorkersImp.end(); it++)
    {
        pwi = *it;
        State_t st = pwi->GetState();
        if (STATE_NORMAL != st)
        {
            m_State = st;
            break;
        }
    }

    return m_State;
}


std::string InmQDistributor::GetExceptionMsg(void)
{
    return m_ExceptionMsg;
}


void InmQDistributor::SetExceptionStatus(const std::string &excmsg)
{
    m_State = STATE_EXCEPTION;
    m_ExceptionMsg = excmsg;
}


void InmQDistributor::GetStatusOfWorkers(Statuses_t &statuses)
{
    WorkerImp *pwi;
    for (WorkersImpIter_t it = m_WorkersImp.begin(); it != m_WorkersImp.end(); it++)
    {
        pwi = *it;
        statuses.push_back(Status_t(pwi->GetState(), pwi->GetExceptionMsg()));
    }
}


bool InmQDistributor::CreateWorkers(const Workers_t &workers)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed;

    m_Q = new (std::nothrow) ACE_Message_Queue<ACE_MT_SYNCH>();
    bformed = m_Q;
    if (bformed)
    {
        for (ConstWorkersIter_t it = workers.begin(); it != workers.end(); it++)
        {
            Worker *ps = *it;
            WorkerImp *pwi = new (std::nothrow) WorkerImp(ps, &m_ThreadManager);
            bformed = pwi;
            if (!bformed)
            {
                DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating a worker\n");
                break;
            }
            m_WorkersImp.push_back(pwi);
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating input queue for a workers\n");
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmQDistributor::Connect(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = true;

    for (Workers_t::size_type i = 0; i < m_WorkersImp.size(); i++)
    {
        m_WorkersImp[i]->SetInQFromDistributor(m_Q);
    }
 
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmQDistributor::SpawnWorkers(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = false;

    for (Workers_t::size_type i = 0; i < m_WorkersImp.size(); i++)
    {
        bformed = (-1 != m_WorkersImp[i]->open());
        if (!bformed)
        {
            DebugPrintf(SV_LOG_ERROR, "could not spawn a worker\n");
            break;
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmQDistributor::Distribute(void *pdata, const size_t &size, QuitFunction_t qf, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Message_Block *pmb = new ACE_Message_Block((char *) pdata, size);
     
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return EnQ(m_Q, pmb, waitsecs, qf);
}
