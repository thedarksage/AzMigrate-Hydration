#ifndef __EVENT_GENARATOR_H_
#define __EVENT_GENARATOR_H_

#include <map>
#include <vector>
#include "ERP.h"
#include "Mutex.h"
#include "common.h"

class EventGenarator
{
	int m_Stop;	//0	running, 1 stop

	//This map stores the point of execution and the associated erps to be executed at that point in time.
	std::map <unsigned long, std::vector<PERP> > m_EventMap;	

	//This vector stores all the events to compare with in a sorted order.
	std::vector<unsigned long> m_EventQueue;

	Mutex m_Mutex;		//Synchronization variable for the above two structures.

    unsigned long m_GeneratorThreadHandle;

    void removeAll();

public:
	static void	generateEvent2(void *context);

	SCH_STATUS setEvent(time_t starttime,PERP perp);

	SCH_STATUS init();			//This will start the event generator.

	void generateEvent();	//This function is responsible for generating triggers.

	PERP remove(ERP* erp);

	void printAll();

    void stopEventGenerator();
};

#endif
