#include "differentialsynctasks.h"

int const APPLY_QUIT = 0x2000;
int const APPLY_DATA = 0x2001;
int const APPLY_DONE = 0x2002;

int const APPLY_NORMAL_PRIORITY = 0x01;
int const APPLY_HIGH_PRIORITY   = 0x10;

int GetDifferentialTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

int GetDifferentialTask::close(u_long flags)
{ 
    return 0;
}

int GetDifferentialTask::svc()
{
    for (int i = 0; i < 10; ++i) {  
        ACE_Message_Block *mb = new ACE_Message_Block(0, APPLY_DATA);
        mb->msg_priority(APPLY_NORMAL_PRIORITY);
        m_ApplyTask->msg_queue()->enqueue_prio(mb);        
    }

    ACE_Message_Block *mb = new ACE_Message_Block(0, APPLY_DONE);
    mb->msg_priority(APPLY_NORMAL_PRIORITY);
    m_ApplyTask->msg_queue()->enqueue_prio(mb);

    return 0;
}

int ApplyDifferentialTask::open(void *args)
{
    return this->activate(THR_BOUND);
    return 0;
}

int ApplyDifferentialTask::close(u_long flags)
{
    return 0;
}

int ApplyDifferentialTask::svc()
{
    int i = 0;
    bool quit = false;
    while(!quit)
    {        
        ACE_Message_Block *mb;
        if (-1 == this->msg_queue()->dequeue_head(mb)) {
            break;
        }
        switch(mb->msg_type()) {
            case APPLY_QUIT:        
                quit = true;

            case APPLY_DATA:
                break;

            case APPLY_DONE:
                quit = true;

            default:        
                break;
        }
        mb->release();        
    }
    return 0;
}
