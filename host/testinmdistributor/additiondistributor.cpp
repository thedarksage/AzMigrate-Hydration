#include "additiondistributor.h"


AdditionDistributor::AdditionDistributor():
m_Sum(0)
{
}


AdditionDistributor::~AdditionDistributor()
{
}


void AdditionDistributor::add(const int &value)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard( m_AddLock );
    m_Sum += value;
}


int AdditionDistributor::Sum(void)
{
    return m_Sum;
}

