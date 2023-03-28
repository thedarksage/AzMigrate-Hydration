#ifndef __TASK_DEFINITION_H_
#define __TASK_DEFINITION_H_

class ITaskDefinition
{
public:
	virtual ITaskDefinition* clone()=0;
	virtual void logDetails()=0;
	virtual ~ITaskDefinition(){ };
};

enum ERPTYPE {SCH_MASTER,SCH_WORKER};

class TaskDefinition:public ITaskDefinition
{
public:

	long taskId;
	long taskGroupId;
	long scheduleId;
	long mysqlId;
	long order;

	int executionType;

	ERPTYPE erpType;

	TaskDefinition()
	{
		taskId			= 0;
		taskGroupId		= 0;
		scheduleId		= 0;
		mysqlId			= 0;
		order			= 0;
		executionType	= 0;
		erpType			= SCH_MASTER;
	}

	ITaskDefinition* clone()
	{
		TaskDefinition *tmp	= new TaskDefinition;

		tmp->taskId			= taskId;
		tmp->taskGroupId	= taskGroupId;
		tmp->scheduleId		= scheduleId;
		tmp->mysqlId		= mysqlId;
		tmp->executionType	= executionType;
		tmp->order			= order;
		tmp->erpType		= erpType;

		return tmp;
	}

	void logDetails()
	{
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "---------------File Replication Details-------");
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "taskId:%ld\t taskGroupId:%ld \t scheduleId:%ld \t mysqlId:%ld \t order:%ld \t erpType:%s", 
															taskId, taskGroupId, scheduleId, mysqlId, order, erpType == SCH_MASTER?"Master":"Worker");
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "----------------------------------------------");
	}

	~TaskDefinition(){ }
};

class SnapshotDefinition: public ITaskDefinition
{
public:

	long taskId;
	long taskGroupId;
	long scheduleId;

	string szSnapshotId;

	ITaskDefinition* clone()
	{
		SnapshotDefinition *tmp = new SnapshotDefinition;

		tmp->taskId				= taskId;
		tmp->taskGroupId		= taskGroupId;
		tmp->scheduleId			= scheduleId;
		tmp->szSnapshotId		= szSnapshotId;

		return tmp;
	}

	void logDetails()
	{
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "---------------Snapshot Details---------------");
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "taskId:%ld \t taskGroupId:%ld \t scheduleId:%ld \t szSnapshotId:%s", taskId, taskGroupId, scheduleId, szSnapshotId.c_str());
		Logger::Log("taskdefinition.h", __LINE__, 0, 0, "----------------------------------------------");
	}
	SnapshotDefinition()
	{
		taskId = 0;
		taskGroupId = 0;
		scheduleId = 0; 
		szSnapshotId = "";
	}
	~SnapshotDefinition(){ }
};

#endif

