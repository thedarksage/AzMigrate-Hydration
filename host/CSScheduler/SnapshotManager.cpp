#pragma warning(disable: 4786)

#include "SnapshotManager.h"
#include "ExecutionObject.h"
#include "svconfig.h"
#include "Log.h"
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "scopeguard.h"

static boost::mutex g_jobvectorLock; //scope guard lock for snapshot vector


#define QUERY_SIZE 1024

SnapshotUtlClass *SnapshotUtlClass::m_Instance = NULL;

int IsDayLightSavingOn( void );
string getNewsnapshotId(Connection *con);

void snapshotCompletionRoutine(ERP *erp,bool success)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "snapshotCompletionRoutine");
	Connection *con			= NULL;
try
{
	std::stringstream szSQL;
	if(erp->getExecutionStatus() != SCH_DELETED)
	{
		//
		//Reschedule ERP for next execution if it is a periodic ERP
		//
		if(erp->getExecutionStatus() != SCH_PENDING)
		{
			

			//task id should be -1 for subsequent fetch
			SnapshotManager *repmgr = dynamic_cast<SnapshotManager *>(erp->getRepositoryManager());

			if(repmgr != NULL)
			{
				
				con			= createConnection();

				if(con != NULL )
				{
					SnapshotDefinition *tskdef = static_cast<SnapshotDefinition *>(erp->getInformation());

					if(tskdef != NULL)
					{
						szSQL << "select pending, bytesSent, status, destHostid, destDeviceName from snapShots where id = " << tskdef->szSnapshotId;
						DBResultSet lInmrset	= con->execQuery((szSQL.str().c_str()));
						
						long pending		= 0;
						long bytesent		= 0;
						long status			= 0;
						string dHostid		= "";
						string dDeviceName	= "";

						if (!lInmrset.isEmpty())
						{
							DBRow row		= lInmrset.nextRow();
							pending			= row.nextVal().getInt();
							bytesent		= row.nextVal().getInt();
							status			= row.nextVal().getInt();
							dHostid			= row.nextVal().getString();
							dDeviceName		= row.nextVal().getString();
						}
						szSQL.str("");
						szSQL << "select a.repeat from snapSchedule a, snapshotMain b where a.Id = b.snapScheduleId and b.snapshotId = " << tskdef->taskId;
						lInmrset = con->execQuery((szSQL.str().c_str()));

						if(!lInmrset.isEmpty())
						{
							DBRow row	= lInmrset.nextRow();
							long repeat	= row.nextVal().getInt();

							if (repeat == 1)
							{
								szSQL.str("");
								szSQL << "update snapshotMain set status = " << status <<", bytesSent = " << bytesent << " , pending =  " << 1 <<  " where snapshotId = " << tskdef->taskId;								
								con->execQuery((szSQL.str().c_str()));
							}
							else
							{
								szSQL.str("");
								szSQL << "update snapshotMain set status = " << status << " , bytesSent = " << bytesent << " , pending = " << 0 << " where snapshotId = " << tskdef->taskId;								
								con->execQuery((szSQL.str().c_str()));	
							}
						}
						szSQL.str("");
						szSQL << "select a.id, a.snapshotId , b.snapshotId from snapShots a, snapshotMain b where a.destDeviceName = '" << dDeviceName.c_str() << "' and a.destHostid = '" << dHostid.c_str() <<  "' and a.executionState = 0 and a.snapshotId = b.snapshotId and b.deleted = 0";
						lInmrset = con->execQuery((szSQL.str().c_str()));

						if (!lInmrset.isEmpty())
						{
							//Execute the next ERP which is blocked because of this ERP since the snapshotDrive being used by the current ERP
							DBRow row	= lInmrset.nextRow();
							string mid	= row.nextVal().getString();
							long snapId = row.nextVal().getInt();
							szSQL.str("");
							szSQL << "update snapShots set executionState = 1 where id = " << mid;
							con->execQuery((szSQL.str().c_str()));

							// call execute function to run job async.
							ERP* nerp				= repmgr->createAsyncERP(snapId, mid);

							if(nerp != NULL)
							{
								ExecutionController *ec = ExecutionController::instance();

								if(ec != NULL)
									ec->execute(nerp, repmgr);
								//
								//Delete this erp as the execute function internally make a copy of the 
								//oroginal erp.
								
								
								SAFE_DELETE( nerp );
							}
						}
						else
						{
							//
							//Look for any recovery jobs
							//
							szSQL.str("");
							szSQL << "select id, snapshotId from snapShots where destDeviceName = '" << dDeviceName.c_str() << "' and destHostid = '" << dHostid.c_str() << "' and executionState = 0 and snapshotId = 0";
							lInmrset = con->execQuery((szSQL.str().c_str()));

							if (!lInmrset.isEmpty())
							{
								//Execute the next ERP which is blocked because of this ERP since the snapshotDrive being used by the current ERP
								DBRow row	= lInmrset.nextRow();
								string mid	= row.nextVal().getString();
								long snapId = row.nextVal().getInt();

								szSQL.str("");
								szSQL << "update snapShots set executionState = 1 where id = " << mid;
								con->execQuery((szSQL.str().c_str()));

								// call execute function to run job async.
								ERP* nerp				= repmgr->createAsyncERP(snapId, mid);
								if(nerp != NULL)
								{
									ExecutionController *ec = ExecutionController::instance();

									if(ec != NULL)
										ec->execute(nerp, repmgr);
									//
									//Delete this erp as the execute function internally make a copy of the 
									//oroginal erp.
									//
									SAFE_DELETE( nerp );
								}
							}
						}
					}
				}
			}
		}
	}
}
catch(char * str)
{
Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Exception raised=%s",str);

}
catch(...) 
{  
Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting snapshotCompletionRoutine");
	return;
}

void SnapshotManager::LoadScheduledSnapshotVetor(IRepositoryManager* repmgr)
{
	std::stringstream szSQL;
	vector<string> sched_jobs;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::LoadScheduledSnapshotVetor");
    
try
{
        szSQL << "select snapshotId from snapshotMain where pending = 1 and deleted = 0 and typeOfSnapshot IN (1,3)";
		DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
		for(unsigned int i = 0; i < lInmrset.getSize(); i++)
		{
			
			DBRow row	= lInmrset.nextRow();
			long id		= row.nextVal().getInt();
        	
			szSQL.str("");
			szSQL << "select count(*) from snapShots where snapshotId = " << id;

    			DBResultSet rset1 = m_InmCon->execQuery((szSQL.str().c_str()));
				DBRow row1   = rset1.nextRow();
	            long count  = row1.nextVal().getInt();
				if(count != 0)
				{
					LoadERPS(id,repmgr);
					Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2,"ID pushed into vector =%ld",id);	
				}	
		
				
		}
		
	
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Default Exception raised");
}
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::LoadScheduledSnapshotVetor");
}


void SnapshotManager::LoadERPS(long snapshotId,IRepositoryManager* repmgr)
{
    Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Entering SnapshotManager::LoadERPS");
    
    ERP* erp                = NULL;
    
	ExecutionController *ec = ExecutionController::instance();
    
	
try
{
	   std::stringstream szSQL;
	   szSQL << "select snapScheduleId from snapshotMain where snapshotId = " << snapshotId;
	   DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
	   if(!lInmrset.isEmpty())
	   {
			DBRow row	= lInmrset.nextRow();
			long schid	= row.nextVal().getInt();
			
			szSQL.str("");
			szSQL <<  "select * from snapSchedule where Id = " << schid;
			lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
			if(!lInmrset.isEmpty())
			{
				DBRow row           = lInmrset.nextRow();
				long tmpId      = row.nextVal().getInt();
				unsigned long startTime = convertStringToTime(row.nextVal().getString());
				if(startTime == 0)
				{
					startTime = 0;
				}
				long repeat     = row.nextVal().getInt();
				if(repeat == 1)
				{
					repeat = -1;
				}
				SchData schdata;
				schdata.everyDay        = row.nextVal().getInt();
				schdata.everyHour       = row.nextVal().getInt();
				schdata.everyMinute     = row.nextVal().getInt();
				schdata.everyMonth      = row.nextVal().getInt();
				schdata.everyYear       = row.nextVal().getInt();
				schdata.atYear          = row.nextVal().getInt();
				schdata.atMonth         = row.nextVal().getInt();
				schdata.atDay           = row.nextVal().getInt();
				schdata.atHour          = row.nextVal().getInt();
				schdata.atMinute        = row.nextVal().getInt();
				int sType               = row.nextVal().getInt();

				erp = new ERP;
				erp->setSchData(schdata);
				erp->setType(2);
				erp->setSynchronous(false);
				erp->setStartTime(startTime);
				erp->setLastExecutionTime(0);
				if(repeat)
				{
					erp->setRepeatition(-1);
				}    
				else
				{
					erp->setRepeatition(0);
				}

				BaseObject *obj = new BaseSnapshot;
				erp->setObject(obj);

				SnapshotDefinition *tskdef  = new SnapshotDefinition;
				tskdef->taskId              = snapshotId;
				tskdef->scheduleId          = tmpId;
				tskdef->taskGroupId         = 0;
				tskdef->szSnapshotId        = "";

				int sz = sizeof(*tskdef);

				erp->setInformation(tskdef);
				erp->setSizeInformation(sizeof(tskdef));
				if(repeat)
				{
					erp->setFrequency(schdata.everyDay * 24 * 60 * 60 + schdata.everyHour * 60 * 60 + schdata.everyMinute * 60);
				}
				erp->setCompletionRoutine(snapshotCompletionRoutine);
				erp->setRepositoryManager(repmgr);
				int result = ec->setEvents(startTime,erp);
				if( result != 0)
				{
					Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Set Event is failed inserting into Vector\n");
					m_ScheduledJobs.push_back(snapshotId);
				}
				else
				{
					Logger::Log("ExecutionController.cpp", __LINE__, 0, 1, "Inserted into  EventQ \n");
				}	
				
			}	
	    }
    
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "snapshotCompletionRoutine::Default Exception raised");
}
	
    Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::LoadERPS");
}
SnapshotManager::SnapshotManager()
{
	m_InmCon = NULL;
	m_InmCon = new CXConnectionEx();
	m_Cnt = 0;
}

//Callbacks that are called by the scheduler on start of execution and on end of execution resp.
//context points to the copy of the job manager defined structure
void SnapshotManager::onSchedule(Status *status,void *context)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::onSchedule");
	Connection* con			= NULL;
	con						= createConnection();
 try
 {
	
	std::stringstream szSQL;
	if(con != NULL )
	{
		SnapshotDefinition *tskdef = static_cast<SnapshotDefinition*>(context);

		if(tskdef != NULL)
		{
			if(tskdef->taskId != 0)
			{
				//update maintable for lastexecuted
				

				// VSNAPFC - Persistence
				// In case of Linux
				// destDeviceName - /dev/vs/cx
				// mountedOn - /mnt/vsnap1
				// selecting mountedOn from snapshotMain 
 
				szSQL << "select destDeviceName, mountedOn from snapshotMain where snapshotId = " << tskdef->taskId;
				DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

				string volStr	= "";
				string mountStr = "";

				if(!lInmrset.isEmpty())
				{
					DBRow row	= lInmrset.nextRow();
					volStr		= row.nextVal().getString();
					mountStr	= row.nextVal().getString();
				}



				Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::onSchedule destDeviceName = %s\t mountedOn = %s\n", volStr.c_str(), mountStr.c_str());

                    size_t outlen;
                    INM_SAFE_ARITHMETIC(outlen = InmSafeInt<size_t>::Type(volStr.size()) * 2 + 1, INMAGE_EX(volStr.size()))
					char* output = new char[outlen];
					inm_sql_escape_string(output, (char*)volStr.c_str(), volStr.size());
					volStr = std::string(output);
					SAFE_DELETE_ARRAY( output );
				
				
				szSQL.str("");
				szSQL << "update snapshotMain set lastScheduledVol = '" << volStr.c_str() << "' ,lastUpdateTime = '" << convertTimeToString(status->executedTime).c_str() << "' where snapshotId = " << tskdef->taskId;
				con->execQuery((szSQL.str().c_str()));

				
				// VSNAPFC - Persistence
				// to set the mountedOn in snapShots table,
				// so that VsnapRemountVolume will get this for scheduled snapshots

				if(!mountStr.empty())
				{
                    size_t mountlen;
                    INM_SAFE_ARITHMETIC(mountlen = InmSafeInt<size_t>::Type(mountStr.size()) * 2 + 1, INMAGE_EX(mountStr.size()))
					char* mounted = new char[mountlen];
					inm_sql_escape_string(mounted, (char*)mountStr.c_str(), mountStr.size());
					mountStr = std::string(mounted);
					SAFE_DELETE_ARRAY( mounted );
					

					szSQL.str("");
					szSQL << "update snapShots set mountedOn = '" << mountStr << "' where snapshotId = " << tskdef->taskId;
					con->execQuery((szSQL.str().c_str()));

				}


			}
		}
	}
 }
 catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "onSchedule::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "onSchedule::Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::onSchedule");
}

void SnapshotManager::onFinished(Status *status,void *context)
{ 
	SnapshotDefinition *tskdef = static_cast<SnapshotDefinition *>(context);
	m_RunningEventSnapShots.erase(tskdef->szSnapshotId);
	vector<long>::iterator scheduledJob;
    vector<long>::iterator temp;
try
{
	for(scheduledJob = m_ScheduledJobs.begin(); scheduledJob != m_ScheduledJobs.end(); scheduledJob++)
    {
		if( *scheduledJob == tskdef->taskId)
        {
            temp = scheduledJob;
			Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Removed scheduled id from vector = %ld",tskdef->taskId);
			boost::mutex::scoped_lock gaurd(g_jobvectorLock);
			m_ScheduledJobs.erase(temp);
			m_Cnt--;
			break;
		}	
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "onFinished::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "onFinished::Default Exception raised");
}

}

void SnapshotManager::onStart(void *context)
{
	//This function is called when a new job is scheduled aynchronously to the scheduler.
	Logger::Log("SnapshotManager.cpp", __LINE__, 1, 1, "Exiting SnapshotManager::onStart.. It has empty body");
	return;
}

//This function is called when a job is removed from the scheduler.
void SnapshotManager::onRemove(void *context,Status *status){ 
	Logger::Log("SnapshotManager.cpp", __LINE__, 1, 1, "Exiting SnapshotManager::onRemove.. It has empty body");
}

//Status is a structure allocated containing the status 
void SnapshotManager::onStop(Status *status,void *context){
	Logger::Log("SnapshotManager.cpp", __LINE__, 1, 1, "Exiting SnapshotManager::onStop.. It has empty body");
}

void SnapshotManager::getNewJobs()
{
	
	vector<string> sched_jobs;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::getNewJobs");
try
{
	std::stringstream szSQL;
	if(m_ScheduledJobs.size() > 0)
	{
		string szScheduled;

		for(unsigned int i = 0; i < m_ScheduledJobs.size(); i++)
		{
			
			std::stringstream szBuffer;
			szBuffer << getSnapshotId(i);
			szScheduled += szBuffer.str();
			szScheduled += ",";
		}

		if(!szScheduled.empty())
		{
			szScheduled.erase(szScheduled.size() - 1, 1);
		}

        szSQL.str("");
        szSQL << "select snapshotId from snapshotMain where snapshotId not in ( " << szScheduled.c_str() << " ) and pending = 1 and deleted = 0 and typeOfSnapshot IN (1,3)";
	}
	else
	{   
		szSQL.str("");
		szSQL << "select snapshotId from snapshotMain where pending = 1 and deleted = 0 and typeOfSnapshot IN (1,3)";
	}
	
   

		DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
		for(unsigned int i = 0; i < lInmrset.getSize(); i++)
		{
		
			DBRow row	= lInmrset.nextRow();
			long id		= row.nextVal().getInt();
        	
			szSQL.str("");
			szSQL << "select count(*) from snapShots where snapshotId = " << id;

    		
				DBResultSet rset1 = m_InmCon->execQuery((szSQL.str().c_str()));
				DBRow row1   = rset1.nextRow();
	            long count  = row1.nextVal().getInt();
				if(count == 0)
				{
					m_ScheduledJobs.push_back(id);
					Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2,"ID pushed into vector =%ld",id);	
				}	
		
			
		
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "getNewJobs::Exception raised=%s",str);
}
catch (const std::out_of_range& oor)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "getNewJobs::Out of Range error Exception = %s", oor.what());
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "getNewJobs::Default Exception raised");
}	
	
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::getNewJobs");
}

void SnapshotManager::executeAsyncERP()
{
	std::stringstream szSQL;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::executeAsyncERP");

	//Execute any asynchronously scheduled ERPs.

try
{
	szSQL << "select id from snapShots where snapshotId = 0 and executionState = 0";
	DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));

	if(!lInmrset.isEmpty())
	{
		for(unsigned int i = 0; i < lInmrset.getSize(); i++)
		{
			DBRow row				= lInmrset.nextRow();
			string szSnapshotId		= row.nextVal().getString();
			ERP *erp				= createAsyncERP(0,szSnapshotId);
            if(erp != NULL)
            {
			    ExecutionController *ec = ExecutionController::instance();

                if(ec != NULL)
			        ec->execute(erp,this);
			    //
			    //Delete this erp as the execute function internally make a copy of the 
			    //oroginal erp.
			    //
			    SAFE_DELETE( erp );
            }
		}
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "executeAsyncERP::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "executeAsyncERP::Default Exception raised");
}	
	

	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::executeAsyncERP");
}

//Fetch all records from database which are to be scheduled;
PERP SnapshotManager::nextERP()
{
	ERP *erp = NULL;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::nextERP");
try
{
	std::stringstream szSQL;
	if(m_InmCon != NULL )
	{

		executeAsyncERP();
		getNewJobs();
		EventSnapshotWorker();
		
		if(m_Cnt < m_ScheduledJobs.size())
		{
			vector<long>::iterator iter = m_ScheduledJobs.begin();
			unsigned int i=0;

			for(i = 0; i < m_Cnt; i++, iter++){ }

			if(iter != m_ScheduledJobs.end())
			{
				

				szSQL << "select snapScheduleId from snapshotMain where snapshotId = " << getSnapshotId(i);
				DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));

				if(!lInmrset.isEmpty())
				{
					DBRow row	= lInmrset.nextRow();
					long schid	= row.nextVal().getInt();

					szSQL.str("");
					szSQL << "select * from snapSchedule where Id = " << schid;
					lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));

					if(!lInmrset.isEmpty())
					{

						row			= lInmrset.nextRow();

						long tmpId		= row.nextVal().getInt();
						long startTime	= convertStringToTime(row.nextVal().getString());

						if(startTime == 0)
						{
							startTime = 0;
						}

						long repeat		= row.nextVal().getInt();

						if(repeat == 1)
						{
							repeat = -1;
						}

						SchData schdata;

						schdata.everyDay		= row.nextVal().getInt();
						schdata.everyHour		= row.nextVal().getInt();
						schdata.everyMinute		= row.nextVal().getInt(); 
						schdata.everyMonth		= row.nextVal().getInt(); 
						schdata.everyYear		= row.nextVal().getInt(); 
						schdata.atYear			= row.nextVal().getInt(); 
						schdata.atMonth			= row.nextVal().getInt(); 
						schdata.atDay			= row.nextVal().getInt(); 
						schdata.atHour			= row.nextVal().getInt(); 
						schdata.atMinute		= row.nextVal().getInt(); 
						int sType				= row.nextVal().getInt();

						startTime = calcStartTime(schdata,sType);
						
						erp = new ERP;

						erp->setSchData(schdata);
						erp->setType(2);
						erp->setSynchronous(false);
						erp->setStartTime(startTime);
						erp->setLastExecutionTime(0);

						if(repeat)
						{
							erp->setRepeatition(-1);
						}
						else 
						{
							erp->setRepeatition(0);
						}

						BaseObject *obj = new BaseSnapshot;
						erp->setObject(obj);

						SnapshotDefinition *tskdef	= new SnapshotDefinition;

						tskdef->taskId = getSnapshotId(i);
						tskdef->scheduleId			= schid;
						tskdef->taskGroupId			= 0;
						tskdef->szSnapshotId		= "";

						int sz = sizeof(*tskdef);

						erp->setInformation(tskdef);
						erp->setSizeInformation(sizeof(tskdef));

						if(repeat)
						{
							erp->setFrequency(schdata.everyDay * 24 * 60 * 60 + schdata.everyHour * 60 * 60 + schdata.everyMinute * 60);
						}
						erp->setCompletionRoutine(snapshotCompletionRoutine);
						boost::mutex::scoped_lock gaurd(g_jobvectorLock);
						m_Cnt++;
					}
					}
			}
		}
	}
}	
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "nextERP::Exception raised=%s",str);
}
catch (const std::out_of_range& oor)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "Out of Range error Exception = %s", oor.what());
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "nextERP::Default Exception raised");
}	
	
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::nextERP");
	return erp;
}

vector<SnapshotDefinition> SnapshotManager::onView()
{
	vector<SnapshotDefinition> vTskdef;
	return vTskdef;
}

SnapshotManager::~SnapshotManager()
{
	SAFE_SQL_CLOSE( m_InmCon );
}

PERP SnapshotManager::createAsyncERP(long snapId, string szSnapshotId)
{
	//task id should be -1 for subsequent fetch
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::createAsyncERP");
	PERP erp					= NULL;
	Connection *con				= NULL;
	SnapshotDefinition *tskdef	= NULL;
	con = createConnection();
	std::stringstream szSQL;
try
{
	if(con != NULL )
	{
		erp = new ERP;

		tskdef	= new SnapshotDefinition;
		if( tskdef != NULL && erp != NULL )
		{
			tskdef->taskId				= snapId;
			tskdef->taskGroupId			= 0;
			tskdef->szSnapshotId		= szSnapshotId;

			if(snapId == 0)
			{
				tskdef->taskId		= snapId;
				tskdef->scheduleId	= 0;
			}
			else
			{
				

				szSQL << "select snapScheduleId from snapshotMain where snapshotId = " << snapId;
				DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

				if(!lInmrset.isEmpty())
				{
					DBRow row	= lInmrset.nextRow();

					long schid	= row.nextVal().getInt();
					
					szSQL.str("");
					szSQL << "select * from snapSchedule where Id = " << schid;
					lInmrset = con->execQuery((szSQL.str().c_str()));

					if(!lInmrset.isEmpty())
					{
						row				= lInmrset.nextRow();
						
						long tmpId		= row.nextVal().getInt();
						long startTime	= convertStringToTime(row.nextVal().getString());

						if (startTime == -1)
						{
							startTime = 0;
						}

						long repeat		= row.nextVal().getInt();

						if(repeat == 1)
						{
							repeat = -1;
						}

						SchData schdata;

						schdata.everyDay		= row.nextVal().getInt();
						schdata.everyHour		= row.nextVal().getInt();
						schdata.everyMinute		= row.nextVal().getInt(); 
						schdata.everyMonth		= row.nextVal().getInt(); 
						schdata.everyYear		= row.nextVal().getInt(); 
						schdata.atYear			= row.nextVal().getInt(); 
						schdata.atMonth			= row.nextVal().getInt(); 
						schdata.atDay			= row.nextVal().getInt(); 
						schdata.atHour			= row.nextVal().getInt(); 
						schdata.atMinute		= row.nextVal().getInt(); 
						int sType				= row.nextVal().getInt();

						startTime				= calcStartTime(schdata,sType);

						erp->setSchData(schdata);
						erp->setStartTime(startTime);
						erp->setLastExecutionTime(0);

						if(repeat)
						{
							erp->setRepeatition(-1);
						}
						else 
						{
							erp->setRepeatition(0);
						}

						tskdef->scheduleId = schid;

						if(repeat)
						{
							erp->setFrequency(schdata.everyDay * 24 * 60 * 60 + schdata.everyHour * 60 * 60 + schdata.everyMinute * 60);
						}
					}
				}
			}
			if( erp!= NULL )
			{
				if(snapId == 0)
				{
					erp->setRepeatition(0);		//Asynchronously scheduled ERP. execute once.
				}
				erp->setType(2);
				erp->setSynchronous(false);

				BaseObject *obj = new BaseSnapshot;

				erp->setObject(obj);
				erp->setInformation(tskdef);
				erp->setSizeInformation(sizeof(tskdef));
				erp->setCompletionRoutine(snapshotCompletionRoutine);
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "createAsyncERP::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "createAsyncERP::Default Exception raised");
}

	SAFE_SQL_CLOSE( con );
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::createAsyncERP");
	return erp;
}

long SnapshotManager::calcStartTime(SchData schdata, int sType)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::calcStartTime");
	long startTime = 0;
try
{
	if(schdata.everyMonth == 0 && schdata.everyYear == 0)
	{
		if(sType == 3)
		{
			//Case -> Periodic start time calculation
			//Calculate current minute. Consistent with file replication.
			long curTime	= time(NULL);
			startTime		=  curTime -  curTime % 60;
		}
		else if(sType == 4)
		{
			//Case -> Daily Schedule
			startTime = getScheduleDaily(schdata, time(NULL));
		}
		else if(sType == 5)
		{
			//Case -> Weekly Schedule
			startTime = getScheduleWeekly(schdata, time(NULL));
		}
		else if(schdata.atYear != 0)
		{
			//Case -> Run at: start Time calculation.
			tm t;
			memset(&t,0,sizeof(t));
			t.tm_hour	= schdata.atHour;
			t.tm_mday	= schdata.atDay;
			t.tm_min	= schdata.atMinute;
			t.tm_mon	= schdata.atMonth - 1;
			t.tm_year	= schdata.atYear - 1900;
			t.tm_isdst  = IsDayLightSavingOn();
			startTime	= mktime(&t);
		}
		else if(schdata.isEmpty())
		{
			//Run now start Time calculation
			startTime	= time(NULL);
		}
		
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "createAsyncERP::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "createAsyncERP::Default Exception raised");
}
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::calcStartTime");
	return startTime;
}

void SnapshotManager::UpdateNewEventJobs()
{        
	std::stringstream szSQL;
    std::stringstream szSQL_NewJobs;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::UpdateNewEventJobs");
    DBResultSet lInmrset, rset_NewJobs, rset_insert;
	Connection *con			= NULL;
    // Get all the event based snapshot instances from snapShots
    // table in wait state.
try
{
	szSQL << "select snapshotId from snapShots where executionState IN (0,1,2,3) and snaptype = 6";

	con = createConnection();

	if(con != NULL )
	{

		lInmrset = con->execQuery((szSQL.str().c_str()));

		if(!lInmrset.isEmpty()) {
			string szScheduled;

			for(unsigned int i = 0; i < lInmrset.getSize(); i++) {
				std::stringstream szBuffer;
				DBRow row  = lInmrset.nextRow();
				int id     = row.nextVal().getInt();

				szBuffer << id;
				szScheduled += szBuffer.str();
				szScheduled += ",";
			}
			if(!szScheduled.empty()) {
				szScheduled.erase(szScheduled.size() - 1, 1);
				// Get all new definition for the event based jobs from snapshotMain table.

				//VSNAPFC - Persistence
				//Getting MountedOn
                szSQL_NewJobs << "select snapshotId, srcHostid, srcDeviceName, destHostid, destDeviceName, mountedOn, pending, volumeType from snapshotMain where pending = 1 and deleted = 0 and typeOfSnapshot = 2 and snapshotId NOT IN "  << szScheduled;

			}
	           
		}
		else {
				// Get all first time definition for the event based jobs from snapshotMain table.

				//VSNAPFC - Persistence
				//Getting mountedOn
				szSQL_NewJobs.str("");
				szSQL_NewJobs << "select snapshotId, srcHostid, srcDeviceName, destHostid, destDeviceName,mountedOn, pending, volumeType from snapshotMain where pending = 1 and deleted = 0 and typeOfSnapshot = 2" ;
		}

		rset_NewJobs = con->execQuery((szSQL_NewJobs.str().c_str()));
		if(!rset_NewJobs.isEmpty()) {
			// If new jobs found insert in the snapShots table
			for(unsigned int i = 0; i < rset_NewJobs.getSize(); i++) {
				DBRow row       = rset_NewJobs.nextRow();

				string szId = getNewsnapshotId(m_InmCon);
				long snapId = row.nextVal().getInt();
				string srcid = row.nextVal().getString();
				string srcname = row.nextVal().getString();
				string destid = row.nextVal().getString();
				string destname = row.nextVal().getString();
				//VSNAPFC - mountedOn
				string mountpoint = row.nextVal().getString();
				string pending = row.nextVal().getString();
				int volType = row.nextVal().getInt();
				long isMnt = 0;
	            
				if(volType ==2){
					isMnt = 1;
				}	
                size_t srclen;
                INM_SAFE_ARITHMETIC(srclen = InmSafeInt<size_t>::Type(srcname.size())*2+1, INMAGE_EX(srcname.size()))
				char* src = new char[srclen];
				inm_sql_escape_string(src,(char*)srcname.c_str(),srcname.size());
	   
                size_t destlen;
                INM_SAFE_ARITHMETIC(destlen = InmSafeInt<size_t>::Type(destname.size())*2+1, INMAGE_EX(destname.size()))
   				char* dest = new char[destlen];
   				inm_sql_escape_string(dest,(char*)destname.c_str(),destname.size());
		

				//VSNAPFC - Persistence
				//mountedOn
                size_t mountlen;
                INM_SAFE_ARITHMETIC(mountlen = InmSafeInt<size_t>::Type(mountpoint.size())*2+1, INMAGE_EX(mountpoint.size()))
   				char* mount = new char[mountlen];
   				inm_sql_escape_string(mount,(char*)mountpoint.c_str(),mountpoint.size());

				//VSNPAFC - Persistence
				//Updating mountedOn into snapShots
			
			
				boost::format formatIter("insert into snapShots (id,srcHostid,srcDeviceName,destHostid, destDeviceName,mountedOn,pending,snapshotId,executionState,snaptype,isMounted) values ('%1%','%2%','%3%','%4%','%5%','%6%',%7%,%8%,%9%,%10%,%11%)");

			 	formatIter % szId.c_str() % srcid.c_str() % src % destid.c_str() % dest % mount % 1 % snapId % 0 % 6 % isMnt; 

				rset_insert = con->execQuery((formatIter.str().c_str()));
				if(rset_insert.getRowsUpdated() == 0)
				{
					Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::UpdateNewEventJobs ERROR::INSERT QUERY FAILED TO UPDATE IN DB");
				}

				SAFE_DELETE_ARRAY( src );
				SAFE_DELETE_ARRAY(dest);
				SAFE_DELETE_ARRAY(mount);
				
			}
		}
        
	}
}
catch(char * str)
{
Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "UpdateNewEventJobs::Exception raised=%s",str);

}
catch(...) 
{  
Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "UpdateNewEventJobs::Default Exception raised");
}	
	SAFE_SQL_CLOSE( con );
	
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::UpdateNewEventJobs");
}

//Execute any event scheduled ERPs.
void SnapshotManager::EventSnapshotWorker()
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotManager::EventSnapshotWorker");
	UpdateNewEventJobs();

	// Get all the jobs from snapshots table of type event based and   
	// executionState = 1
	
	Connection *con = NULL;

	con = createConnection();
try
{
	std::stringstream szSQL;
	if(con != NULL ) 
	{
		szSQL << "select a.id, b.snapshotId from snapShots a, snapshotMain b where a.executionState = 1 and a.snaptype = 6 and a.srcHostid = b.srcHostid";
		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				DBRow row				= lInmrset.nextRow();
				string szSnapshotId		= row.nextVal().getString();
				long snapshotId = row.nextVal().getInt();
				if(! m_RunningEventSnapShots.insert(szSnapshotId).second )
					continue;
					
				ERP *erp				= createAsyncERP(0, szSnapshotId);

				if(erp != NULL)
				{
					ExecutionController *ec = ExecutionController::instance();

					if(ec != NULL)
						ec->execute(erp,this);
					//
					//Delete this erp as the execute function internally make a copy of the 
					//oroginal erp.
					//
					SAFE_DELETE( erp );
				}
			}
		}
		Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::EventSnapshotWorker");
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "EventSnapshotWorker:: Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "EventSnapshotWorker::Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotManager::EventSnapshotWorker");
}

///
/// Replaces (Translates) all instances of inputChar in the string pszInString with outputChar. This is
/// an inplace replacement; no new memory is allocated. Equivalent to perl's tr// operator.
///
void SnapshotManager::ReplaceChar(char *pszInString, char inputChar,
		char outputChar)
{

try
{
	if (NULL != pszInString)
	{
	    for (int i=0; '\0' != pszInString[i]; i++) 
		{
	      		  if (pszInString[i] == inputChar) 
				{
	            			pszInString[i] = outputChar;
	        		}
	    	}
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "ReplaceChar:: Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "ReplaceChar::Default Exception raised");
}

}

void SnapshotUtlClass::SchedulePendingJobs(const std::string dHostid, const std::string dDeviceName, Connection * con, SnapshotManager *repmgr)
{
	std::stringstream szSQL;
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "SnapshotUtlClass::SchedulePendingJobs");
	
    size_t outlen;
    INM_SAFE_ARITHMETIC(outlen = InmSafeInt<size_t>::Type(dDeviceName.size()) * 2 + 1, INMAGE_EX(dDeviceName.size()))
	char* output = new char[outlen];
	inm_sql_escape_string(output, dDeviceName.c_str(), dDeviceName.size());
	//dDeviceName = std::string(output);
	//delete[] output;


try
{
	// check if there are any running jobs
	
	szSQL << "select destDeviceName CurrentRun from snapShots where destDeviceName = '" << output << "' and destHostid = '" << dHostid << "' and executionState != 0 and executionState != 4 and executionState != 5";
	DBResultSet rset_running = con->execQuery((szSQL.str().c_str()));

	// we can move pending jobs to ready provided 
	// there are no jobs running on this device
	if(!rset_running.isEmpty())
	{
		Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotUtlClass::SchedulePendingJobs");
		SAFE_DELETE_ARRAY( output );
		
		return;
	}

	// get the list of pending jobs
	szSQL.str("");
	szSQL << "select a.id, a.snapshotId , b.snapshotId from snapShots a, snapshotMain b where a.destDeviceName= '" << output<< "' and a.destHostid = '"<< dHostid << "' and a.executionState = 0 and a.snapshotId = b.snapshotId and b.deleted = 0";
	DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

	if (!lInmrset.isEmpty())
	{
		//Execute the next ERP which is blocked  since the snapshotDrive was in use 
		DBRow row	= lInmrset.nextRow();
		string mid	= row.nextVal().getString();
		long snapId = row.nextVal().getInt();
		szSQL.str("");
		szSQL << "update snapShots set executionState = 1 where id = " << mid;
		con->execQuery((szSQL.str().c_str()));

		// call execute function to run job async.
		ERP* nerp				= repmgr->createAsyncERP(snapId, mid);

		if(nerp != NULL)
		{
			ExecutionController *ec = ExecutionController::instance();

			if(ec != NULL)
				ec->execute(nerp, repmgr);
			//
			//Delete this erp as the execute function internally make a copy of the 
			//oroginal erp.
			//
			SAFE_DELETE( nerp );
		}
	}
	else
	{
		//
		//Look for any recovery jobs
		//
		szSQL.str("");
		szSQL <<"select id, snapshotId from snapShots where destDeviceName = '" << output << "' and destHostid = '"<< dHostid <<"' and executionState = 0 and snapshotId = 0";
		lInmrset = con->execQuery((szSQL.str().c_str()));

		if (!lInmrset.isEmpty())
		{
			//Execute the next ERP which is blocked because of this ERP since the snapshotDrive being used by the current ERP
			DBRow row	= lInmrset.nextRow();
			string mid	= row.nextVal().getString();
			long snapId = row.nextVal().getInt();
			szSQL.str("");
			szSQL << "update snapShots set executionState = 1 where id = " << mid;
			con->execQuery((szSQL.str().c_str()));

			// call execute function to run job async.
			ERP* nerp				= repmgr->createAsyncERP(snapId, mid);
			if(nerp != NULL)
			{
				ExecutionController *ec = ExecutionController::instance();

				if(ec != NULL)
					ec->execute(nerp, repmgr);
				//
				//Delete this erp as the execute function internally make a copy of the 
				//oroginal erp.
				//
				SAFE_DELETE( nerp );
			}
		}
	}
}	
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "SchedulePendingJobs::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "SchedulePendingJobs::Default Exception raised");
}	
	SAFE_DELETE_ARRAY(output);
	
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Exiting SnapshotUtlClass::SchedulePendingJobs");
}


void SnapshotManager::waitForPendingJobs()
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "Waiting for Pending SnapshotJobs to Move to consistent state...");
	std::stringstream szSQL;
	int i = 0;
	szSQL << "select b.snapshotId from snapSchedule a, snapshotMain b where a.repeat = 0 and b.pending = 1 and a.Id = b.snapshotId";
try
{
	while(true)
	{
		DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
		if(lInmrset.isEmpty())
		{
			break;
		}
		sch::Sleep(5000);
		i++;
		if( i == 10 )
		{
			Logger::Log("SnapshotManager.cpp", __LINE__, 0, 2, "There are still some pending snapshots.");
			break;
		}
	}
}
catch(char * str)
{
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "waitForPendingJobs::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("SnapshotManager.cpp", __LINE__, 0, 1, "waitForPendingJobs::Default Exception raised");
}

}

int SnapshotManager::getSnapshotId(int index)
{
	boost::mutex::scoped_lock guard(g_jobvectorLock);
	return m_ScheduledJobs.at(index);
}