#pragma warning(disable: 4786)
#include "ExecutionController.h"
#include "FileRepManager.h"
#include "util.h"
#include "ExecutionObject.h"
#include "svconfig.h"
#include "Log.h"
#include <set>
#include <sstream>
#include "inmsafecapis.h"


using namespace std;


#define START_TIME_OUT 180*60					//180 min timeout for job to be in running state
#define QUERY_SIZE 1024

extern int gMaxCount;
int procCount = 0;
static Mutex sch_mtx;


void executeBatch(vector<PERP> batch,FileRepManager *repmgr, Connection* con)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "executeBatch");
	ExecutionController *ec		= NULL;
	
	
	TaskDefinition *tskdef		= NULL;
try
{
	if( con != NULL  )
	{
		ec = ExecutionController::instance();
		for(unsigned int i = 0; i < batch.size(); i++)
		{
			PERP curERP = batch[i];

			//
			//Cache the mysqlIds for the ERP's as execute ERP creates a copy of the original ERP.
			//Hence, mysqlId which get stored in the execute method of the ExecutionObject will not
			//get reflected into this original ERP. It is only accessible in the corresponding completionRoutine.
			//
			if( curERP != NULL )
				tskdef = dynamic_cast<TaskDefinition*>(curERP->getInformation());	
			else
				Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "Invalid Execution request");

			if(tskdef != NULL)
			{
				std::stringstream szSQL;
				szSQL << "select id from frbJobs where jobConfigId = " << tskdef->taskId ;
				DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

				if(!lInmrset.isEmpty())
				{
					DBRow row = lInmrset.nextRow();

					if(row.getSize() == 1)
					{
						tskdef->mysqlId = row.nextVal().getInt();
					}
				}
			}

			if(ec != NULL)
				ec->execute(curERP, repmgr);
		}
	}
	else
		Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "invalid connection object");
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "executeBatch ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "executeBatch  Default Exception raised");
}

	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting executeBatch");
}

long WaitForCompletionMultiple(vector<long> vIdList,Connection *con,bool bWaitForAll)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "WaitForCompletionMultiple");
	std::stringstream szSQL;
	string szIdlist = "";

	DBResultSet lInmrset;
	set<long> setFinished;	//Contains job ids of jobs which finished execution
	vector<long> vFinished;
try
{

	if(vIdList.size() != 0 && con != NULL  )
	{
		for(unsigned int i = 0; i < vIdList.size(); i++)
		{
			std::stringstream szBuffer;
            szBuffer << vIdList[i];
			szIdlist += szBuffer.str();
			szIdlist += ",";
		}

		if(!szIdlist.empty())
		{
			szIdlist.erase(szIdlist.size()-1, 1);		//Erase last comma
		}

		
		szSQL <<  "select jobConfigId, assignedState, currentState, daemonState from frbJobs,frbJobConfig where frbJobs.jobConfigId in ( " << szIdlist << " )and frbJobConfig.id = frbJobs.jobConfigId and frbJobConfig.deleted=0";

		if(bWaitForAll)
		{
			while(1)
			{
				//Poll DB to check the currentState value every 10 milliseconds
				lInmrset = con->execQuery(szSQL.str().c_str());
				if(!lInmrset.isEmpty())
				{
					for(unsigned int i = 0; i < lInmrset.getSize(); i++)
					{
						DBRow row			= lInmrset.nextRow();
						long id				= row.nextVal().getInt();
						long assignedState	= row.nextVal().getInt();
						long currState		= row.nextVal().getInt();
						long daemonState	= row.nextVal().getInt();

						if(assignedState == 0)
						{
							setFinished.insert(id);
						}
					}
					if(setFinished.size() == vIdList.size())
					{
						break;
					}
					sch::Sleep(60000);
				}
				else
					break;				
			}
		}
		else
		{
			while(1)
			{
				//Poll DB to check the currentState value every 10 milliseconds
				lInmrset = con->execQuery((szSQL.str().c_str()));

				if(!lInmrset.isEmpty())
				{
					for(unsigned int i =  0; i < lInmrset.getSize(); i++)
					{
						DBRow row			= lInmrset.nextRow();
						long id				= row.nextVal().getInt();
						long assignedState	= row.nextVal().getInt();
						long currState		= row.nextVal().getInt();
						long daemonState	= row.nextVal().getInt();

						if(assignedState == 0)
						{
							return id;
						}
					}
					sch::Sleep(60000);
				}
				else
					break;				
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "WaitForCompletionMultiple ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "WaitForCompletionMultiple  Default Exception raised");
}
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "Exiting WaitForCompletionMultiple");
	return -1;
}

SCH_STATUS waitForRunning(PERP perp, string szJobId, long timeout)
{
	//If job is not taken by Target in some specified time out period
	//then delete the job.

	Connection *con			= NULL;
	FileRepManager *repmgr	= NULL;
	TaskDefinition *tskdef			= NULL;
	std::stringstream szSQL;
	DBResultSet lInmrset;
	
try
{
	repmgr = dynamic_cast<FileRepManager *>(perp->getRepositoryManager());
	

	if(repmgr == NULL)
	{
		Logger::Log("FileRepManager.cpp", __LINE__, 0, 0,"Repository Manager not available\n");
		return SCH_INVALID_PARAM;
	}
	if(szJobId.size() != 0)
	{
		time_t startTime	= time(NULL);
		time_t curTime	= time(NULL);
		tskdef = static_cast<TaskDefinition *>(perp->getInformation());
		szSQL << "select id, assignedState, currentState, daemonState from frbJobs where jobConfigId in ( " << szJobId << " )" ;
		
	
				
		con = createConnection();
		if(con != NULL )
		{
				lInmrset = con->execQuery((szSQL.str().c_str()));
		
				if(!lInmrset.isEmpty())
				{
		 			DBRow row			= lInmrset.nextRow();
					long id				= row.nextVal().getInt();
					long assignedState	= row.nextVal().getInt();
					long currentState	= row.nextVal().getInt();
					long daemonState	= row.nextVal().getInt();

	

					if(assignedState == 0 && (tskdef->mysqlId != 0 && tskdef->mysqlId < id))
					{
	
						SAFE_SQL_CLOSE( con );
						return SCH_SUCCESS;
					}
				}
		}
				 
	
		SAFE_SQL_CLOSE( con );
		 
		while(1)
		{

		Connection *con			= NULL;
		con = createConnection();
		if(con != NULL )
		{
			lInmrset = con->execQuery((szSQL.str().c_str()));
		}
		if(!lInmrset.isEmpty())
		{
			
	
			DBRow row = lInmrset.nextRow();
			long id				=	row.nextVal().getInt();
			long assignedState	=	row.nextVal().getInt();
			long currentState	=	row.nextVal().getInt();
			long daemonState	=	row.nextVal().getInt();



			if((assignedState == 1 && currentState != 0  && daemonState != 0) || (assignedState == 0 && (tskdef->mysqlId != 0 && tskdef->mysqlId < id)))
			{
				

				SAFE_SQL_CLOSE( con );
				return SCH_SUCCESS;
			}
			

			SAFE_SQL_CLOSE( con );
			sch::Sleep(60000);
		}
		else
		{
			
			//SAFE_SQL_CLOSE( con );
			break;
		}
		if(curTime - startTime >= timeout)
		{
			break;
		}			
		curTime = time(NULL);
		
		
		SAFE_SQL_CLOSE( con );
		}
		 
	}
	else 
	{
		return SCH_INVALID_PARAM;
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "waitForRunning ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "waitForRunning  Default Exception raised");
}	
	SAFE_SQL_CLOSE( con );
	return SCH_TIMEOUT;
	
}

SCH_STATUS WaitForCompletionSingle(string szJobId, Connection *con)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "WaitForCompletionSingle");
	std::stringstream szSQL;

	//
	//Wait for completion of a single job.
	//
try
{
	if( szJobId.size() != 0 && con != NULL )
	{
		DBResultSet lInmrset;
		szSQL << "select jobConfigId, assignedState from frbJobs,frbJobConfig where frbJobs.jobConfigId in ( " << szJobId << " ) and frbJobConfig.id = frbJobs.jobConfigId and frbJobConfig.deleted=0";
		

		//
		//Wait for completion
		//
		while(1)
		{
			lInmrset = con->execQuery((szSQL.str().c_str()));
			//Poll DB to check the currentState value every 10 milliseconds
			if(!lInmrset.isEmpty())
			{
				DBRow row			=	lInmrset.nextRow();
				long id				=	row.nextVal().getInt();
				long assignedState	=	row.nextVal().getInt();
				
				if(assignedState == 0)
				{
					break;
				}
				sch::Sleep(60000);
			}
			else
				break;
		}
	}
	else 
	{
		Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "WaitForCompletionSingle Invalid Connection Object");
		return SCH_INVALID_PARAM;
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "WaitForCompletionSingle ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "WaitForCompletionSingle  Default Exception raised");
}	
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "Exiting WaitForCompletionSingle");
	return SCH_SUCCESS;
}

void completionRoutineWorker(PERP perp,bool success)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "completionRoutineWorker");
	Connection *con				= NULL;
	FileRepManager *repmgr		= NULL;
	TaskDefinition *tskdef		= NULL;
try
{
	if(perp != NULL && perp->getExecutionStatus() != SCH_DELETED)
	{
		con = createConnection();
		repmgr = dynamic_cast<FileRepManager *>(perp->getRepositoryManager());
		if( repmgr != NULL ) {
			

			if( con != NULL )
			{
				tskdef = static_cast<TaskDefinition *>(perp->getInformation());

				if(tskdef != NULL)
				{
					std::stringstream szBuffer;
                    szBuffer << tskdef->taskId;
									

					if(waitForRunning(perp, szBuffer.str(),START_TIME_OUT) == SCH_TIMEOUT)		//Job failed to start
					{
				//	    char szSQL[128];

				//	    con->execQuery(szSQL);
					}

					if(perp->getExecutionStatus() != SCH_PENDING)
						//Wait for this erp to complete.
						WaitForCompletionSingle(szBuffer.str(),con);
				}

				updateJobStatus(perp);
			}
			else
				Logger::Log("FileRepManager.cpp", __LINE__, 2, 1, "Invalid connection Object");
		}
		else
			Logger::Log("FileRepManager.cpp", __LINE__, 2, 1, "Failed to get repository manager");
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "completionRoutineWorker ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "completionRoutineWorker  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "Exiting completionRoutineWorker");
}

void completionRoutineMaster(PERP perp,bool success)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "completionRoutineMaster");
	Connection *con			= NULL;
	FileRepManager *repmgr	= NULL;
try
{
	if(perp != NULL && perp->getExecutionStatus() != SCH_DELETED)
	{
		con = createConnection();
		repmgr = dynamic_cast<FileRepManager *>(perp->getRepositoryManager());

		if(repmgr == NULL)
		{
			Logger::Log("FileRepManager.cpp", __LINE__, 0, 0,"Repository Manager not available\n");
		}
		else
		{
			
			if(con != NULL )
			{
				//Task id should be -1 for subsequent fetch
				//Execute other jobs of the same order in this group
				TaskDefinition *tskdef = NULL;
				tskdef = static_cast<TaskDefinition *>(perp->getInformation());
				if(tskdef == NULL)
				{
					Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Could not execute master ERP");
				}
				else 
				{
					vector<PERP> batch = repmgr->nextBatch(tskdef,perp);

					if(!batch.empty())
					{
						//Execute other erps of the same order as of this erp in this group.
						executeBatch(batch, repmgr, con);
						sch::Sleep(100);		//Give chance for thread to execute first
					}

					std::stringstream szBuffer;					
					szBuffer << tskdef->taskId;

					if(waitForRunning(perp, szBuffer.str(), START_TIME_OUT) == SCH_TIMEOUT)		//Job failed to start
					{
				//		char szSQL[128];

				//		con->execQuery(szSQL);
					}

					if(perp->getExecutionStatus() == SCH_PENDING)
					{
						repmgr->freeBatch(batch);
						batch.clear();
					}
					else 
					{
						vector<long> vIdList;
						vIdList.push_back(tskdef->taskId);

						unsigned int i = 0;

						for(i = 0; i < batch.size(); i++)
						{
							PERP curERP = batch[i];
							TaskDefinition *tskdefTmp = NULL;
							tskdefTmp = static_cast<TaskDefinition *>(curERP->getInformation());

							if(tskdefTmp != NULL && tskdef->taskId  != tskdefTmp->taskId )
								vIdList.push_back(tskdefTmp->taskId);
						}

						//Wait for other scheduled erps of the same order from the same
						//group to complete.
						
						vector<long> vIdsToWait;
						vIdsToWait.assign(vIdList.begin(), vIdList.end());

						while(1)
						{
							long id = WaitForCompletionMultiple(vIdsToWait, con, false);

							//If the ERP completed is current ERP, do status updates.
							if(id == tskdef->taskId)	
							{
								updateJobStatus(perp);				
							}
							else if( id == -1 )
								break;
							vector<long> vIdsTmp;
							vIdsTmp.assign(vIdsToWait.begin(), vIdsToWait.end());;

							vIdsToWait.clear();

							for(i = 0; i < vIdsTmp.size(); i++)
							{
								if(id != vIdsTmp[i])
								{
									vIdsToWait.push_back(vIdsTmp[i]);
								}
							}
							
							if(vIdsToWait.size() == 0)
							{
								break;
							}
						}

						tskdef = static_cast<TaskDefinition *>(perp->getInformation());
						int lastExecutedOrder = tskdef->order;

						//
						//Push the current ERP on to the list of other ERPs of the same order
						//
						batch.push_back(perp);

						//Check if the ERP's are completed with exitCode as success.
						if(!repmgr->isCompletedWithSuccess(batch))
						{
							//
							//Remove the current ERP which was pushed on to the back
							//
							batch.pop_back();
							//
							//Failure case: Return wihtout executing the next order jobs.
							//
							repmgr->freeBatch(batch);
							batch.clear();

						}
						else 
						{
							//
							//Remove the current ERP which was pushed on to the back
							//
							batch.pop_back();

							//
							//Delete all other ERPs as the scheduler copies the erps internally.
							//
							repmgr->freeBatch(batch);
							batch.clear();

							while(1)
							{
								//Execute jobs of the higher order in this group.
								TaskDefinition *tskdef2 = dynamic_cast<TaskDefinition* >(tskdef->clone());
								tskdef2->order = ++lastExecutedOrder;

								vector<ERP*> batch = repmgr->nextBatch(tskdef2, perp);
								SAFE_DELETE( tskdef2 );
								vIdList.clear();		//Clear the previous jobs

								if(!batch.empty())
								{
									executeBatch(batch, repmgr, con);

									string szBatchIdlist	= "";
									unsigned int i = 0;

									for(i = 0; i < batch.size(); i++)
									{
										ERP *curERP = batch[i];
										TaskDefinition *tskdef = static_cast<TaskDefinition *>(curERP->getInformation());

										if(tskdef != NULL)
											vIdList.push_back(tskdef->taskId);
									}

									for(i = 0; i < batch.size(); i++)
									{
										ERP *curERP = batch[i];
										TaskDefinition *tskdef = static_cast<TaskDefinition *>(curERP->getInformation());
										
										if(tskdef == NULL)
											continue;

										std::stringstream szBuffer;
										szBuffer << tskdef->taskId;

										if(waitForRunning(curERP, szBuffer.str(), START_TIME_OUT) == SCH_TIMEOUT)
										{
											std::stringstream szSQL;
											szSQL << "update frbJobs set assignedState = 0, currentState = 0, daemonState = 0 where jobConfigId = " << tskdef->taskId;
											con->execQuery((szSQL.str().c_str()));
										}
									}

									WaitForCompletionMultiple(vIdList, con, true);

									if(!repmgr->isCompletedWithSuccess(batch))
									{
										//
										//Failure case: Return wihtout executing the next order jobs.
										//
										repmgr->freeBatch(batch);
										batch.clear();
									}
									else 
									{
										//
										//Delete all other ERPs as the scheduler copies the erps internally.
										//
										repmgr->freeBatch(batch);
										batch.clear();
									}
								}
								else
								{
									break;
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
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "completionRoutineMaster ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "completionRoutineMaster  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 1, "Exiting completionRoutineMaster");
}

FileRepManager::FileRepManager()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::FileRepManager");
		m_InmCon = NULL;
		m_InmCon = new CXConnectionEx();
		m_Cnt = 0;

		if(m_InmCon != NULL )
		{
			updatePrevIncompleteJobs();
		}
		else
		{
			Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Connection object is NULL");
		}
	
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::FileRepManager");
}

void FileRepManager::updatePrevIncompleteJobs()
{
	std::string szSQL;
	int lCount = 0;
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::updatePrevIncompleteJobs");
try
{
	if( m_InmCon != NULL ) 
	{
		szSQL.clear();
		szSQL = "update frbJobs set currentState=2,daemonState=2 where assignedState=2";
		
		m_InmCon->execQuery(szSQL.c_str());
		
		szSQL.clear();
		szSQL = "delete from frbJobs where assignedState=2";
		
		m_InmCon->execQuery(szSQL.c_str());

		szSQL.clear();
		szSQL = "select id, jobConfigId from frbJobs where assignedState=1";
		DBResultSet lInmrset = m_InmCon->execQuery(szSQL.c_str());

		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				DBRow row = lInmrset.nextRow();
				if(row.getSize() == 2)
				{
					long mysqlId		= row.nextVal().getInt();
					long jobConfigId	= row.nextVal().getInt();

					PERP perp		= createNewERP(jobConfigId, m_InmCon);
					
					if(perp == NULL) {
						Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "Invalid execution request");
						continue;
					}

					BaseObject *obj = new FileRepInit;

					perp->setObject(obj);
					perp->setCompletionRoutine(NULL);

					TaskDefinition *tskdef = dynamic_cast<TaskDefinition*>(perp->getInformation());

					if(tskdef != NULL)
						tskdef->erpType			= SCH_WORKER;
					else
						Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "Invalid task definition");
					ExecutionController *ec = ExecutionController::instance();

					if(ec != NULL)
						ec->execute(perp,this);
					else
						Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "Invalid execution controller instance");
					//
					//Delete this erp as the execute function internally make a copy of the 
					//oroginal erp.
					//
					SAFE_DELETE( perp );
				}
			}
		}

		szSQL.clear();
		szSQL = "select jobConfigId FROM frbJobs";
		lInmrset = m_InmCon->execQuery(szSQL.c_str());

		string szJobList = "";

		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				std::stringstream szBuffer;
				szBuffer << lInmrset.nextRow().nextVal().getInt();
				
				

				szJobList += szBuffer.str();
				szJobList += ",";
			}
			if(szJobList.size() > 0)
				szJobList.erase(szJobList.size() - 1);
		}
        std::stringstream szSQLStr;
        if(szJobList.size() > 0)
			szSQLStr << "select id from frbJobConfig where id not in ( " << szJobList << " ) and deleted = 0 ";
			
		else
			szSQLStr <<  "select id from frbJobConfig where deleted = 0";

		lInmrset = m_InmCon->execQuery((szSQLStr.str().c_str()));
		
		
		

		for(unsigned int i = 0; i < lInmrset.getSize(); i++)
		{
			//Syncronization for counter
			sch_mtx.lock();
			lCount = genCount();
			sch_mtx.unlock();

			int jobConfigId = lInmrset.nextRow().nextVal().getInt();
			std::stringstream SPOut;
			SPOut << "CREATE PROCEDURE `"<<"inm_sch_dyn_procedure_"<<lCount<<"`"<<" ()"<< std::endl
				  << "BEGIN" << std::endl
				  << "  DECLARE frbJobId BIGINT DEFAULT NULL;" << std::endl
				  << "  DECLARE jobConfId BIGINT DEFAULT " << jobConfigId << ";" << std::endl
				  << "  DECLARE error TINYINT DEFAULT 0;" << std::endl
				  << "  DECLARE scheduleStartTime DATETIME;" << std::endl
				  << "  DECLARE frbJobOrder INT;" << std::endl
				  << "  DECLARE CONTINUE HANDLER FOR SQLSTATE '23000' SET error = 1;" << std::endl
				  << "  DECLARE CONTINUE HANDLER FOR SQLSTATE '02000' SET error = 1;" << std::endl
				  << "  START TRANSACTION;" << std::endl
				  << "  INSERT INTO frbJobs (jobConfigId) VALUES (jobConfId);" << std::endl
				  << "  SET frbJobId = LAST_INSERT_ID();" << std::endl
				  << "  INSERT INTO frbStatus (id,jobConfigId) VALUES (frbJobId, jobConfId);" << std::endl
				  << "  IF error = 0 THEN" << std::endl
				  << "    SELECT" << std::endl
				  << "      sc.startTime,c.jobOrder INTO scheduleStartTime, frbJobOrder" << std::endl
				  << "    FROM" << std::endl
				  << "      frbSchedule sc, frbJobConfig c, frbStatus s" << std::endl
				  << "    WHERE" << std::endl
				  << "      sc.id = c.scheduleId AND" << std::endl
				  << "      c.id = s.jobConfigId AND" << std::endl
				  << "      s.jobConfigId = jobConfId AND c.deleted = 0 LIMIT 1;" << std::endl
				  << "  END IF;" << std::endl
				  << "  IF error = 0 THEN" << std::endl
				  << "    UPDATE frbStatus SET startTime = scheduleStartTime WHERE id = frbJobId;" << std::endl
				  << "  END IF;" << std::endl
				  << "  IF error THEN" << std::endl
				  << "    ROLLBACK;" << std::endl
				  << "  ELSE" << std::endl
				  << "    COMMIT;" << std::endl
				  << "  END IF;" << std::endl
				  << "  SELECT error;" << std::endl
				  << "END" << std::endl;
			m_InmCon->execQuery((SPOut.str().c_str()),true);
		}
        
	}
	else 
	{
		Logger::Log("FileRepManager.cpp", __LINE__, 2, 0, "Invalid sql connection Object");
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updatePrevIncompleteJobs ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updatePrevIncompleteJobs  Default Exception raised");
}	
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::updatePrevIncompleteJobs");
}

//Callbacks that are called by the scheduler on start of execution and on end of execution resp.
void FileRepManager::onSchedule(Status *status, void *context)		//context points to the copy of the repository manager defined structure
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onSchedule");
	Connection* con			= NULL;
	TaskDefinition *tskdef	= NULL;
	con = createConnection();
try
{
	if(con != NULL )
	{
		tskdef = static_cast<TaskDefinition*>(context);

		if(tskdef != NULL && tskdef->erpType == SCH_MASTER)
		{
			std::stringstream szSQL;

			if(status->repeat == 0)
			{
				//Execute once jobs.
				szSQL <<  "update frbSchedule set startTime = '0000-00-00 00:00:00' where id = " << tskdef->scheduleId << " and startTime!='0000-00-00 00:00:00'";
				con->execQuery((szSQL.str().c_str()));
			}
			else
			{
				//Periodically repeated jobs.
				string szNewTime = convertTimeToString(status->nextExecutionTime);
				//Periodically repeated jobs.
				szSQL << " update frbSchedule set startTime = '" << szNewTime.c_str() << "' where id = " << tskdef->scheduleId << " and startTime!='0000-00-00 00:00:00'" ;
				
				con->execQuery((szSQL.str().c_str()));

				Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Schedule update: Job ID:%ld Schedule ID:%d Next Start Time:%s", tskdef->taskId, tskdef->scheduleId, szNewTime.c_str());
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "onSchedule ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "onSchedule  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::onSchedule");
	return;
}

void FileRepManager::onFinished(Status *status,void *context)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onFinished empty function body");
}

void updateJobStatus(PERP perp)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateJobStatus");
	std::stringstream szSQL;
	Connection* con			= NULL;
	FileRepManager *repmgr	= NULL;
	TaskDefinition *tskdef	= NULL;
	con						= createConnection();
try
{
	
	if( perp != NULL )
		repmgr					= dynamic_cast<FileRepManager *>(perp->getRepositoryManager());

	

	if(con != NULL )
	{
		tskdef = static_cast<TaskDefinition *>(perp->getInformation());
		if(tskdef != NULL)
		{
			//Create log entry for the newly started job.
			szSQL << "select a.id, b.jobOrder from frbJobs a, frbJobConfig b where a.jobConfigId = " << tskdef->taskId << " and a.jobConfigId = b.id" ;			
			DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

			if(!lInmrset.isEmpty())
			{
				DBRow row = lInmrset.nextRow();
				if(row.getSize() == 1)
				{
					long mysql_id = row.nextVal().getInt();
					int runorder = row.nextVal().getInt();
					Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateJobStatus insert frbStatus %s", convertTimeToString(time(NULL)).c_str());

					if(perp->getRepeatition() == -1 && runorder == 0)
					{
						szSQL.str("");
						szSQL << "update frbStatus set startTime = '" << convertTimeToString(perp->getNextScheduledTime()).c_str() << "' where id = " << mysql_id << " and endTime = '0000-00-00 00:00:00'";						
						con->execQuery((szSQL.str().c_str()));
					}
				}
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateJobStatus ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateJobStatus  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting updateJobStatus");
}

void FileRepManager::onStart(void *context)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onStart.. it has empty body");
	//This function is called when a new job is scheduled aynchronously to the scheduler.
	return;
}
void FileRepManager::LoadScheduledSnapshotVetor(IRepositoryManager* repmgr)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::LoadScheduledSnapshotVetor.. it has empty body");
	//This function is called when a new job is scheduled aynchronously to the scheduler.
	return;
}

//This function is called when a job is removed from the scheduler.
void FileRepManager::onRemove(void *context,Status *status)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onRemove");
	Connection *con			= NULL;
	TaskDefinition *tskdef	= NULL;
	con						= createConnection();
try
{

	if(con != NULL )
	{

		std::stringstream szSQL;
		
		tskdef = static_cast<TaskDefinition *>(context);

		if(tskdef != NULL)
		{
			szSQL << "select id FROM frbJobs WHERE jobConfigId = " << tskdef->taskId ;
			DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

			if(!lInmrset.isEmpty())
			{
				DBRow row = lInmrset.nextRow();

				if(row.getSize() == 1)
				{
					long mysqlId = row.nextVal().getInt();

					szSQL.str("");
					szSQL << "delete from frbStatus where id = " << mysqlId;
					con->execQuery((szSQL.str().c_str()));
				}
			}
			szSQL.str("");
			szSQL << "DELETE FROM frbJobs WHERE jobConfigId = " << tskdef->taskId;			
			con->execQuery((szSQL.str().c_str()));
			szSQL.str("");
			szSQL << "DELETE FROM frbSchedule WHERE id = " << tskdef->scheduleId;			
			con->execQuery((szSQL.str().c_str()));
			szSQL.str("");
			szSQL << "DELETE FROM frbOptions WHERE id = " << tskdef->taskId;			
			con->execQuery((szSQL.str().c_str()));

			Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Remove Job ID:%ld", tskdef->taskId);
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "onRemove ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "onRemove  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	return;
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::onRemove");
}

//Status is a structure allocated containing the status
void FileRepManager::onStop(Status *status,void *context)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onStop.. empty body");
}

bool FileRepManager::isCompletedWithSuccess(vector<PERP> vBatch)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::isCompletedWithSuccess");
	Connection *con			= NULL;
	TaskDefinition *tskdef	= NULL;

	con						= createConnection();
	bool lbRet				= true;
try
{

	if(con != NULL )
	{
		string szIdList = "";

		unsigned int i = 0;
		for(i = 0; i < vBatch.size(); i++)
		{
			PERP perp = vBatch[i];

			tskdef = dynamic_cast< TaskDefinition* >(perp->getInformation());
			
			if(tskdef == NULL) 
			{
				Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "Invalid task definition");
				continue;
			}

			std::stringstream szBuffer;
            szBuffer << tskdef->mysqlId;
			szIdList += szBuffer.str();
			szIdList += ",";
		}


		if( szIdList.size() > 0 )
		{
			szIdList.erase(szIdList.size() - 1, 1);
		}

		std::stringstream szSQL;

		szSQL <<  "select exitCode from frbStatus where id in ( " << szIdList <<" )";
		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

		for(i = 0; i < lInmrset.getSize(); i++)
		{
			DBRow row = lInmrset.nextRow();
			if(row.getSize() == 1)
			{
				int exitCode = row.nextVal().getInt();

				if(exitCode != 0)
				{
					lbRet = false;
				}
			}
		}
	}
	else 
	{
		Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "invalid sql connection object");
		lbRet = false;
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "waitForRunning ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "waitForRunning  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::isCompletedWithSuccess");
	return lbRet;
}

void FileRepManager::getNewJobs()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::getNewJobs");

	std::stringstream szSQL;
	vector<string> scheduledJobs;
try
{
    if(m_InmCon != NULL ) 
	{
		if(m_ScheduledJobs.size() > 0)
		{
			map<long, vector<long> >::iterator iter = m_ScheduledJobs.begin();
			while(iter != m_ScheduledJobs.end())
			{
				string szTmp    = "";
				unsigned int i  = 0;

				for(i = 0; i < iter->second.size() - 1; i++)
				{
					
					std::stringstream szBuffer;
					szBuffer << iter->second[i];
					szTmp += szBuffer.str();
					
					szTmp += ",";
				}

				std::stringstream szBuffer;
				szBuffer << iter->second[i];
				szTmp += szBuffer.str();

				scheduledJobs.push_back(szTmp);

				iter++;
			}
			
			string szScheduled = "";

			for(unsigned int i = 0; i < scheduledJobs.size() - 1; i++)
			{
				szScheduled += scheduledJobs[i] + ",";
			}
			if(scheduledJobs.size()>0)
			{
				szScheduled += scheduledJobs[scheduledJobs.size() - 1];
			}

			
			szSQL.str("");
			szSQL << "select id, groupId, jobOrder from frbJobConfig where id not in ( " << szScheduled << " ) and deleted = 0 order by groupId, jobOrder";
			
		}
		else
		{
			szSQL.str("");
			szSQL << "select id, groupId, jobOrder from frbJobConfig where deleted = 0 order by groupId, jobOrder";
		}
		
		

			DBResultSet lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				DBRow row	= lInmrset.nextRow();
				long id		= row.nextVal().getInt();
				long gid	= row.nextVal().getInt();
				long order	= row.nextVal().getInt();

				map<long, vector<long> >::iterator iter = m_ScheduledJobs.find(gid);

				if(iter != m_ScheduledJobs.end())
				{
					vector<long> & tmp = iter->second;
					tmp.push_back(id);
				}
				else
				{
					vector<long> newGroup;
					newGroup.push_back(id);
					m_ScheduledJobs.insert(make_pair(gid, newGroup));
				}
			}
		
	}
	
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "getNewJobs ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "getNewJobs  Default Exception raised");
}
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::getNewJobs");
}

//Fetch all records from database which are to be scheduled;

void FileRepManager::updateCompletedJobs()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::updateCompletedJobs");
	DBResultSet lInmrset ;
	int lCount= 0;
	
try
{
	if(m_InmCon != NULL )
	{
		std::stringstream szSQL;

		szSQL << "select autoClearLogYear, autoClearLogMonth, autoClearLogWeek, autoClearLogDay from frbGlobalSettings";
		lInmrset = m_InmCon->execQuery((szSQL.str().c_str()));

		if(!lInmrset.isEmpty())
		{
			DBRow row	= lInmrset.nextRow();
			long year	= row.nextVal().getInt();
			long month	= row.nextVal().getInt();
			long week	= row.nextVal().getInt();
			long day 	= row.nextVal().getInt();

			long threshold = year * 365 + 30 * month + 7 * week + day;
			
			if(threshold > 0)
			{
			// Start Code Change for- Auto deleting the fx logs
			szSQL.str("");	
			szSQL << "select log, daemonLogPath  from frbStatus where endTime < DATE_SUB(NOW(), INTERVAL " << threshold << " DAY ) and endTime <> 0";
			DBResultSet rsset = m_InmCon->execQuery((szSQL.str().c_str()));

				for(unsigned int i = 0; i < rsset.getSize(); i++)
				{
				char str[1000] = {0};
				DBRow row	= rsset.nextRow();
				 char jobLog [100];
				 char jobdaemonLog[100];
				 inm_strcpy_s(jobLog,ARRAYSIZE(jobLog), row.nextVal().getString().c_str());
				
	
				 inm_strcpy_s(jobdaemonLog, ARRAYSIZE(jobdaemonLog),row.nextVal().getString().c_str());
				Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "jobLog = %s and jobdaemonLog = %s and lInmrset size=%d",jobLog,jobdaemonLog,rsset.getSize());
	
				
	 
				 
				int res= remove(jobLog);
				int res1 =remove(jobdaemonLog);
				Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "joblogdel res=%d and jobdaemonlogdel res=%d",res,res1);
	 
				}
			//end
			szSQL.str("");	
			szSQL << "delete  from frbStatus where endTime < DATE_SUB(NOW(), INTERVAL " << threshold << " DAY ) and endTime <> 0";
			m_InmCon->execQuery((szSQL.str().c_str()));
			}
		}
		szSQL.str("");	
		szSQL << "select jobConfigId from frbJobs where assignedState = 2 ";
		DBResultSet lInmrset	= m_InmCon->execQuery((szSQL.str().c_str()));
		string szIdlist		= "";

		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				DBRow row	= lInmrset.nextRow();
				long id		= row.nextVal().getInt();

				
				std::string szSQL1 = "select j.id, sc.startTime, st.startTime, jc.jobOrder";
				std::string szSQL2 = " from svsdb1.frbJobs j, svsdb1.frbJobConfig jc, svsdb1.frbSchedule sc, svsdb1.frbStatus st";

				std::stringstream szSQL3;
				szSQL3 << " where j.jobConfigId = " << id << " and jc.scheduleId = sc.id and j.id = st.id and j.JobConfigId = jc.id and jc.deleted = 0";

				szSQL.str("");
				szSQL << szSQL1 << szSQL2 << szSQL3.str() ;
				

				DBResultSet rset2 = m_InmCon->execQuery((szSQL.str().c_str()));

				if( !rset2.isEmpty() )
				{
					row					= rset2.nextRow();

					long mysql_id		= row.nextVal().getInt();
					string scStartTime	= row.nextVal().getString();
					string stStartTime	= row.nextVal().getString();
					long jobOrder		= row.nextVal().getInt();
					
					long lscStartTime	= convertStringToTime(scStartTime);
					long lstStartTime	= convertStringToTime(stStartTime);
					
					
					
					//code to generate counter with synchronization
					sch_mtx.lock();
					lCount = genCount();
					sch_mtx.unlock();
					

					std::stringstream SPOut;
					SPOut << "CREATE PROCEDURE `"<<"inm_sch_dyn_procedure_"<<lCount<<"`"<<" ()"<< std::endl
						  << "BEGIN" << std::endl
						  << "  DECLARE frbJobId BIGINT DEFAULT NULL;" << std::endl
						  << "  DECLARE jobConfId BIGINT DEFAULT " << id << ";" << std::endl
						  << "  DECLARE scheduleStartTime DATETIME DEFAULT '" << (lscStartTime ? scStartTime : "0000-00-00 00:00:00") << "';" << std::endl
						  << "  DECLARE error TINYINT DEFAULT 0;" << std::endl
						  << "  DECLARE CONTINUE HANDLER FOR SQLSTATE '23000' SET error = 1;" << std::endl
						  << "  START TRANSACTION;" << std::endl
						  << "  DELETE FROM frbJobs WHERE jobConfigId = jobConfId;" << std::endl
						  << "  IF error = 0 THEN" << std::endl
						  << "    INSERT INTO frbJobs (jobConfigId, assignedState) VALUES (jobConfId, 0);" << std::endl
						  << "    SET frbJobId = LAST_INSERT_ID();" << std::endl
						  << "  END IF;" << std::endl
						  << "  IF error = 0 THEN" << std::endl
						  << "    INSERT INTO frbStatus (id,jobConfigId,startTime) VALUES( frbJobId, jobConfId, scheduleStartTime);" << std::endl
						  << "  END IF;" << std::endl
						  << "  IF error THEN" << std::endl
						  << "    ROLLBACK;" << std::endl
						  << "  ELSE" << std::endl
						  << "    COMMIT;" << std::endl
						  << "  END IF;" << std::endl
						  << "  SELECT error;" << std::endl
						  << "END" << std::endl;

					m_InmCon->execQuery((SPOut.str().c_str()),true);

					
					std::stringstream szBuffer;
					szBuffer << id;
					szIdlist += szBuffer.str();					
					szIdlist += ",";
				}
			}

			if(szIdlist.size() > 0)
			{
				szIdlist.erase(szIdlist.size() - 1, 1);
			}

		}
	}
	else
		Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "invalid sql connection object");
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateCompletedJobs ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "updateCompletedJobs  Default Exception raised");
}
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::updateCompletedJobs");
}

ERP* FileRepManager::nextERP()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::nextERP");
	PERP perp				= NULL;
	TaskDefinition* tskdef	= NULL;
try
{
	updateCompletedJobs();
	getNewJobs();

	if(m_Cnt < m_ScheduledJobs.size())
	{
		map<long,vector<long> >::iterator iter = m_ScheduledJobs.begin();
		unsigned int i = 0;

		for(i = 0; i < m_Cnt; i++, iter++){ }

		if(iter != m_ScheduledJobs.end())
		{
			vector<long>& m_GroupList = iter->second;
			perp = createNewERP(m_GroupList[0], m_InmCon);

            if(perp != NULL)
            {
				BaseObject *obj = new FileRepImpl;
				perp->setObject(obj);
				TaskDefinition* tskdef = dynamic_cast<TaskDefinition*>(perp->getInformation());
				m_Cnt++;
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextERP ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextERP  Default Exception raised");
}
	return perp;
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::nextERP");
}

ERP* FileRepManager::createNewERP(long jobConfigId, Connection* con)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::createNewERP");
	std::stringstream szSQL;
	PERP perp			= NULL;
try
{
	
	if( con != NULL )
	{

		szSQL << "select groupId, jobOrder, scheduleId from frbJobConfig where id = " << jobConfigId;
		
		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

		if(!lInmrset.isEmpty())
		{
			DBRow row	= lInmrset.nextRow();

			long gid	= row.nextVal().getInt();
			long order	= row.nextVal().getInt();
			long schid	= row.nextVal().getInt();
			szSQL.str("");
			szSQL << "select * from frbSchedule where id = " << schid;
			lInmrset = con->execQuery((szSQL.str().c_str()));

			if(!lInmrset.isEmpty())
			{
				row					= lInmrset.nextRow();
				schid				= row.nextVal().getInt();
				string startTime	= row.nextVal().getString();
				int repeat			= row.nextVal().getInt();

				SchData schdata;

				schdata.everyDay	= row.nextVal().getInt();
				schdata.everyHour	= row.nextVal().getInt();
				schdata.everyMinute = row.nextVal().getInt();
				schdata.atYear		= row.nextVal().getInt();
				schdata.atMonth		= row.nextVal().getInt();
				schdata.atDay		= row.nextVal().getInt();
				schdata.atHour		= row.nextVal().getInt();
				schdata.atMinute	= row.nextVal().getInt();
				schdata.everyYear	= 0;
				schdata.everyMonth	= 0;

				perp = new ERP;
				if( perp != NULL )
				{
					perp->setSchData(schdata);
					perp->setType(2);
					perp->setSynchronous(false);
					perp->setStartTime(convertStringToTime(startTime));
					perp->setLastExecutionTime(0);

					if(repeat)
					{
						perp->setRepeatition(-1);		//execute repeatedly
					}
					else
					{
						perp->setRepeatition(0);		//execute once
					}
					szSQL.str("");
					szSQL << "select executionType from frbJobGroups where id = " << gid;
					lInmrset = con->execQuery((szSQL.str().c_str()));

					if(!lInmrset.isEmpty())
					{
						row					= lInmrset.nextRow();
						int executionType	= row.nextVal().getInt();
						TaskDefinition *tskdef	= NULL;
						tskdef	= new TaskDefinition;
						if( tskdef != NULL ) 
						{
							tskdef->taskId			= jobConfigId;
							tskdef->executionType	= executionType;
							tskdef->order			= order;
							tskdef->scheduleId		= schid;
							tskdef->taskGroupId		= gid;
							tskdef->erpType			= SCH_MASTER;
						}
						perp->setInformation(tskdef);
						perp->setSizeInformation(sizeof(tskdef));

						if(repeat)
						{
							perp->setFrequency(schdata.everyDay * 24 * 60 * 60 + schdata.everyHour * 60 * 60 + schdata.everyMinute * 60);
						}

						perp->setCompletionRoutine(completionRoutineMaster);
					}
				}
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "createNewERP ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "createNewERP  Default Exception raised");
}
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::createNewERP");
	return perp;
}

vector<PERP> FileRepManager::nextBatch(TaskDefinition* job, PERP masterErp)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::nextBatch");
	std::stringstream szSQL;
	Connection *con			= NULL;
	con						= createConnection();
	vector<PERP> vERP;
	PERP perp = NULL;
try
{
	if(con != NULL )
	{
		if(job->taskId != -1)
		{
			szSQL.str("");
			szSQL << "select id, scheduleId from frbJobConfig where groupId = " << job->taskGroupId << " and jobOrder = " << job->order << " and id<> " << job->taskId;
			
		}
		else
		{
			szSQL.str("");
			szSQL << "select id, scheduleId from frbJobConfig where groupId = " << job->taskGroupId << " and jobOrder = " << job->order;
			
		}

		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));
		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				DBRow row	= lInmrset.nextRow();
				long id		= row.nextVal().getInt();
				perp	= createNewERP(id,con);

				if(perp == NULL) 
				{
					Logger::Log("FileRepManager.cpp", __LINE__, 1, 0, "createNewERP failed to create the ERP");
					continue; 
				}

				perp->setType(2);
				perp->setSynchronous(false);
				perp->setStartTime(masterErp->getStartTime());
				perp->setNextScheduledTime(masterErp->getNextScheduledTime());
				perp->setPrevScheduledTime(masterErp->getPrevScheduledTime());

				if(masterErp->getLastExecutionTime() == masterErp->getStartTime())
				{
					perp->setLastExecutionTime(0);
				}
				else
				{
					perp->setLastExecutionTime(masterErp->getLastExecutionTime());
				}
				TaskDefinition *tskdef	= NULL;
				tskdef	= dynamic_cast<TaskDefinition*>(perp->getInformation());

				if(tskdef != NULL)
					tskdef->erpType			= SCH_WORKER;

				BaseObject *obj			= new FileRepImpl;

				perp->setObject(obj);
				perp->setCompletionRoutine(completionRoutineWorker);

				vERP.push_back(perp);
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextBatch ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextBatch  Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::nextBatch");
	return vERP;
}

void FileRepManager::freeBatch(vector<PERP>& vBatch)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::freeBatch");
try
{
	for(unsigned int i = 0; i < vBatch.size(); i++)
	{
		PERP perp = vBatch[i];
		SAFE_DELETE( perp );
	}
}
catch(char * str)
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextBatch ()::Exception raised=%s",str);
}
catch(...) 
{  
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "nextBatch  Default Exception raised");
}	
	
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::freeBatch");
}

vector<TaskDefinition> FileRepManager::onView()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::onView");
	vector<TaskDefinition> vTskdef;
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::onView");
	return vTskdef;
}

FileRepManager::~FileRepManager()
{
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "FileRepManager::~FileRepManager");
	SAFE_SQL_CLOSE( m_InmCon );
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, "Exiting FileRepManager::~FileRepManager");
}


int FileRepManager::genCount()
{
	if(procCount>= gMaxCount)
		procCount = 0;
	else
		procCount++;
	Logger::Log("FileRepManager.cpp", __LINE__, 0, 0, " procCount value = %d",procCount);
	return procCount;
}
