#pragma warning(disable: 4786)

#include <stdio.h>
#include <algorithm>
#include <time.h>
#include "EventGenarator.h"
#include "ExecutionController.h"
#include "CXConnection.h"
#include "IRepositoryManager.h"
#include "thread.h"
#include "common.h"
#include "Log.h"

using namespace std;
using namespace sch;

void EventGenarator::generateEvent2(void *context)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::generateEvent2");
try
{
	if(context != NULL)
	{
		EventGenarator* eventGen = static_cast<EventGenarator*>(context);

		if(eventGen != NULL)
		{
			eventGen->generateEvent();
		}
	}
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "generateEvent2 ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "generateEvent2 Defualt exeception raised");
}
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::generateEvent2");
	int ret = 0;
	pthreadExit(&ret);	
}

SCH_STATUS EventGenarator::setEvent(time_t startTime,PERP perp)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::setEvent");
	SCH_STATUS lRet = SCH_SUCCESS;
try
{
	if(perp == NULL)
	{
		Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "Invalid execution request");
		lRet = SCH_ERROR;
	}
	if(perp != NULL && perp->getRepositoryManager() == NULL)
	{
		Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "Repository Manager not set appropriately");
		lRet = SCH_ERROR;
	}

	if(perp != NULL && perp->getId() == -1)
	{
		Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "Invalid ERP Id not properly set");
		lRet = SCH_ERROR;
	}
	//The startTime for this event should be > current time
	if(startTime<time(NULL))
	{
		Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "Start time is less than current time, Hence ERP not scheduled");
		lRet = SCH_ERROR;
	}
	if(lRet != SCH_ERROR)
	{
		//Repository Manager check
		m_Mutex.lock();

		//This is a map to keep startTime and associated events
		map<unsigned long,vector<PERP> >::iterator iter = m_EventMap.find(startTime);
		
		if( iter == m_EventMap.end())
		{
			vector<PERP> vperp;
			vperp.push_back(perp);

			m_EventMap.insert(make_pair(startTime,vperp));

			vector<unsigned long>::iterator iter2 = m_EventQueue.begin();

			for( ; iter2 != m_EventQueue.end() && *iter2 < (unsigned long)startTime; iter2++){ }

			m_EventQueue.insert(iter2,startTime);
		}
		else
		{
			vector<PERP>& vEvents = iter->second;
			vEvents.push_back(perp);
		}

		m_Mutex.unlock();
	}
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "setEvent ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "setEvent Defualt exeception raised");
}
	//Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::setEvent");
	return lRet;
}

SCH_STATUS EventGenarator::init()
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::init");
	m_Stop = 0;				//Set event generator state to running.

	//Create thread for generation of time based triggers.
	THREAD_T thrId	= 0;
try
{
	int hr = pthreadCreate(&thrId,EventGenarator::generateEvent2,this);

	if( hr != SCH_SUCCESS)
	{
		if(hr == SCH_EAGAIN)
		{
			//Sleep for 100 msec and try again.
			sch::Sleep(100);

			hr = pthreadCreate(&thrId,EventGenarator::generateEvent2,this);

			if(hr != SCH_SUCCESS)
			{
				Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "pthreadCreate failed..with error=%d ",hr);
				Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::init");
				return SCH_ERROR;
			}
			else
			{
				Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::init");
				return SCH_SUCCESS;
			}
		}
		else
		{
			Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "pthreadCreate failed.. with error=%d",hr);
			return SCH_ERROR;
		}
	}
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "init ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "init Defualt exeception raised");
}
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::init");
	return SCH_SUCCESS;
}

void EventGenarator::generateEvent()
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::generateEvent");
try
{
	while(m_Stop == 0)
	{
		m_Mutex.lock();

		if(m_EventQueue.size() != 0)
		{
			unsigned long curTime = time(NULL);

			if(curTime >= m_EventQueue[0])
			{
				PEC pec = ExecutionController::instance();
                if(pec == NULL)
                {
                    //
                    //This should never be reached;
                    //
					Logger::Log("EventGenerator.cpp", __LINE__, 2, 1, "ExecutionController::instance failed");
                    m_Mutex.unlock();
                    removeAll();
                }
				else
				{
					map<unsigned long,vector<PERP> >::iterator iter = m_EventMap.find(m_EventQueue[0]);
					iter->second.size();

					for(unsigned int i = 0; i < iter->second.size(); i++)
					{
						ERP* erp = iter->second[i];

						//Call execution controller callback function for scheduling the ERP.
						pec->executeAndSchedule(iter->second[i],iter->second[i]->getRepositoryManager());
					}

					m_EventMap.erase(m_EventQueue[0]);
					m_EventQueue.erase(m_EventQueue.begin());
					
				}
			}
		}
		m_Mutex.unlock();
		sch::Sleep(5000);
	}
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "generateEvent ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "generateEvent Defualt exeception raised");
}

	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::generateEvent");
}

void EventGenarator::stopEventGenerator()
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::stopEventGenerator");
try
{
	m_Stop = 1; //Stop the event generator thread
    removeAll();
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "stopEventGenerator ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "stopEventGenerator Defualt exeception raised");
}

	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::stopEventGenerator");
}

void EventGenarator::removeAll()
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::removeAll");
try
{
	m_Mutex.lock();

	
	for(unsigned int i = 0; i < m_EventQueue.size(); i++)
	{
		m_EventMap.erase(m_EventQueue[i]);	//Remove the entry from map
	}

    m_EventQueue.clear();   //Delete all the events from the queue

    m_Mutex.unlock();
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "removeAll ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "removeAll Defualt exeception raised");
}

    return;
}

PERP EventGenarator::remove(ERP* erp)
{
	ERP *erpToDelete =  NULL;
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "EventGenarator::remove");
try
{
	if(erp != NULL && erp->getId() != -1)
	{
		m_Mutex.lock();

		map<unsigned long,vector<PERP> >::iterator iter = m_EventMap.begin();

		bool erpDeleted		= false;
		while(iter != m_EventMap.end())
		{
			vector<PERP>::iterator iter2 = iter->second.begin();

			while(iter2 != iter->second.end())
			{
				ERP *erp2 = *iter2;

				if(erp->getId() == erp2->getId())
				{
					iter->second.erase(iter2);

					if(iter->second.size()==0)
					{
						//Remove the event from the eventqueue
						vector<unsigned long>::iterator queueIter = m_EventQueue.begin();
                        vector<unsigned long> tempVector;
                        
                        for(unsigned int i = 0; i < m_EventQueue.size(); i++, queueIter++)
						{
							if(m_EventQueue[i] == iter->first)
							{
								m_EventMap.erase(m_EventQueue[i]);	//Remove the entry from map also.
                                tempVector.push_back(*queueIter);
							}
						}
                        vector<unsigned long>::iterator tempVectorIter =  tempVector.begin();
                        while(tempVectorIter != tempVector.end())
                        {
                            m_EventQueue.erase(std::remove(m_EventQueue.begin(),m_EventQueue.end(),(*tempVectorIter)), m_EventQueue.end());
                            tempVectorIter++;
                        }
					}

					erpToDelete = erp2;
					erpDeleted = true;
					break;
				}
				iter2++;
			}
			if(erpDeleted)
				break;
			iter++;
		}

		m_Mutex.unlock();
	}
}	
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "remove ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "remove Defualt exeception raised");
}
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "Exiting EventGenarator::remove");
	return erpToDelete;
}

void EventGenarator::printAll()
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "EventGenarator::printAll");
try
{
	m_Mutex.lock();

	map<unsigned long,vector<PERP> >::iterator iter = m_EventMap.begin();

	while(iter != m_EventMap.end())
	{
		printf("Key Execution Time:%d\n",iter->first);
		vector<PERP>::iterator iter2 = iter->second.begin();

		int i=0;

		while(iter2 != iter->second.end())
		{
			ERP *erp2 = *iter2;
			printf("ERPID:%d |",erp2->getId());
			iter2++;
		}

		iter++;
		printf("\n");
	}

	for(unsigned int i = 0; i < m_EventQueue.size(); i++)
	{
		printf("%ld ",m_EventQueue[i]);
	}

	printf("\n\n");
	m_Mutex.unlock();
}
catch(char * str)
{
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "printAll ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("EventGenerator.cpp", __LINE__, 0, 1, "printAll Defualt exeception raised");
}

	Logger::Log("EventGenerator.cpp", __LINE__, 0, 0, "Exiting EventGenarator::printAll");
}


