#include "icatqueue.h"
#include "icatexception.h"


ArcQueue::ArcQueue(ACE_SHARED_MQ_Ptr queue)
:m_queue(queue)
{
	m_state = ARC_Q_INUSE ;
}
ACE_SHARED_MQ_Ptr ArcQueue::queue()
{
	return m_queue ;
}
ARC_QUEUE_STATE ArcQueue::state()
{
	return m_state ;
}

void ArcQueue::setState(ARC_QUEUE_STATE state)
{
	m_state = state ;
}

