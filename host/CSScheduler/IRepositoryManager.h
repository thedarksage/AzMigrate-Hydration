#ifndef __IREPOSITORY_MANAGER_H_
#define __IREPOSITORY_MANAGER_H_
#include "common.h"
#include "TaskDefinition.h"

typedef struct _Status Status;
class ERP;

class IRepositoryManager
{
public:
	virtual void onSchedule(Status *status,void *context)=0;		//context points to the copy of the job manager defined structure

	virtual void onFinished(Status *status,void *context)=0;

	virtual void onStart(void *context)=0;

	virtual void onRemove(void *context,Status *status)=0;

	virtual void onStop(Status *status,void *context)=0;		//Status is a structure allocated containing the status 

	virtual ERP* nextERP()=0;

	virtual void LoadScheduledSnapshotVetor(IRepositoryManager* repmgr)=0;

};

typedef struct _Status
{
	SCH_STATUS Success;

	void *information;

	int size;

	long executedTime;

	long frequency;

	int repeat;

	unsigned long nextExecutionTime;

}Status;






#endif
