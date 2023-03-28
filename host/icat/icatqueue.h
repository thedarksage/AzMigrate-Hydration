#ifndef ICAT_QUEUE_H
#define ICAT_QUEUE_H
#include "defs.h"

enum ARC_QUEUE_STATE {ARC_Q_NONE, ARC_Q_INUSE, ARC_Q_DONE} ;

class ArcQueue
{
	ACE_SHARED_MQ_Ptr m_queue ;
	ARC_QUEUE_STATE m_state ;
	ArcQueue& operator=(const ArcQueue&) ;
	ArcQueue(const ArcQueue&) ;
public:
	ArcQueue(ACE_SHARED_MQ_Ptr) ;
	ACE_SHARED_MQ_Ptr queue() ;
	ARC_QUEUE_STATE state() ;
	void setState(ARC_QUEUE_STATE ) ;
} ;
typedef boost::shared_ptr<ArcQueue> ArcQueuePtr ;
#endif

