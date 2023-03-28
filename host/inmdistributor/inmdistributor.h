#ifndef INM__DISTRIBUTOR__H__
#define INM__DISTRIBUTOR__H__

#include <vector>
#include <iterator>

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include <ace/Mutex.h>
#include <ace/Message_Queue_T.h>
#include <ace/Message_Block.h>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "event.h"
#include "inmdefines.h"
#include "inmquitfunction.h"

class InmDistributor
{
public:
    typedef ACE_Message_Queue<ACE_MT_SYNCH> *ACE_Shared_MQ;

    class Worker
    {
    public:
        Worker() 
        : m_quit(false, false),
          m_inQFromDistributor(0),
          m_State(STATE_NORMAL)
        {
        }
        virtual ~Worker() {}
        virtual int svc(void) = 0;
        void SetQuitEvent(void);
        bool IsStopRequested(void);
        bool ShouldQuit(long int seconds);
        WAIT_STATE WaitOnQuitEvent(long int seconds);
        void SetInQFromDistributor(ACE_Shared_MQ inQ);
        void Print(void);
        bool WriteToDistributor(void *data, const size_t size, const int &waitsecs);
        void * ReadFromDistributor(const int &waitsecs);
        State_t GetState(void);
        void SetExceptionStatus(const std::string &excmsg);
        std::string GetExceptionMsg(void);

    private:
        void * ReadFromQ(ACE_Shared_MQ Q, const int &waitsecs);
        bool WriteToQ(ACE_Shared_MQ Q, void *data, const size_t size, const int &waitsecs);

    private:
        Event m_quit;
        ACE_Shared_MQ m_inQFromDistributor;
        State_t m_State;
        std::string m_ExceptionMsg;
    };
    typedef std::vector<Worker *> Workers_t;
    typedef Workers_t::const_iterator ConstWorkersIter_t;
    typedef Workers_t::iterator WorkersIter_t;

public:
    virtual bool Create(const Workers_t &workers) = 0;
    virtual bool Destroy(void) = 0;
    virtual State_t GetState(void) = 0;
    virtual void GetStatusOfWorkers(Statuses_t &statuses) = 0;
    virtual std::string GetExceptionMsg(void) = 0;
    virtual bool Distribute(void *pdata, const size_t &size, QuitFunction_t qf, const int &waitsecs) = 0;
    virtual ~InmDistributor() {}
};

#endif /* INM__DISTRIBUTOR__H__ */
