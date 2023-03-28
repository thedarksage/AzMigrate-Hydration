#ifndef ADDITION_DISTRIBUTOR_H
#define ADDITION_DISTRIBUTOR_H

#include <ace/Guard_T.h>
#include <ace/Thread_Mutex.h>
#include <ace/Recursive_Thread_Mutex.h>
#include "inmqdistributor.h"

class AdditionDistributor : public InmQDistributor
{
public:
    AdditionDistributor();
    ~AdditionDistributor();
    void add(const int &value);
    int Sum(void);

private:
    int m_Sum;
    ACE_Recursive_Thread_Mutex m_AddLock;
};

#endif /* ADDITION_DISTRIBUTOR_H */
