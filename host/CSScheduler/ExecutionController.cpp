#pragma warning(disable: 4786)

#include "ExecutionController.h"
#include "IRepositoryManager.h"
#include "FileRepManager.h"
#include "SnapshotManager.h"
#include <stdio.h>
#include "queue.h"
#include "thread.h"
#include "inmsafecapis.h"
#include "inmageex.h"
using namespace sch;
using namespace std;

ExecutionController* ExecutionController::m_instance = 0;
bool ExecutionController::m_Stop = 0;
THREAD_T ExecutionController::m_ThreadHandlePollNewJobs = 0;
Queue<ERP*> exeQueue;
int ERP::erpCount;

//
//Poll for new Jobs every 10 seconds.
//

void ExecutionController::pollNewJobs(void* context)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::pollNewJobs");
	ExecutionController *ec			= NULL;
	IRepositoryManager *frrepmgr	= NULL;
	IRepositoryManager *snaprepmgr	= NULL;
	ITaskDefinition* tskdef			= NULL;
	ERP* erp						= NULL;

	ec = static_cast<ExecutionController*>(context);
	frrepmgr	= new FileRepManager;
	snaprepmgr	= new SnapshotManager;
try
{
	if( ec != NULL )
	{
		snaprepmgr->LoadScheduledSnapshotVetor(snaprepmgr);
		while(!m_Stop)
		{
			if( frrepmgr != NULL ) 
			{
				erp = frrepmgr->nextERP();
				if(erp != NULL)
				{
					tskdef = erp->getInformation();
					ec->schedule(erp,frrepmgr);
					if( tskdef != NULL )
					{
						Logger::Log("ExecutionController.cpp",__LINE__, 0, 0,"File Replication ERP scheduled Details");
						tskdef->logDetails();
					}
				}
			}
			else
				Logger::Log("ExecutionController.cpp", __LINE__, 2, 0, "Failed to get repository manager");
			if( snaprepmgr != NULL )
			{
				erp = snaprepmgr->nextERP();
				if(erp != NULL)
				{
					tskdef = static_cast<SnapshotDefinition*>(erp->getInformation());
					ec->schedule(erp,snaprepmgr);
					if( tskdef != NULL ) 
					{
						Logger::Log("ExecutionController.cpp",__LINE__, 0, 2,"Snapshot ERP scheduled Details");
						tskdef->logDetails();
					}
				}
			}
			else
				Logger::Log("ExecutionController.cpp", __LINE__, 2, 2, "Failed to get repository manager");
			sch::Sleep(30000);
		}
	}
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 0, "pollNewJobs ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 0, "pollNewJobs  Default Exception raised");
}
	SAFE_DELETE( frrepmgr );
	SAFE_DELETE( snaprepmgr );
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::pollNewJobs");
	int ret = 0;
	sch::pthreadExit(&ret);	
}

void ExecutionController::schedule(PERP erp,IRepositoryManager* repmgr)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::schedule");
	//
	//calculate time for next execution of this job and set the event accordingly.
	//Schedule this erp for execution.
	//
try
{
	if(erp == NULL)
	{
		Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::schedule");
		return;
	}

	ITaskDefinition *tskdef = erp->getInformation();

	if(tskdef != NULL)
	{
		Logger::Log(__FILE__,__LINE__,0,0,"ERP Details");
		erp->logDetails();
	}

	bool bExecute = false;

	if(repmgr != NULL)
	{
		erp->setRepositoryManager(repmgr);
	}
	
	unsigned long erpStartTime		= erp->getStartTime();
	unsigned long curTime			= time(NULL);
	unsigned long lastExecTime		= erp->getLastExecutionTime();
	unsigned long prevScheduledTime = erp->getNextScheduledTime();
    string RunAtTimeStr;
	unsigned long JobRunTime;
	char temp[25];
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ERP Start Time is %s Current Tims is %s Last Execution Time is %s\n" ,
												convertTimeToString(erpStartTime).c_str(), 
												convertTimeToString(curTime).c_str(),
												convertTimeToString(lastExecTime).c_str());
	string szErpTime = convertTimeToString(erpStartTime);
	string szCurTime = convertTimeToString(curTime);

	SchData schdata = erp->getSchData();

	long newStartTime = 0;

	if(lastExecTime == 0)
	{
		if(schdata.everyYear == 0 && schdata.everyMonth == 0)
		{
			if(erp->getRepeatition() == 0)
			{
				if(erpStartTime == 0 && schdata.isEmpty())
				{
					//OnDemand Case. Do not schedule event.
					Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::schedule");
					return;
				}
			}

			if(erpStartTime > curTime)
			{
				newStartTime = erpStartTime;
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
															convertTimeToString(newStartTime).c_str());
				int result = evntgen.setEvent(newStartTime,erp);
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
			}
			else if(erpStartTime < curTime)
			{
				//Execute the ERP first time
				//--------------------------------------------------------------------//
				//The following 4 lines is a consequence of taking out this code from executeERP

				if(erp->getLastExecutionTime() == 0 && erp->getStartTime() != 0)
				{
					erp->setLastExecutionTime(erp->getStartTime());
				}
				else
				{
					erp->setLastExecutionTime(time(NULL));
				}
				//--------------------------------------------------------------------//
			
				inm_sprintf_s(temp,ARRAYSIZE(temp),"%d-%d-%d %d:%d",schdata.atYear,schdata.atMonth,schdata.atDay,schdata.atHour,schdata.atMinute);
				RunAtTimeStr = temp;
				if(RunAtTimeStr.compare("0-0-0 0:0")== 0)
				{
					RunAtTimeStr = "0000-00-00 00:00:00";
				}
			 	JobRunTime =  convertStringToTime(RunAtTimeStr);
                if(JobRunTime != 0)
                {    
					if(JobRunTime < curTime )
				    {
						bExecute = false;
				    }
				    else
				    {  							
						bExecute = true;
				    }
                }
                else
				{
                    bExecute = true;
				}
				if(erp->getRepeatition() == -1)
				{
					newStartTime = erpStartTime;

					//Calculate new start time
					while(newStartTime < time(NULL))
					{
						newStartTime += erp->getFrequency();
					}

					long tmpTime = time(NULL);
					Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
																convertTimeToString(newStartTime).c_str());
					int result = evntgen.setEvent(newStartTime,erp);
					Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
					
				}
			}
			else if(erpStartTime == curTime)
			{
				//If the execution time passed away while queing the erp and if the
				//erp is set for periodic execution then set the next event for execution
				if(evntgen.setEvent(erpStartTime,erp) == -1)
				{
					if(erp->getRepeatition() == -1)
					{
						newStartTime = erpStartTime + erp->getFrequency();
						Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
																	convertTimeToString(newStartTime).c_str());
						int result = evntgen.setEvent(newStartTime,erp);
						Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);						
					}
				}
			}
		}
		else
		{
			newStartTime = getNextStartTime(schdata,curTime);

			if(newStartTime == curTime)
			{
				bExecute = true;

				//Execute the ERP first time
				//--------------------------------------------------------------------//
				//The following line is a consequence of taking out this code from executeERP
				if(erp->getLastExecutionTime() == 0 && erp->getStartTime() != 0)
				{
					erp->setLastExecutionTime(erp->getStartTime());
				}
				else
				{
					erp->setLastExecutionTime(time(NULL));
				}
				//--------------------------------------------------------------------//
			}
			else
			{
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
																convertTimeToString(newStartTime).c_str());
				int result = evntgen.setEvent(newStartTime,erp);
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
				
			}
		}

		if(erpStartTime == 0)
		{
			Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ERP START TIME IS 0\n");
			prevScheduledTime = newStartTime;
		}
		else
		{
			prevScheduledTime = erpStartTime;
		}
	}
	else if(erp->getRepeatition() == -1)
	{
		if(prevScheduledTime == 0)
		{
			prevScheduledTime = curTime;
		}	
		if(schdata.everyYear == 0 && schdata.everyMonth == 0)
		{
			if(lastExecTime < curTime)
			{
				newStartTime = prevScheduledTime;

				//Calculate new start time
				while(newStartTime < time(NULL))
				{
					newStartTime += erp->getFrequency();
				}

				long tmpTime = time(NULL);
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
																	convertTimeToString(newStartTime).c_str());
				int result = evntgen.setEvent(newStartTime,erp);
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
				
			}
			else if(lastExecTime == curTime)
			{
				newStartTime = prevScheduledTime + erp->getFrequency();
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
																		convertTimeToString(newStartTime).c_str());
				int result = evntgen.setEvent(newStartTime,erp);
				Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
				
			}
		}
		else
		{
			newStartTime = getNextStartTime(schdata,curTime + 1);
			Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Scheduling a event at Time %s\n",
														convertTimeToString(newStartTime).c_str());
			int result = evntgen.setEvent(newStartTime,erp);
			Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "setEvent returned %d\n",result);
			
		}
	}

	erp->setPrevScheduledTime(prevScheduledTime);
	erp->setNextScheduledTime(newStartTime);

	if(bExecute)
	{
		erp->setRepositoryManager(repmgr);
		executeERP(erp,erp->getRepositoryManager());
	}
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::schedule Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::schedule Default Exception raised");
}	
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::schedule");
	
}

bool ExecutionController::executeERP(PERP perp,IRepositoryManager* repmgr)
{
	//Execute the erp once without scheduling it for the next execution
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeERP");
	ITaskDefinition *tskdef		= NULL;
	Status *status				= NULL;
try
{
	if( perp != NULL )
	{
		if(perp->getInformation() != NULL)
		{
			//Create a copy of the user paramter structure
			tskdef = perp->getInformation()->clone();
			Logger::Log("ExecutionController.cpp",__LINE__,0, 1, "ERP Details");
			tskdef->logDetails();
		}
		if ( repmgr != NULL ) 
		{
			status = new Status;
			if( status != NULL )
			{
				status->executedTime		= time(NULL);
				status->frequency			= perp->getFrequency();
				status->repeat				= perp->getRepeatition();
				status->nextExecutionTime	= perp->getNextScheduledTime();
				status->Success				= SCH_SUCCESS;

				//Callback to the repository manager on queuing the erp for execution
				repmgr->onSchedule(status,tskdef);
			
				PERP perpTmp = new ERP(perp);
				exeQueue.enqueue(perpTmp);
			}
			else
				Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Failed to create Status Object");
		}
		else
			Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Failed get repository manager");
	}
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeERP Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeERP Default Exception raised");
}
	SAFE_DELETE( status );
	SAFE_DELETE( tskdef );
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::executeERP");
	return 0;
}

bool ExecutionController::execute(PERP erp,IRepositoryManager* repmgr)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::execute");
	//Execute the erp once without scheduling it for the next execution
try
{
	if(erp != NULL)
	{
		//--------------------------------------------------------------------//
		//The following 4 lines is a consequence of taking out this code from executeERP
		if(erp->getLastExecutionTime() == 0 && erp->getStartTime() != 0)
		{
			erp->setLastExecutionTime(erp->getStartTime());
		}
		else
		{
			erp->setLastExecutionTime(time(NULL));
		}

		erp->setRepositoryManager(repmgr);
		//--------------------------------------------------------------------//

		executeERP(erp,repmgr);
	}
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::execute Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::execute Default Exception raised");
}	
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::execute");
	return 0;
}

bool ExecutionController::executeAndSchedule(PERP erp,IRepositoryManager* repmgr)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeAndSchedule");
	//--------------------------------------------------------------------//
	//The following 4 lines is a consequence of taking out this code from executeERP
try
{
	if(erp->getLastExecutionTime() == 0 && erp->getStartTime() != 0)
	{
		erp->setLastExecutionTime(erp->getStartTime());
	}
	else
	{
		erp->setLastExecutionTime(time(NULL));
	}

	erp->setRepositoryManager(repmgr);
	//--------------------------------------------------------------------//

	//Schedule for next execution.
	schedule(erp,repmgr);
	executeERP(erp,repmgr);
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeAndSchedule Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::executeAndSchedule Default Exception raised");
}	
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::executeAndSchedule");
	return 0;
}


void ExecutionController::LoadERPQueue(IRepositoryManager* repmgr)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::LoadERPQueue");
	//Load new erps from the repository manager
	ERP* erp				= NULL;
	TaskDefinition* def		= NULL;
try
{
	while(1)
	{
		erp = repmgr->nextERP();
		if(erp == NULL)
			break;
		def = static_cast<TaskDefinition*>(erp->getInformation());
		schedule(erp,repmgr);
	}
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::LoadERPQueue Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::LoadERPQueue Default Exception raised");
}
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::LoadERPQueue");
}

bool ExecutionController::start(PERP erp,IRepositoryManager* repmgr)
{
	//Start a scheduled job asyncronously and adjust the schedule for the next execution
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::start.. empty function..");
	return 0;
}

bool ExecutionController::stop()
{
	//Stop a job
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::stop.. empty function..");
	return true;
}

bool ExecutionController::updateStatus(ERPID id)
{
	//Update Status with IRepositoryManager
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::updateStatus.. empty function..");
	return true;
}

bool ExecutionController::removeERP(PERP perp)
{
	//Remove a job permanently
	//Get the original erp of which this erp is a copy and also remove it from the event generator
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::removeERP");
	PERP erpScheduled			= NULL;
	ITaskDefinition *tskdef		= NULL;
	IRepositoryManager *repmgr	= NULL;
	Status *status				= NULL;
try
{
	if( perp != NULL ) 
	{
		repmgr = perp->getRepositoryManager();

		if( repmgr != NULL )
		{
			erpScheduled = evntgen.remove(perp);
			tskdef = perp->getInformation()->clone();
			status = new Status;
			if( status != NULL )
			{
				status->executedTime		= perp->getLastExecutionTime();
				status->frequency			= perp->getFrequency();
				status->repeat				= perp->getRepeatition();
				status->nextExecutionTime	= perp->getNextScheduledTime();
				status->Success				= SCH_SUCCESS;

				repmgr->onRemove(tskdef,status);
				perp->setExecutionStatus(SCH_DELETED);
			}
			else
				Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Failed to create Status Object");
		}
		else
			Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Failed to get repository manager");
	}
	else
		Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Invalid Execution Object");
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::removeERP Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::removeERP Default Exception raised");
}
	SAFE_DELETE( status );
	SAFE_DELETE( tskdef );
	SAFE_DELETE( erpScheduled );
	//
	//Delete the original ERP scheduled in event generator.
	//
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "exiting ExecutionController::removeERP");
	return true;
}

void ExecutionController::notify(PERP perp)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::notify");
	Status *status			= NULL;
	ITaskDefinition *tskdef = NULL;
try
{
	if( perp != NULL )
	{
		if(perp->m_CompletionRoutine != NULL)
			perp->m_CompletionRoutine(perp,true);

		status = new Status;
		if( status != NULL )
		{
			status->executedTime		= time(NULL);
			status->frequency			= perp->getFrequency();
			status->repeat				= perp->getRepeatition();
			status->nextExecutionTime	= perp->getNextScheduledTime();
			status->Success				= perp->getExecutionStatus();

			if(perp->getInformation() != NULL)
				tskdef = dynamic_cast<ITaskDefinition*>(perp->getInformation()->clone());

			if(perp->getRepositoryManager() != NULL)
				perp->getRepositoryManager()->onFinished(status,tskdef);
		}
		else
			Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Failed to create Status Object");
	}
	else 
		Logger::Log("ExecutionController.cpp", __LINE__, 2, 1, "Invalid Execution Object");
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::notify Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::notify Default Exception raised");
}
	SAFE_DELETE( status );
	SAFE_DELETE( tskdef );
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::notify");
}

void ExecutionController::stopController()
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::stopController");
try
{
	evntgen.stopEventGenerator();   //Stop Event generator

	m_Stop = true;
	/*
	   Waiting for thread here is not currect. 
	   This routine will be called from singal handler fuction and it should respond quickly by setting stop flags.
	   Hence commented the following thread-wait code line.
	*/
    //sch::pthreadJoin(m_ThreadHandlePollNewJobs);
}
catch(char * str)
{
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::stopController Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "ExecutionController::stopController Default Exception raised");
}
	Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Exiting ExecutionController::stopController");
}
