#include "ace/Task.h"

class DifferentialSync;

class ApplyDifferentialTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    ApplyDifferentialTask(DifferentialSync* diffSync, ACE_Thread_Manager* threadManager = 0) 
        : ACE_Task<ACE_MT_SYNCH>(threadManager), m_DiffSync(diffSync) {}
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();

private:
    DifferentialSync* m_DiffSync;
};

class GetDifferentialTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    GetDifferentialTask(DifferentialSync* diffSync, ApplyDifferentialTask* applyTask, ACE_Thread_Manager* threadManager = 0) 
        : ACE_Task<ACE_MT_SYNCH>(threadManager), m_DiffSync(diffSync), m_ApplyTask(applyTask) {}        
            
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();

private:
    DifferentialSync* m_DiffSync;
    ApplyDifferentialTask* m_ApplyTask;
};


