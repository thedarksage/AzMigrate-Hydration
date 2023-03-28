#ifndef CLOUD_CONTROLLER_H
#define CLOUD_CONTROLLER_H
#include "app/appcontroller.h"
#include "util.h"
#include "configwrapper.h"
#include "ErrorLogger.h"
#include <boost/lexical_cast.hpp>

#define INM_CLOUD_PREP_TARGET_CONF_FILE		"prepTgtConfData.dat"
#define INM_CLOUD_PREP_TARGET_RESULT_FILE	"prepTgtResult.dat"
#define INM_CLOUD_RECOVERY_CONF_FILE		"recoveryConfData.dat"
#define INM_CLOUD_RECOVERY_RESULT_FILE		"recoveryResult.dat"
#define INM_CLOUD_DRDRILL_CONF_FILE			"DrDrillConfData.dat"
#define INM_CLOUD_DRDRILL_RESULT_FILE		"DrDrillResult.dat"

#define INM_CLOUD_RECOVERY_STATUS			"RecoveryStatus"
#define INM_CLOUD_PLACE_HOLDER				"PlaceHolder"
#define INM_CLOUD_MT_HOSTNAME               "MTHostName"
#define INM_CLOUD_LOG_FILE                  "LogFile"

#define INM_CLOUD_RECOVERY_PRESCRIPT_LOG		"PreScriptLog"
#define INM_CLOUD_RECOVERY_PRESCRIPT_STATUS		"PreScriptStatus"
#define INM_CLOUD_RECOVERY_POSTSCRIPT_LOG		"PostScriptLog"
#define INM_CLOUD_RECOVERY_POSTSCRIPT_STATUS	"PostScriptStatus"

#define INM_CLOUD_CS_REMOTE_DIR              "/home/svsystems/var/cli/"            //This path is used to upload policy log file
#define INM_CLOUD_RECOVERY_STATUS_FILE       "_recoveryprogress"

inline int InmATOI(const std::string &strNum)
{
	int iRet = -1;
	if(!strNum.empty() && strNum.find_first_not_of("0123456789") == std::string::npos)
		iRet = atoi(strNum.c_str());
	else
		DebugPrintf(SV_LOG_ERROR,"Invalid input : [%s]\n",strNum.c_str());

	return iRet;
}

class CloudController;
typedef boost::shared_ptr<CloudController> CloudControllerPtr;


/// \brief Filter driver notifier exception utility macro
#define APPFRAMEWORK_EXCEPTION(code, message) AppFrameWorkException(__FILE__, __LINE__, FUNCTION_NAME, code, message)

/// \brief exception thrown from cloudcontroller
class AppFrameWorkException : public std::exception
{
public:

	///\ brief constructor
	AppFrameWorkException(const char *fileName,    ///< source filename
		const int &lineNo,                         ///< line number
		const std::string &functionName,           ///< function name
		const std::string &code,                   ///< code
		const std::string &message                 ///< message
		) : m_Code(code),
		m_Message(std::string("FILE: ") + fileName +
		std::string(", LINE: ") + boost::lexical_cast<std::string>(lineNo)+
		std::string(", FUNCTION NAME: ") + functionName +
		std::string(", MESSAGE: ") + message +
		std::string(", CODE: ") + code)
	{}

	/// \brief destructor
	~AppFrameWorkException() throw() {}

	/// \brief message including location and error code
	const char* what() const throw() { return m_Message.c_str(); }

	/// \brief return error code as string
	///
	const char*  code() const throw() { return m_Code.c_str(); }

private:
	std::string m_Code;     ///< code

	std::string m_Message; ///< message
};

typedef std::map<std::string, std::string> hctlDeviceMap_t;
typedef std::map<std::string, std::string> srcdiskTgtlunMap_t;
typedef std::map<std::string, std::string> srcdiskTgtDiskMap_t;
typedef std::map<std::string, std::string> srcdiskTgtDMDiskMap_t;

class CloudController : public AppController
{
	Configurator * m_pConfig;
	CloudController(ACE_Thread_Manager*);
	static CloudControllerPtr m_CldControllerPtr;

	//
	typedef SVERROR (CloudController::*CldPolicyHandlerRoutine_t)(void);
	std::map<std::string,CldPolicyHandlerRoutine_t> m_CldPolicyHandlers;

	SVERROR Process();

#ifndef WIN32
	SVERROR PrepareTargetForProtection(const std::string & virtualizationPlatform,
		const SrcCloudVMsInfo & protectionDetails,
		std::stringstream & serializedResponse);

	void RefreshDeviceNodes(const std::string & virtualizationPlatform,
		const std::string & vm,
		const srcdiskTgtlunMap_t & srcdiskTgtlunMap);

	std::string GetHctl(const std::string & virtualizationPlatform,
		const std::string &vm,
		const std::string & lunId);

	void CollectDiskDevices(hctlDeviceMap_t & hctlDeviceMap);

	void FindMatchingDisks(const std::string & virtualizationPlatform,
		const std::string & vm,
		const hctlDeviceMap_t & hctlDeviceMap,
		const srcdiskTgtlunMap_t & srcdiskTgtLunMap,
		srcdiskTgtDiskMap_t & srcdiskTgtNativeDiskMap);

	std::string FindMatchingDisk(const std::string & virtualizationPlatform,
		const std::string & vm,
		const std::string & lun,
		const hctlDeviceMap_t & hctlDeviceMap
		);

	void FindCorrespondingDevMapperDisks(const std::string & vm,
		const srcdiskTgtDiskMap_t & srcdiskTgtNativeDiskMap,
		srcdiskTgtDMDiskMap_t & srcTgtDMDiskMap);

#endif

#ifdef WIN32 //Windows specifig member functions
	std::string GetPrepareTargetCommand(const std::string &planName, std::string &configFile, std::string &resultFile,const std::string & cloudEnv, bool bPhySnapshot=false);
#else		//Non-windows member functions
	SVERROR DoPrepareTargetAWS(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot);
	SVERROR GetTargetDevices(std::vector<std::string> &devices);
	SVERROR DoPrepareTargetAzure(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot);
	SVERROR GetTargetDevicesLunMapping(std::map<std::string,std::string> & device_lun_mapping);
	SVERROR ProcessLsScsiCmdLine(
		const std::string & line, 
		std::string & device_name, 
		std::string & lun,
		std::string & target_number,
		std::string & channel,
		std::string & scsi_host);
	std::string GetAzureLinuxScsiHostNumber();
	SVERROR ExecuteLinuxCmd(const std::string& command, std::string& strCmdOutput);
#endif
	std::string GetRecoveryCommand(const std::string &planName, std::string &configFile, std::string &resultFile, const std::string & cloudEnv, bool bPhySnapshot=false);
	SVERROR DoPrepareTarget(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & prepTgtInfo,const std::string & strLogFile,bool bPhySnapshot=false);
	SVERROR DoRecover(std::stringstream & updateMsgStream, const std::string & policyData, const std::string & strLogFile, bool bPhySnapshot=false);
	bool VerifyPairStatus(std::stringstream & updateMsgStream);
    SVERROR GetProtectedDiskList(std::set<std::string> &protectedDiskIds);
    void UpdateGenericError(ExtendedErrorLogger::ExtendedErrors &extendedErrors,
        std::string errorName, std::string &errMsg,
        std::map<std::string, std::string> errorMap = (std::map<std::string, std::string>()));
	bool UploadFiletoRemoteLocation(const std::string& strRemoteFile, const std::string& strFileName);
	bool DeleteRecoveryProgressFiles(const SrcCloudVMsInfo & preTgtPolicyData);
	bool InitializeConfig(ConfigSource configSrc = USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE);
	void UninitializeConfig();

	//Policy handler functions
	SVERROR OnPrepareTarget();
	SVERROR OnRecover();
	SVERROR OnDrDrill();
	SVERROR OnConsistency();
	SVERROR OnRcmRegistration();
	SVERROR OnRcmFinalReplicationCycle();
	SVERROR OnRcmResumReplication();
	SVERROR OnRcmMigration();

public:
	virtual int open(void *args=0);
	virtual int close(u_long flags=0);
	virtual int svc();

	static CloudControllerPtr getInstance(ACE_Thread_Manager *tm);

	virtual void ProcessMsg(SV_ULONG policyId);

	//Dummy definitions for base class pure virtual functions
	virtual void PrePareFailoverJobs() {}
    virtual void UpdateConfigFiles() {}
    virtual bool IsItActiveNode() { return true; }
    virtual void MakeAppServicesAutoStart() {}
    virtual void PreClusterReconstruction() {}
    virtual bool StartAppServices() { return true; }
};
#endif //~CLOUD_CONTROLLER_H
