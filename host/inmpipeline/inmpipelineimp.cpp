#include "inmpipelineimp.h"
#include "logger.h"
#include "portablehelpers.h"


int InmPipelineImp::StageImp::open(void *args)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return this->activate(THR_BOUND);
}


int InmPipelineImp::StageImp::close(unsigned long flags)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return 0;
}

int InmPipelineImp::StageImp::svc()
{
    return m_pStage->svc();
}


void InmPipelineImp::StageImp::SetQuitEvent(void)
{
    m_pStage->SetQuitEvent();
}


State_t InmPipelineImp::StageImp::GetState(void)
{
    return m_pStage->GetState();
}

std::string InmPipelineImp::StageImp::GetExceptionMsg(void)
{
    return m_pStage->GetExceptionMsg();
}

void InmPipelineImp::StageImp::SetInQFromBackStage(ACE_Shared_MQ inQ)
{
    m_pStage->SetInQFromBackStage(inQ);
}

void InmPipelineImp::StageImp::SetInQFromLastStage(ACE_Shared_MQ inQ)
{
    m_pStage->SetInQFromLastStage(inQ);
}

void InmPipelineImp::StageImp::SetInQFromForwardStage(ACE_Shared_MQ inQ)
{
    m_pStage->SetInQFromForwardStage(inQ);
}

void InmPipelineImp::StageImp::SetOutQToForwardStage(ACE_Shared_MQ outQ)
{
    m_pStage->SetOutQToForwardStage(outQ);
}

void InmPipelineImp::StageImp::SetOutQToFirstStage(ACE_Shared_MQ outQ)
{
    m_pStage->SetOutQToFirstStage(outQ);
}

void InmPipelineImp::StageImp::SetOutQToBackStage(ACE_Shared_MQ outQ)
{
    m_pStage->SetOutQToBackStage(outQ);
}


InmPipelineImp::InmPipelineImp() :
m_State(STATE_NORMAL)
{
}


InmPipelineImp::~InmPipelineImp()
{
    Destroy();
}


bool InmPipelineImp::Create(const Stages_t &stages, const Type_t type)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bcreated = false;

    SetType(type);
    if (stages.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "number of stages in pipeline is empty\n");
    }
    else 
    {
        bcreated = FormStages(stages);
        if (bcreated)
        {
            DebugPrintf(SV_LOG_DEBUG, "stages formed successfully\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "stages could not be formed in pipeline\n");
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bcreated;
}


bool InmPipelineImp::FormStages(const Stages_t &stages)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed;

    bformed = CreateStages(stages);
    if (bformed)
    {
        /* always connect linearly first */
        bformed = ConnectLinearly();
        if (bformed)
        {
            if (CIRCULAR == m_Type)
            {
                bformed = ConnectCircularly();
            }
            else if(MULTIRINGS == m_Type)
            {
                bformed = ConnectInDualRings();
            }
        }
    }

    if (bformed)
    {
        bformed = SpawnStages();
    }

    if (!bformed)
    {
        DebugPrintf(SV_LOG_ERROR, "could not connect stages in pipeline\n");
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmPipelineImp::Destroy(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bdestroyed = true;

    SetQuitOnAllStages();
    
    DebugPrintf(SV_LOG_DEBUG, "Waiting for pipeline stages to quit\n");
    m_ThreadManager.wait();

    for (ConstImpStagesIter_t it = m_ImpStagesInfo.m_ImpStages.begin(); it != m_ImpStagesInfo.m_ImpStages.end(); it++)
    {
        StageImp *psimp = *it;
        delete psimp;
    }
    m_ImpStagesInfo.m_ImpStages.clear();

    for (Const_ACE_Shared_MQSIter_t it = m_ImpStagesInfo.m_QS.begin(); it != m_ImpStagesInfo.m_QS.end(); it++)
    {
        ACE_Shared_MQ pq = *it;
        delete pq;
    }
    m_ImpStagesInfo.m_QS.clear();

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bdestroyed;
}


bool InmPipelineImp::SetQuitOnAllStages(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bset = true;

    for (ConstImpStagesIter_t it = m_ImpStagesInfo.m_ImpStages.begin(); it != m_ImpStagesInfo.m_ImpStages.end(); it++)
    {
        StageImp *psimp = *it;
        psimp->SetQuitEvent();
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bset;
}


State_t InmPipelineImp::GetState(void)
{
    for (ConstImpStagesIter_t it = m_ImpStagesInfo.m_ImpStages.begin(); it != m_ImpStagesInfo.m_ImpStages.end(); it++)
    {
        StageImp *psimp = *it;
        State_t st = psimp->GetState();
        if (STATE_NORMAL != st)
        {
            m_State = st;
            break;
        }
    }

    return m_State;
}


std::string InmPipelineImp::GetExceptionMsg(void)
{
    return m_ExceptionMsg;
}


void InmPipelineImp::SetExceptionStatus(const std::string &excmsg)
{
    m_State = STATE_EXCEPTION;
    m_ExceptionMsg = excmsg;
}


void InmPipelineImp::GetStatusOfStages(Statuses_t &statuses)
{
    for (ConstImpStagesIter_t it = m_ImpStagesInfo.m_ImpStages.begin(); it != m_ImpStagesInfo.m_ImpStages.end(); it++)
    {
        StageImp *psimp = *it;
        statuses.push_back(Status_t(psimp->GetState(), psimp->GetExceptionMsg()));
    }
}


void InmPipelineImp::SetType(const Type_t type)
{
    m_Type = type;
}


bool InmPipelineImp::CreateStages(const Stages_t &stages)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = false;

    for (ConstStagesIter_t it = stages.begin(); it != stages.end(); it++)
    {
        if ((it + 1) != stages.end())
        {
            ACE_Shared_MQ pq = new (std::nothrow) ACE_Message_Queue<ACE_MT_SYNCH>();
            bformed = pq;
            if (!bformed)
            {
                DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating queue for a stage\n");
                break;
            }
            m_ImpStagesInfo.m_QS.push_back(pq);
        }

        Stage *ps = *it;
        StageImp *psimp = new (std::nothrow) StageImp(ps, &m_ThreadManager);
        bformed = psimp;
        if (!bformed)
        {
            DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating a stage\n");
            break;
        }
        m_ImpStagesInfo.m_ImpStages.push_back(psimp);
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmPipelineImp::ConnectLinearly(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = true;

    for (ImpStages_t::size_type i = 0; i < m_ImpStagesInfo.m_ImpStages.size(); i++)
    {
        if (0 != i)
        {
            m_ImpStagesInfo.m_ImpStages[i]->SetInQFromBackStage(m_ImpStagesInfo.m_QS[i - 1]);
        }
        if ((m_ImpStagesInfo.m_ImpStages.size() - 1) != i)
        {
            m_ImpStagesInfo.m_ImpStages[i]->SetOutQToForwardStage(m_ImpStagesInfo.m_QS[i]);
        }
    }
 
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmPipelineImp::ConnectCircularly(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed;
    
    ACE_Shared_MQ pq = new (std::nothrow) ACE_Message_Queue<ACE_MT_SYNCH>();
    bformed = pq;
    if (bformed)
    {
        m_ImpStagesInfo.m_QS.push_back(pq);
        m_ImpStagesInfo.m_ImpStages[m_ImpStagesInfo.m_ImpStages.size() - 1]->SetOutQToFirstStage(pq);
        m_ImpStagesInfo.m_ImpStages[0]->SetInQFromLastStage(pq);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating queue for a stage\n");
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmPipelineImp::ConnectInDualRings(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = false;
    
    ACE_Shared_MQS::size_type prevqsz = m_ImpStagesInfo.m_QS.size();

    for (ImpStages_t::size_type i = 0; i < m_ImpStagesInfo.m_ImpStages.size(); i++)
    {
        if ((i+1) != m_ImpStagesInfo.m_ImpStages.size())
        {
            ACE_Shared_MQ pq = new (std::nothrow) ACE_Message_Queue<ACE_MT_SYNCH>();
            bformed = pq;
            if (!bformed)
            {
                DebugPrintf(SV_LOG_ERROR, "could not allocate memory for creating queue for a stage\n");
                break;
            }
            m_ImpStagesInfo.m_QS.push_back(pq);
        }
    }

    for (ImpStages_t::size_type i = 0; i < m_ImpStagesInfo.m_ImpStages.size(); i++)
    {
        if (0 != i)
        {
            m_ImpStagesInfo.m_ImpStages[i]->SetOutQToBackStage(m_ImpStagesInfo.m_QS[prevqsz + i - 1]);
        }
        if ((m_ImpStagesInfo.m_ImpStages.size() - 1) != i)
        {
            m_ImpStagesInfo.m_ImpStages[i]->SetInQFromForwardStage(m_ImpStagesInfo.m_QS[prevqsz + i]);
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}


bool InmPipelineImp::SpawnStages(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    bool bformed = false;

    for (ImpStages_t::size_type i = 0; i < m_ImpStagesInfo.m_ImpStages.size(); i++)
    {
        bformed = (-1 != m_ImpStagesInfo.m_ImpStages[i]->open());
        if (!bformed)
        {
            DebugPrintf(SV_LOG_ERROR, "could not spawn a stage\n");
            break;
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bformed;
}

