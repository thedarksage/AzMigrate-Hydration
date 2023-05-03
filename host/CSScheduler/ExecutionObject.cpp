#pragma warning(disable: 4786)

#include "ERP.h"
#include "FileRepManager.h"
#include "SnapshotManager.h"
#include "util.h"
#include "ExecutionObject.h"
#include "Log.h"
#include <boost/format.hpp>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"



bool isAlreadyPresent(string szId,vector<string> &vIds);
Mutex mutexSnaphot;

string getNewsnapshotId(Connection *con)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "getNewsnapshotId");
	//get all ids from snapshot table
	
	vector<string> vIds;
	string 	szReturn = "";
try
{
	std::string szSQL;
	if(con != NULL )
	{
	
		szSQL = "select Id from snapShots";

		DBResultSet lInmrset = con->execQuery((szSQL.c_str()));
		DBRow row;

		if(!lInmrset.isEmpty())
		{
			for(unsigned int i = 0; i < lInmrset.getSize(); i++)
			{
				row = lInmrset.nextRow();
				vIds.push_back(row.nextVal().getString());
			}
		}
		

		long maxNo = 65535;
		char szNewsnapId[1024] = {0};


		/* initialize random generator */
		srand ( time(NULL) );

		/* generate some random numbers */
		std::stringstream szRand1,szRand2,szRand3,szRand4,szRand5,szRand6,szRand7,szRand8;

		do
		{
			string szRand;

			szRand1 << rand()%maxNo;
			szRand2 << rand()%maxNo;
			szRand3 << rand()%maxNo;
			szRand4 << rand()%maxNo;
			szRand5 << rand()%maxNo;
			szRand6 << rand()%maxNo;
			szRand7 << rand()%maxNo;
			szRand8 << rand()%maxNo;

			szRand += szRand1.str();
			szRand += szRand2.str();
			szRand += "-";
			szRand += szRand3.str();
			szRand += "-";
			szRand += szRand4.str();
			szRand += "-";
			szRand += szRand5.str();
			szRand += "-";
			szRand += szRand6.str();
			szRand += szRand7.str();
			szRand += szRand8.str();
			
			inm_strcpy_s(szNewsnapId, ARRAYSIZE(szNewsnapId),szRand.c_str());
			stringupr(szNewsnapId);
			findAndReplace(szNewsnapId);

		}while(isAlreadyPresent(szNewsnapId, vIds));

		szReturn = szNewsnapId;
	}
}
catch(char * str)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "getNewsnapshotId raised=%s",str);
}
catch(...) 
{  

	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "getNewsnapshotId Exception raised");
}
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "Exiting getNewsnapshotId");
	return szReturn;
}

bool isAlreadyPresent(string szId,vector<string> &vIds)
{
	bool lbRet = false;
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "isAlreadyPresent");
try
{
	for(unsigned int i = 0; i < vIds.size(); i++)
	{
		if(vIds[i].compare(szId) == 0)
		{
			lbRet = true;
			break;
		}
	}
}
catch(char * str)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "isAlreadyPresent raised=%s",str);
}
catch(...) 
{  

	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "isAlreadyPresent Exception raised");
}	
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "Exiting isAlreadyPresent");
	return lbRet;
}

SCH_STATUS FileRepImpl::execute(PERP perp)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "FileRepImpl::execute");
	TaskDefinition* tskdef			= NULL;
	FileRepManager *repmgr			= NULL;
	Connection *con					= NULL;
	SCH_STATUS lRet					= SCH_ERROR;
try
{
	if(perp != NULL)
	{
		tskdef = static_cast<TaskDefinition*>(perp->getInformation());

		if(tskdef != NULL)
		{

			//Insert job into frbJobs. Update startTime in frbStatus.
			std::stringstream szSQL;
			szSQL << "select deleted, jobOrder from frbJobConfig where id = " << tskdef->taskId;
			
			repmgr = dynamic_cast<FileRepManager *>(perp->getRepositoryManager());

			if(repmgr != NULL)
			{

				
				con = createConnection();
				
				if(con != NULL )
				{
					DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

					int deleted = 0;

					if(!lInmrset.isEmpty())
					{
						deleted = lInmrset.nextRow().nextVal().getInt();
					}
					
					if(deleted)
					{
						//Call scheduler onRemove so that it will call FileRepManager onRemove function.
						ExecutionController* ec = ExecutionController::instance();
						
						if(ec != NULL)
							ec->removeERP(perp);
						
						Logger::Log("ExecutionObject.cpp",__LINE__,0,1,"Removed ERP ID:%ld",tskdef->taskId);
						lRet = SCH_SUCCESS;
					}
					else
					{	
						szSQL.str("");
						szSQL << "select assignedState, currentState, daemonState from frbJobs where jobConfigId = " << tskdef->taskId;
						DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

						if(!lInmrset.isEmpty())
						{
							int assignedState	= 0;
							int currentState	= 0;
							int daemonState		= 0;

							DBRow row = lInmrset.nextRow();

							if(row.getSize() == 3)
							{
								assignedState	= row.nextVal().getInt();
								currentState	= row.nextVal().getInt();
								daemonState		= row.nextVal().getInt();
							}
							else
							{
								lRet = SCH_ERROR;
								Logger::Log("ExecutionObject.cpp",__LINE__,1,1,"No State information Found for ERP:%ld",tskdef->taskId);
							}

							if(assignedState == 0)
							{
								//Job is assigned Initial condition while adding through the UI
								//Start the job for execution
								szSQL.str("");
								szSQL << "update frbJobs set assignedState=1, currentState=0, daemonState=0 where jobConfigId = " << tskdef->taskId;
								con->execQuery((szSQL.str().c_str()));

								//Create status entry for the newly started job.
								szSQL.str("");
								szSQL << "select id from frbJobs where jobConfigId = " << tskdef->taskId;
								DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

								if(lInmrset.isEmpty())
								{
									Logger::Log("ExecutionObject.cpp",__LINE__,1,1,"Failed to read states of job ERP:%ld",tskdef->taskId);
									lRet = SCH_ERROR;
								}
								else 
								{

									DBRow row= lInmrset.nextRow();

									if(row.getSize() == 1)
									{
										long mysqlId = row.nextVal().getInt();

										//
										//Store the mysqlId for furhter use in status updates after completion of this job
										//
										tskdef->mysqlId	= mysqlId;
										szSQL.str("");
										szSQL << "update frbStatus set startTime = '" << convertTimeToString(perp->getLastExecutionTime()).c_str() << "' where id = " << mysqlId;
										con->execQuery((szSQL.str().c_str()));

										Logger::Log("ExecutionObject.cpp",__LINE__,0, 1, "Job started successfully. ERP:%ld",tskdef->taskId);
										lRet = SCH_SUCCESS;
									}
									else
									{
										Logger::Log("ExecutionObject.cpp",__LINE__,1,1,"Could not start job. ERP:%ld",tskdef->taskId);
										lRet = SCH_ERROR;
									}
								}
							}
							else
							{
								//Job is running. Do Nothing.
								Logger::Log("ExecutionObject.cpp",__LINE__, 1, 1,"Job already running ERP:%ld", tskdef->taskId);
								lRet = SCH_PENDING;
							}
						}
						else
						{
							lRet = SCH_PENDING;
						}
					}
				}
			}
		}
	}
}
catch(char * str)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, " FileRepImpl::execute Exception raised=%s",str);
}
catch(...) 
{  

	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "FileRepImpl::execute Default Exception raised");
}
	SAFE_SQL_CLOSE( con );
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "Exiting FileRepImpl::execute");
	return lRet ;
}

SCH_STATUS FileRepInit::execute(PERP perp)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "FileRepInit::execute");
	SCH_STATUS lRet			= SCH_ERROR;
	Connection *con			= NULL;
	FileRepManager *repmgr	= NULL;
	TaskDefinition *tskdef	= NULL;
try
{
	if(perp != NULL)
	{

		//Insert job into frbJobs. Update startTime in frbStatus.
		con = createConnection();
		repmgr = dynamic_cast<FileRepManager *>(perp->getRepositoryManager());

		

		if(con != NULL )
		{
			tskdef = static_cast<TaskDefinition *>(perp->getInformation());

			if(tskdef != NULL)
			{
				std::stringstream szBuffer;
				szBuffer << tskdef->taskId;

				//Wait for this erp to complete.
				WaitForCompletionSingle(szBuffer.str(),con);
				lRet = SCH_SUCCESS;
			}
		}
	}
}
catch(char * str)
{
Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "FileRepInit::execute::Exception raised=%s",str);

}
catch(...) 
{  

Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "FileRepInit::execute Default Exception raised");

}
	SAFE_SQL_CLOSE( con );
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 0, "Exiting FileRepInit::execute");
	return lRet ;
}

///
/// Replaces (Translates) all instances of inputChar in the string pszInString with outputChar. This is
/// an inplace replacement; no new memory is allocated.
/// Equivalent to perl's tr// operator.

void BaseSnapshot::ReplaceChar(char *pszInString, char inputChar,
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
Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "ReplaceChar:Exception raised=%s",str);

}
catch(...) 
{  

Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "ReplaceChar:: Default Exception raised");

}
}

SCH_STATUS BaseSnapshot::execute(PERP perp)
{
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "BaseSnapshot::execute");
	SCH_STATUS lRet				= SCH_ERROR;
	SnapshotDefinition *tskdef	= NULL;
	if(perp == NULL)
	{
		Logger::Log("ExecutionObject.cpp",__LINE__,1,1,"Invalid ERP");
		return SCH_ERROR;
	}
	tskdef= static_cast<SnapshotDefinition *>(perp->getInformation());

	if(tskdef == NULL)
	{
		Logger::Log("ExecutionObject.cpp",__LINE__,1,2,"Task Information Could Not be Found");
		Logger::Log("ExecutionObject.cpp", __LINE__, 1, 2, "Exiting BaseSnapshot::execute");
		return SCH_ERROR;
	}

	SnapshotManager *repmgr = dynamic_cast<SnapshotManager*>(perp->getRepositoryManager());

	
	Connection *con = createConnection();
try
{	
	if( con == NULL  )
	{
		Logger::Log("ExecutionObject.cpp",__LINE__,1,2,"Connection object is NULL, ERP execution terminated");
		Logger::Log("ExecutionObject.cpp", __LINE__, 1, 2, "Exiting BaseSnapshot::execute");
		SAFE_DELETE( con );
		return SCH_ERROR;
	}

	//--------------------------------------------------------------------------

	std::stringstream szSQL;

	//Update next schedule id
    //Check if the erp is periodic whether the nextScheduledTime is valid or not
    if(perp->getRepeatition() != -1 || perp->getNextScheduledTime() != 0)
    {
	    szSQL << "update snapSchedule set startTime = '" << convertTimeToString(perp->getNextScheduledTime()) <<"'  where id = " << tskdef->scheduleId;
	    con->execQuery((szSQL.str().c_str()));
    }

	if(tskdef->szSnapshotId.size() == 0)
	{
		szSQL.str("");
		szSQL << "select executionState from snapShots where snapshotId = " << tskdef->taskId;
	}
	else
	{
		szSQL.str("");
		szSQL << "select executionState from snapShots where id = " << tskdef->szSnapshotId;
	}

	DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

	int executionState	= 0;
	string szId			= "";

	if(!lInmrset.isEmpty())
	{
		DBRow row		= lInmrset.nextRow();
		TypedValue val	= row.nextVal();

		if(val.getType() != TYPENULL)
		{
			executionState = val.getInt();
		}
	}

	if(tskdef->taskId == 0)
	{
		//This is an asynchronously scheduled snapshot.
		mutexSnaphot.lock();

		
		szSQL.str("");
		szSQL << "select snapshotId,srcHostid,srcDeviceName,destHostid,destDeviceName from snapShots where id = " << tskdef->szSnapshotId;
		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

		long	id			= 0;
		int		readyFlag	= 0;

		string	srcid		= "";
		string	srcname		= "";
		string	destid		= "";
		string	destname	= "";
		

		if(!lInmrset.isEmpty())
		{
			DBRow row = lInmrset.nextRow();

			if(row.getSize() == 5)
			{
				id			= row.nextVal().getInt();
				srcid		= row.nextVal().getString();
				srcname		= row.nextVal().getString();
				destid		= row.nextVal().getString();
				destname	= row.nextVal().getString();

                size_t srclen;
                INM_SAFE_ARITHMETIC(srclen = InmSafeInt<size_t>::Type(srcname.size())*2+1, INMAGE_EX(srcname.size()))
                char* src = new char[srclen];
				inm_sql_escape_string(src,(char*)srcname.c_str(),srcname.size());

                size_t destlen;
                INM_SAFE_ARITHMETIC(destlen = InmSafeInt<size_t>::Type(destname.size())*2+1, INMAGE_EX(destname.size()))
				char* dest = new char[destlen];
				inm_sql_escape_string(dest,(char*)destname.c_str(),destname.size());		//ReplaceChar((char*)srcname.c_str(),'\\','/');	
				
				szSQL.str("");
				szSQL << "select destDeviceName CurrentRun from snapShots where destDeviceName = '" << destname.c_str() << "' and destHostid =  '"<< destid.c_str() << "' and executionState != 0 and executionState != 4 and executionState != 5 and id != " << tskdef->szSnapshotId;
				DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

				if(lInmrset.isEmpty())
				{
					//No Snpahot is currently in process on the required snapshot drive.
					//Hence schedule the current snapshot.
					szSQL.str("");
					szSQL << "update snapShots set executionState = 1 where id = " <<tskdef->szSnapshotId;
					DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));
				}
				else
				{
					SAFE_SQL_CLOSE( con );
					SAFE_DELETE_ARRAY( src );
					SAFE_DELETE_ARRAY(dest);
					
					mutexSnaphot.unlock();
					Logger::Log("ExecutionObject.cpp", __LINE__, 1, 2, "Exiting BaseSnapshot::execute");
					return SCH_PENDING;
				}
				SAFE_DELETE_ARRAY(src);
				SAFE_DELETE_ARRAY(dest);
				

			}
		}
		mutexSnaphot.unlock();
	}
	else
	{
		std::stringstream szSQL;

		
		// VSNAPFC - Persistence
		// We need to select scheduledVol - mountpoint
		// as well as lastScheduledVol - Device Name 
		
		szSQL.str("");
		szSQL << "select snapshotId,srcHostid,srcDeviceName,destHostid,lastScheduledVol,scheduledVol,Deleted,volumeType from snapshotMain where snapshotId = " << tskdef->taskId;
		DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

		long	id			= 0;
		int		readyFlag	= 0;
		string srcid		= "";
		string srcname		= "";
		string destid		= "";
		string destname		= "";
		string mountpoint	= "";
		long	deleted		= 0;
		long volType		=0;
		long snapType		=0;

		if(!lInmrset.isEmpty())
		{
			DBRow row = lInmrset.nextRow();

			if(row.getSize() > 0)
			{
				id			= row.nextVal().getInt();
				srcid		= row.nextVal().getString();
				srcname		= row.nextVal().getString();
				destid		= row.nextVal().getString();
				destname	= row.nextVal().getString();
				// VSNAPFC - Persistence
				mountpoint	= row.nextVal().getString();
				deleted		= row.nextVal().getInt();
				volType		= row.nextVal().getInt();
                size_t srclen;
                INM_SAFE_ARITHMETIC(srclen = InmSafeInt<size_t>::Type(srcname.size())*2+1, INMAGE_EX(srcname.size()))
                char* src = new char[srclen];
				inm_sql_escape_string(src,(char*)srcname.c_str(),srcname.size());

                size_t destlen;
                INM_SAFE_ARITHMETIC(destlen = InmSafeInt<size_t>::Type(destname.size())*2+1, INMAGE_EX(destname.size()))
				char* dest = new char[destlen];
				inm_sql_escape_string(dest,(char*)destname.c_str(),destname.size());		//ReplaceChar((char*)srcname.c_str(),'\\','/');	

				// VSNAPFC - Persistence	
                size_t mountlen;
                INM_SAFE_ARITHMETIC(mountlen = InmSafeInt<size_t>::Type(mountpoint.size())*2+1, INMAGE_EX(mountpoint.size()))
				char* mount = new char[mountlen];
				inm_sql_escape_string(mount,(char*)mountpoint.c_str(),mountpoint.size());		//ReplaceChar((char*)srcname.c_str(),'\\','/');	

				long isMnt =0;
				
				if(volType ==2) {
					isMnt = 1;
				}
				
				if(deleted)	{
					//Call scheduler onRemove so that it will call FileRepManager onRemove function.
					ExecutionController* ec = ExecutionController::instance();
                    
                    if(ec != NULL)
					    ec->removeERP(perp);
					
					Logger::Log("ExecutionObject.cpp",__LINE__,0,1,"Removed ERP ID:%ld",tskdef->taskId);
					SAFE_SQL_CLOSE( con );
					SAFE_DELETE_ARRAY( src );
					SAFE_DELETE_ARRAY( dest );
					
					// VSNAPFC - Persistence
					SAFE_DELETE_ARRAY( mount );
					
					Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "Exiting BaseSnapshot::execute");
					return SCH_SUCCESS;
				}

				
				// Check if the scheduledVol is already running.
				//To avoide the race condition between two jobs checking for free
				//snaphsot drive.

                //
                //Check if this snapshot schedule is new or it is a missed out schedule
                //
                if(tskdef->szSnapshotId.length() == 0)
                {
				    mutexSnaphot.lock();

                    szSQL.str("");
                    szSQL << "select destDeviceName CurrentRun from snapShots where destDeviceName = '" << dest  << "' and destHostid = '"<< destid<< "' and srcHostid= '"<< srcid<< "' and srcDeviceName = '" << src << "' and executionState = 0 ";
				    DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));

                    if(!lInmrset.isEmpty())
                    {
                        if(lInmrset.getSize() >= 1)
                        {
                            //
                            // If a snapshot is alreayd queued for this target drive then 
                            // do not schedule any new snapshot on the same drive.
                            //
                            // however, check if we can move the pending job to
                            // ready state
			    			SnapshotUtlClass *utlObj = SnapshotUtlClass::instance();

                            utlObj->SchedulePendingJobs(destid, destname, con, repmgr);
                            mutexSnaphot.unlock();
							SAFE_SQL_CLOSE( con );
							SAFE_DELETE_ARRAY( src );
							SAFE_DELETE_ARRAY( dest );
							
							// VSNAPFC - Persistence
							SAFE_DELETE_ARRAY( mount );
							
							Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "Exiting BaseSnapshot::execute");
                            return SCH_PENDING;
                        }
                    }

				    szSQL.str("");
				    szSQL << "select destDeviceName CurrentRun from snapShots where destDeviceName = '"<< dest << "' and destHostid = '" << destid << "' and executionState != 0 and executionState != 4 and executionState != 5";
				    lInmrset = con->execQuery((szSQL.str().c_str()));

				    if(!lInmrset.isEmpty())
				    {
					    //When snapshot drive is already in use
					    DBRow row = lInmrset.nextRow();

					    if(row.getSize() == 1)
					    {	
						    string CurrRunVol		= row.nextVal().getString();
						    readyFlag				= 1;
						    szId					= getNewsnapshotId(con);
						    tskdef->szSnapshotId	= szId;
								

						    // VSNAPFC - Persistence
						    // also inserting mountedOn into snapShots
	

						    boost::format formatIter("insert into snapShots (id,srcHostid,srcDeviceName,destHostid,destDeviceName,mountedOn,pending,isMounted,snapshotId,executionState,snaptype) values ('%1%','%2%','%3%','%4%','%5%','%6%',%7%,%8%,%9%,%10%,%11%)");


						    formatIter % szId.c_str() % srcid.c_str() % src % destid.c_str() %dest % mount % 1 % isMnt % id % 0 % (snapType = (volType)?4:0);

						    DBResultSet lrset_insert = con->execQuery((formatIter.str().c_str()));

						    if(lrset_insert.getRowsUpdated() == 0)
							{
								Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "ERROR::INSERT QUERY FAILED TO UPDATE IN DB");
							}
							Logger::Log("ExecutionObject.cpp",__LINE__,0,2,"Snapshot in Queue for Snapshot ID:%d Source ID:%s  Source Name: %s Dest ID:%s Dest Name:%s ",tskdef->taskId,srcid.c_str(),src,destid.c_str(),dest);

						    mutexSnaphot.unlock();
							SAFE_SQL_CLOSE( con );
							SAFE_DELETE_ARRAY(src);
							SAFE_DELETE_ARRAY(dest);
							SAFE_DELETE_ARRAY(mount);
							
							Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "Exiting BaseSnapshot::execute");
						    return SCH_PENDING;
					    }
				    }
				    else
				    {
					    //When snapshot drive is free
					    readyFlag				= 0;
					    szId					= getNewsnapshotId(con);
					    tskdef->szSnapshotId	= szId;

					
					// VSNAPFC - Persistence
					// Inserting mountedOn into Snapshots		    
		

					    boost::format formatIter("insert into snapShots (id,srcHostid,srcDeviceName,destHostid,destDeviceName,mountedOn,pending,isMounted,snapshotId,executionState,snaptype) values ('%1%','%2%','%3%','%4%','%5%','%6%',%7%,%8%,%9%,%10%,%11%)");

					    formatIter % szId.c_str() % srcid.c_str() % src % destid.c_str() % dest % mount % 1 % isMnt % id % 1 % ( snapType = (volType)?4:0);


					    DBResultSet lrset1_insert = con->execQuery((formatIter.str().c_str()));
						if(lrset1_insert.getRowsUpdated() == 0)
						{
							Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "ERROR::INSERT QUERY FAILED TO UPDATE IN DB");
						}

					   Logger::Log("ExecutionObject.cpp",__LINE__,0,2,"Snapshot Started for Snapshot ID:%d Source ID:%s  Source Name: %s Dest ID:%s Dest Name:%s ",tskdef->taskId,srcid.c_str(),src,destid.c_str(),dest);
						szSQL.str("");
					    szSQL << "update snapshotMain set status = " << 0 <<", bytesSent = " << 0 << " where snapshotId = " << tskdef->taskId;
					    DBResultSet lInmrset = con->execQuery((szSQL.str().c_str()));
				    }

				    mutexSnaphot.unlock();
                }
				SAFE_DELETE_ARRAY(src);
				SAFE_DELETE_ARRAY(dest);
				SAFE_DELETE_ARRAY(mount);
				
			} // end of if part, for recordset not empty
				
		}
		else
		{
			SAFE_SQL_CLOSE( con );
			Logger::Log("ExecutionObject.cpp",__LINE__,1,2,"No Snapshot Entry Found with ID:%ld",tskdef->taskId);

			return SCH_ERROR;
		}
	}

	while(1)
	{
		szSQL.str("");
		szSQL << "select pending,bytesSent,status,executionState from snapShots where id = " << tskdef->szSnapshotId;
		lInmrset = con->execQuery((szSQL.str().c_str()));
		
		if (!lInmrset.isEmpty())
		{
				DBRow row = lInmrset.nextRow();
				int pending		= row.nextVal().getInt();
				int bytesent	= row.nextVal().getInt();
				int status		= row.nextVal().getInt();
				int estate		= row.nextVal().getInt();

				if(estate >= 4)
				{
					break;
				}
				else
				{
                    szSQL.str("");
                    szSQL << "update snapshotMain set status = " << status << ", bytesSent = "<< bytesent<< ", pending = 1 where snapshotId = " <<tskdef->taskId;
					con->execQuery((szSQL.str().c_str()));
				}
				sch::Sleep(30000);
		}
		else
			break;
		
	}
}
catch(char * str)
{
Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "BaseSnapshot::execute Exception raised in Execute=%s",str);
}
catch(...) 
{  
Logger::Log("ExecutionObject.cpp", __LINE__, 0, 1, "BaseSnapshot::execute() Default Exception raised");
}

//-------------------------------------------------------------------------------------------
	SAFE_SQL_CLOSE( con );
	Logger::Log("ExecutionObject.cpp", __LINE__, 0, 2, "Exiting BaseSnapshot::execute");
	return SCH_SUCCESS;
}
