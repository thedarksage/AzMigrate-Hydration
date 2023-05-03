#ifndef INM__PIPELINEIMP__H__
#define INM__PIPELINEIMP__H__

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include <ace/Mutex.h>

#include "inmpipeline.h"
#include "inmdefines.h"

class InmPipelineImp : public InmPipeline
{
private:
    class StageImp : public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        StageImp(Stage *pStage, ACE_Thread_Manager* threadManager)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_pStage(pStage)
        {
        }
        virtual ~StageImp() {}
        virtual int open(void *args = 0);              /* starts thread and returns */
        virtual int close(unsigned long flags = 0);    /* waits until the forked thread exits */
        virtual int svc(void);
        void SetQuitEvent(void);
        void SetInQFromBackStage(ACE_Shared_MQ inQ);
        void SetInQFromLastStage(ACE_Shared_MQ inQ);
        void SetInQFromForwardStage(ACE_Shared_MQ inQ);
        void SetOutQToForwardStage(ACE_Shared_MQ outQ);
        void SetOutQToFirstStage(ACE_Shared_MQ outQ);
        void SetOutQToBackStage(ACE_Shared_MQ outQ);
        State_t GetState(void);
        std::string GetExceptionMsg(void);

    private:
        Stage *m_pStage;
    };
    typedef std::vector<StageImp *> ImpStages_t;
    typedef ImpStages_t::const_iterator ConstImpStagesIter_t;
    typedef ImpStages_t::iterator ImpStagesIter_t;

    typedef struct StagesInfo
    {
        ACE_Shared_MQS m_QS;
        ImpStages_t m_ImpStages;
        
    } ImpStagesInfo_t;

public:
    InmPipelineImp();
    ~InmPipelineImp();
    bool Create(const Stages_t &stages, const Type_t type);
    bool Destroy(void);
    State_t GetState(void);
    void GetStatusOfStages(Statuses_t &statuses);
    std::string GetExceptionMsg(void);

private:
    bool FormStages(const Stages_t &stages);
    bool SetQuitOnAllStages(void);
    void SetType(const Type_t type);
    void SetExceptionStatus(const std::string &excmsg);
    bool SpawnStages(void);
    bool ConnectInDualRings(void);
    bool ConnectCircularly(void);
    bool ConnectLinearly(void);
    bool CreateStages(const Stages_t &stages);

private:
    ACE_Thread_Manager m_ThreadManager;
    ImpStagesInfo_t m_ImpStagesInfo;
    Type_t m_Type;
    State_t m_State;
    std::string m_ExceptionMsg;
};

#endif /* INM__PIPELINEIMP__H__ */
