#ifndef INM__QDISTRIBUTOR__H__
#define INM__QDISTRIBUTOR__H__

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include <ace/Mutex.h>

#include "inmdistributor.h"
#include "inmdefines.h"

class InmQDistributor : public InmDistributor
{
private:
    class WorkerImp : public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        WorkerImp(Worker *pWorker, ACE_Thread_Manager* threadManager)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_pWorker(pWorker)
        {
        }
        virtual ~WorkerImp() {}
        virtual int open(void *args = 0);              /* starts thread and returns */
        virtual int close(unsigned long flags = 0);    /* waits until the forked thread exits */
        virtual int svc(void);
        void SetQuitEvent(void);
        void SetInQFromDistributor(ACE_Shared_MQ inQ);
        State_t GetState(void);
        std::string GetExceptionMsg(void);

    private:
        Worker *m_pWorker;
    };

    typedef std::vector<WorkerImp *> WorkersImp_t;
    typedef WorkersImp_t::iterator WorkersImpIter_t;
    typedef WorkersImp_t::const_iterator ConstWorkersImpIter_t;

public:
    InmQDistributor();
    ~InmQDistributor();
    bool Create(const Workers_t &workers);
    bool Destroy(void);
    State_t GetState(void);
    void GetStatusOfWorkers(Statuses_t &statuses);
    std::string GetExceptionMsg(void);
    bool Distribute(void *pdata, const size_t &size, QuitFunction_t qf, const int &waitsecs);

private:
    bool FormWorkers(const Workers_t &workers);
    bool SetQuitOnAllWorkers(void);
    void SetExceptionStatus(const std::string &excmsg);
    bool SpawnWorkers(void);
    bool CreateWorkers(const Workers_t &workers);
    bool Connect(void);

private:
    ACE_Thread_Manager m_ThreadManager;
    ACE_Shared_MQ m_Q;
    WorkersImp_t m_WorkersImp;
    State_t m_State;
    std::string m_ExceptionMsg;
};

#endif /* INM__QDISTRIBUTOR__H__ */
