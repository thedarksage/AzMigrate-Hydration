
#ifndef CX_COMMUNITOR__
#define CX_COMMUNITOR__
#include "ace/Thread.h"
#include "ace/Thread_Manager.h"
#include "ace/Get_Opt.h"
#include "ace/Atomic_Op.h"
#include "ace/OS_NS_unistd.h"
#include "ace/Guard_T.h"
#include "ace/RW_Thread_Mutex.h"
#include "ace/Time_Value.h"
#include "ace/Task.h"

#include <string>
#include <map>


typedef std::map<std::string, std::string> MapLunIdToAtLun;

class CxCommunicator :  public ACE_Task <ACE_MT_SYNCH>
{

public:
    CxCommunicator(ACE_Thread_Manager* threadManager):ACE_Task<ACE_MT_SYNCH>(threadManager), quit(false){}

    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
    virtual int handle_signal (int signum, siginfo_t * = 0, ucontext_t * = 0);
    std::string getAtLun(const std::string &lunId);
    void signalQuit() { quit = true;}

private:
    MapLunIdToAtLun m_MapLunIdToAtLun;
    ACE_RW_Thread_Mutex m_RWMutex;
    bool quit;
};
#endif
