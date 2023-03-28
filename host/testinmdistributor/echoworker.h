#ifndef ECHO_WORKER_H
#define ECHO_WORKER_H

#include "additiondistributor.h"

class EchoWorker : public InmDistributor::Worker
{
public:
    EchoWorker(AdditionDistributor *pad);
    int svc(void);

private:
    AdditionDistributor *m_pAdditionDistributor;
};

#endif /* ECHO_WORKER_H */
