#ifndef __SNAPSHOT_MANAGER_H_
#define __SNAPSHOT_MANAGER_H_

//#include <windows.h>
#include "CXConnection.h"
#include "ERP.h"
#include "util.h"
#include <map>
#include <set>
#include <stdexcept>
#include "ExecutionController.h"
#include "IRepositoryManager.h"
#include <string>

using namespace std;

void snapshotCompletionRoutine(ERP *erp,bool success);

class SnapshotManager:public IRepositoryManager
{
	Connection *m_InmCon;

	vector<long> m_ScheduledJobs;
	set <string> m_RunningEventSnapShots;
	unsigned long m_Cnt;

	
	void executeAsyncERP();

	long calcStartTime(SchData schdata, int sType);

public:

	SnapshotManager();

	
	
	void LoadScheduledSnapshotVetor(IRepositoryManager* repmgr);
	
	void LoadERPS(long id,IRepositoryManager* repmgr);

	//Callbacks that are called by the scheduler on start of execution and on end of execution resp.
	void onSchedule(Status *status, void *context);		//context points to the copy of the job manager defined structure

	void onFinished(Status *status, void *context);

	void onStart(void *context);

	//This function is called when a job is removed from the scheduler.
	void onRemove(void *context, Status *status);

	void onStop(Status *status, void *context);		//Status is a structure allocated containing the status 

	void getNewJobs();

	//Fetch all records from database which are to be scheduled;
	PERP nextERP();

	vector<ERP*> nextBatch(TaskDefinition* tskdef);

	vector<SnapshotDefinition> onView();

	ERP * createAsyncERP(long snapId, string szSnapshotId);
	void EventSnapshotWorker();
	void UpdateNewEventJobs();
	void ReplaceChar(char *pszInString, char inputChar, char outputChar);
	void waitForPendingJobs();
	int getSnapshotId(int index);
	~SnapshotManager();
};


class SnapshotUtlClass
{
	static SnapshotUtlClass* m_Instance;

	SnapshotUtlClass(SnapshotUtlClass&){}

	SnapshotUtlClass(){}

	SnapshotUtlClass &operator =(SnapshotUtlClass&) {
		return *this;
	}

public:

	static SnapshotUtlClass * instance() {
		if(m_Instance == 0) {
			m_Instance = new SnapshotUtlClass;
		}
		return m_Instance;
	}
	void SchedulePendingJobs(const std::string dHostid,
			const std::string dDeviceName, Connection * con, SnapshotManager *repmgr);
	~SnapshotUtlClass();
};
#endif
