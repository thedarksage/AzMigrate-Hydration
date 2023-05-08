#include <iostream>
#include "echoworker.h"
#include "logger.h"

const int WaitTime = 5;

EchoWorker::EchoWorker(AdditionDistributor *pad)
 : m_pAdditionDistributor(pad)
{
}


int EchoWorker::svc(void)
{
    do
    {
        void *pv = ReadFromDistributor(WaitTime);
        if (pv)
        {
            int *pi = (int *)pv;
            std::cout << "calling add with: " << *pi;
            m_pAdditionDistributor->add(*pi);
        }
    } while (!ShouldQuit(WaitTime));

    return 0;
}
