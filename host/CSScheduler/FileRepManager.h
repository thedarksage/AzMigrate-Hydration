#ifndef __FILE_REP_MANAGER_H_
#define __FILE_REP_MANAGER_H_
//#include <windows.h>
#include "CXConnection.h"
#include "ERP.h"
#include "util.h"
#include <map>
#include "ExecutionController.h"
#include "IRepositoryManager.h"

using namespace std;

long WaitForCompletionMultiple(vector<long> vIdList,Connection *con,bool bWaitForAll);

SCH_STATUS WaitForCompletionSingle(string szIdlist, Connection *con);

SCH_STATUS waitForRunning(PERP perp, string szJobId, long timeout);

void updateJobStatus(PERP perp);

void completionRoutineWorker(PERP perp,bool success);

void completionRoutineMaster(PERP perp,bool success);

class FileRepManager:public IRepositoryManager
{
	Connection *m_InmCon;

	map<long,vector<long> > m_ScheduledJobs;

	unsigned long m_Cnt;

	void updateCompletedJobs();

	void updatePrevIncompleteJobs();

	ERP* createNewERP(long jobConfigId, Connection* con);
	int genCount();

public:
	FileRepManager();

	

	//Callbacks that are called by the scheduler on start of execution and on end of execution resp.
	void onSchedule(Status *status, void *context);		//context points to the copy of the job manager defined structure

	void onFinished(Status *status, void *context);

	void onStart(void *context);

	//This function is called when a job is removed from the scheduler.
	void onRemove(void *context, Status *status);

	void onStop(Status *status, void *context);		//Status is a structure allocated containing the status 

	void getNewJobs();

	void LoadScheduledSnapshotVetor(IRepositoryManager* repmgr);

	//Fetch all records from database which are to be scheduled;
	ERP* nextERP();

	vector<ERP*> nextBatch(TaskDefinition* tskdef, PERP masterErp);

	vector<TaskDefinition> onView();

	bool isCompletedWithSuccess(vector<PERP> vBatch);

	void freeBatch(vector<PERP>& vBatch);

	~FileRepManager();
};

#endif
