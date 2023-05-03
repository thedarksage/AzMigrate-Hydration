#ifndef __ERP_H_
#define __ERP_H_
#include <string>
#include "util.h"
#include "common.h"
#include "ExecutionObject.h"
#include "TaskDefinition.h"

class ExecutionController;
class ExecutionEngine;
class IRepositoryManager;

typedef long ERPID;
class ERP;

using namespace std;
class ERP
{
	ERPID m_Id;		//This id is interanally used by scheduler to distinguish the erp
					//within the system.
	void *m_Data;		//User defined data used by actual Task
	long m_SizeData;	//Size of userdefined data.

	//Information required by execution controller
	unsigned long m_Frequency;
	unsigned long m_StartTime;
	unsigned long m_LastExecutionTime;
	unsigned long m_NextExecutionTime;
	unsigned long m_PrevScheduledTime;
	int m_Repeatition;

	//Information required by execution engine
	string m_Target;			//name of the target.
	bool m_Synchronous;		//default = false.
	void (* m_CancelRoutine) (void *);	//Routine to cancel ERP in between
	void (* m_CompletionRoutine) (ERP *,bool);	//Routine called after completion of ERP.
											//This routine is guaranteed to be called after
											//every successful/unsuccessful completion.
											//To Do: Call this routine depending on the user configured
											//criterian specified based on status of the execution.

	//Information required by execution agent
	int m_Type;		//0 -> Process : 1 -> Script : 2 -> Object	Information to tell about creation of task
	BaseObject *m_Object;	//Object is created by repository manager and pointer is set into the repository.

	//Information used by Completion Routine and Repository Manager.
	SCH_STATUS m_ExecutionStatus;		//Succeess/Failure
	ITaskDefinition *m_Information;		//Data returned, Can be used whichever way user wants to.
	long m_SizeInformation;		//Size of the information buffer. Is set by the handler.
								//To Do: Safeguard from the wrong size set by the user.
	IRepositoryManager *m_RepositoryManager;

	SchData m_Schdata;
	static int erpCount;
public:	
	ERP()
	{
		m_Id				= erpCount++;	//Needs thread safe interlocked increment
		m_SizeData			= 0;
		m_ExecutionStatus	= SCH_SUCCESS;
		m_CancelRoutine		= NULL;
		m_CompletionRoutine = NULL;
		m_NextExecutionTime	= 0;
		m_LastExecutionTime = 0;
		m_PrevScheduledTime	= 0;
		m_StartTime			= 0;

		m_Information		= NULL;
		m_Data				= NULL;
		m_Object			= NULL;
	}

	ERP(ERP *perp)
	{
		m_Id				= perp->m_Id;
		m_SizeData			= perp->m_SizeData;
		m_Frequency			= perp->m_Frequency;
		m_StartTime			= perp->m_StartTime;
		m_LastExecutionTime = perp->m_LastExecutionTime;
		m_NextExecutionTime = perp->m_NextExecutionTime;
		m_PrevScheduledTime = perp->m_PrevScheduledTime;
		m_Repeatition		= perp->m_Repeatition;

		m_Target			= perp->m_Target;
		m_Synchronous		= perp->m_Synchronous;
		m_CancelRoutine		= perp->m_CancelRoutine;
		m_CompletionRoutine = perp->m_CompletionRoutine;
		m_Type				= perp->m_Type;
		
		m_ExecutionStatus	= perp->m_ExecutionStatus;
		m_SizeInformation	= perp->m_SizeInformation;
		m_RepositoryManager	= perp->m_RepositoryManager;
		m_Schdata			= perp->m_Schdata;
		m_Id				= perp->getId();

          if( perp->getInformation() != NULL )
          {
              m_Information		= perp->getInformation()->clone();
          }
          else
          {
              m_Information = NULL ;
          }
		m_Object			= perp->getObject()->clone();
		m_Data				= NULL;
	}

	void setId(ERPID id){
		m_Id = id;
	}

	ERPID getId(){
		return m_Id;
	}

	void setRepositoryManager(IRepositoryManager* rmgr){
		m_RepositoryManager = rmgr;
	}
	
	void setCompletionRoutine(void (* completionRoutine) (ERP *,bool)){
		m_CompletionRoutine = completionRoutine;
	}

	void setCancelRoutine(void (* cancelRoutine) (void *)){
			m_CancelRoutine = cancelRoutine;
	}

	void setFrequency(long Frequency){
		m_Frequency = Frequency;
	}

	long getFrequency(){
		return m_Frequency;
	}

	void setStartTime(long startTime){
		m_StartTime = startTime < 0?0:startTime;
	}

	long getStartTime(){
		return m_StartTime;
	}

	void setLastExecutionTime(long lastExecutionTime){
		m_LastExecutionTime = lastExecutionTime;
	}

	long getLastExecutionTime(){
		return m_LastExecutionTime;
	}

	void setRepeatition(int repeatition){
		m_Repeatition = repeatition;
	}

	int getRepeatition(){
		return m_Repeatition;
	}

	void setTarget(string target){
		m_Target = target;
	}

	string getTarget(){
		return m_Target;
	}
	
	void setSynchronous(bool synchronous){
		m_Synchronous = synchronous;
	}
	 
	bool getSynchronous(){
		return m_Synchronous;
	}

	void setType(int type){
		m_Type = type;
	}
	 
	int getType(){
		return m_Type;
	}

	void setObject(BaseObject *object){
		m_Object = object;
	}
	 
	BaseObject *getObject(){
		return m_Object;
	}

	void setExecutionStatus(SCH_STATUS executionStatus){
		m_ExecutionStatus = executionStatus;
	}
	 
	SCH_STATUS getExecutionStatus(){
		return m_ExecutionStatus;
	}

	void setInformation(ITaskDefinition *information){
		m_Information = information;
	}
	 
	ITaskDefinition *getInformation(){
		return m_Information;
	}

	void setSizeInformation(long sizeInformation){
		m_SizeInformation = sizeInformation;
	}
	 
	long getSizeInformation(){
		return m_SizeInformation;
	}

	IRepositoryManager* getRepositoryManager(){
		return m_RepositoryManager;
	}

	void setSchData(SchData schdata){
		m_Schdata = schdata;
	}

	SchData getSchData(){
		return m_Schdata;
	}

	void setNextScheduledTime(unsigned long nexttime){
		m_NextExecutionTime = nexttime;
	}

	unsigned long getNextScheduledTime(){
		return m_NextExecutionTime;
	}

	void setPrevScheduledTime(unsigned long prevtime){
		m_PrevScheduledTime = prevtime;
	}

	unsigned long getPrevScheduledTime(){
		return m_PrevScheduledTime;
	}

	void logDetails()
	{
		Logger::Log(__FILE__,__LINE__,0,0,"ERP Details");

		m_Information->logDetails();
		m_Schdata.logDetails();

		Logger::Log(__FILE__,__LINE__,0,0, "Frequency: %ld", m_Frequency);
		Logger::Log(__FILE__,__LINE__,0,0, "StartTime:%ld", m_StartTime);
		Logger::Log(__FILE__,__LINE__,0,0, "m_LastExecutionTime:%ld", m_LastExecutionTime);
		Logger::Log(__FILE__,__LINE__,0,0, "m_NextExecutionTime:%ld", m_NextExecutionTime);
		Logger::Log(__FILE__,__LINE__,0,0, "m_PrevScheduledTime:%ld", m_PrevScheduledTime);
		Logger::Log(__FILE__,__LINE__,0,0, "m_Repeatition:%ld", m_Repeatition);
	}

	~ERP()
	{
		SAFE_DELETE( m_Object );
		SAFE_DELETE( m_Information );
	}

	friend class ExecutionController;
	friend class ExecutionEngine;
};

typedef ERP* PERP;

#endif

