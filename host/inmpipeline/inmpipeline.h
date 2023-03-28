#ifndef INM__PIPELINE__H__
#define INM__PIPELINE__H__

#include <vector>
#include <iterator>

#include <ace/ACE.h>
#include <ace/Time_Value.h>
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

const char * const StrPipelineType[] = {"linear", "circular", "multirings"};

class InmPipeline
{
public:
    typedef ACE_Message_Queue<ACE_MT_SYNCH> *ACE_Shared_MQ;
    typedef std::vector<ACE_Shared_MQ> ACE_Shared_MQS;
    typedef ACE_Shared_MQS::const_iterator Const_ACE_Shared_MQSIter_t;
    typedef ACE_Shared_MQS::iterator ACE_Shared_MQSIter_t;

    typedef enum eType
    {
        LINEAR, 
        CIRCULAR,
        MULTIRINGS

    } Type_t;

    class Stage
    {
    public:
        Stage(const bool &profile = false) 
        : m_quit(false, false),
          m_inQFromBackStage(0),
          m_inQFromLastStage(0),
          m_inQFromForwardStage(0),
          m_outQToForwardStage(0),
          m_outQToFirstStage(0),
          m_outQToBackStage(0),
          m_State(STATE_NORMAL),
		  m_Profile(profile),
		  m_TimeForReadWait(0)
        {
        }
        virtual ~Stage() {}
        virtual int svc(void) = 0;
        void SetQuitEvent(void);
        bool IsStopRequested(void);
        bool ShouldQuit(long int seconds);
        WAIT_STATE WaitOnQuitEvent(long int seconds);
        void SetInQFromBackStage(ACE_Shared_MQ inQ);
        void SetInQFromLastStage(ACE_Shared_MQ inQ);
        void SetInQFromForwardStage(ACE_Shared_MQ inQ);
        void SetOutQToForwardStage(ACE_Shared_MQ outQ);
        void SetOutQToFirstStage(ACE_Shared_MQ outQ);
        void SetOutQToBackStage(ACE_Shared_MQ outQ);
        void Print(void);
        bool WriteToForwardStage(void *data, const size_t size, const int &waitsecs);
        bool WriteToFirstStage(void *data, const size_t size, const int &waitsecs);
        bool WriteToBackStage(void *data, const size_t size, const int &waitsecs);
        void * ReadFromBackStage(const int &waitsecs);
        void * ReadFromLastStage(const int &waitsecs);
        void * ReadFromForwardStage(const int &waitsecs);
        State_t GetState(void);
        void SetExceptionStatus(const std::string &excmsg);
        std::string GetExceptionMsg(void);
		ACE_Time_Value GetTimeForReadWait(void);

    private:
        void * ReadFromQ(ACE_Shared_MQ Q, const int &waitsecs);
        bool WriteToQ(ACE_Shared_MQ Q, void *data, const size_t size, const int &waitsecs);

    private:
        Event m_quit;
        ACE_Shared_MQ m_inQFromBackStage;
        ACE_Shared_MQ m_inQFromLastStage;
        ACE_Shared_MQ m_inQFromForwardStage;
        ACE_Shared_MQ m_outQToForwardStage;
        ACE_Shared_MQ m_outQToFirstStage;
        ACE_Shared_MQ m_outQToBackStage;
        State_t m_State;
        std::string m_ExceptionMsg;
		bool m_Profile;
		ACE_Time_Value m_TimeForReadWait;
    };
    typedef std::vector<Stage *> Stages_t;
    typedef Stages_t::const_iterator ConstStagesIter_t;
    typedef Stages_t::iterator StagesIter_t;

public:
    virtual bool Create(const Stages_t &stages, const Type_t type) = 0;
    virtual bool Destroy(void) = 0;                          /* should this be named as close ? */
    virtual State_t GetState(void) = 0;
    virtual void GetStatusOfStages(Statuses_t &statuses) = 0;
    virtual std::string GetExceptionMsg(void) = 0;
    virtual ~InmPipeline() {}
};

#endif /* INM__PIPELINE__H__ */
