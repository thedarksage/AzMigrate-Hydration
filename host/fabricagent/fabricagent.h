#ifndef FABRICAGENT_H
#define FABRICAGENT_H

#include <set>
#include <string>
#include <vector>
#include "ace/Signal.h"
#include "cservice.h"
#include "talwrapper.h"
#include "rpcconfigurator.h"
#if 0
#include "switchrpcconfigurator.h"
#endif
#include <boost/shared_ptr.hpp>

#define SCSI_TGT_DEFAULT_DEVICES_PATH "/proc/scsi_tgt/groups/Default/devices"
#define SCSI_TGT_GROUPS_PATH          "/proc/scsi_tgt/groups"
#define SCSI_TGT_VDISK_PATH           "/proc/scsi_tgt/vdisk/vdisk"
#define SCSI_TGT_PATH                 "/proc/scsi_tgt"
#define SCSI_TGT_CTRL_PATH            "/proc/scsi_tgt/qla2x00tgt/control"
#define ISCSI_TGT_CTRL_PATH            "/proc/scsi_tgt/iscsi/session"
#define PATH_SEPERATOR_STR            "/"
#define SCSI_TGT_KEYWORD              "scsi_tgt"
#define ADD_GROUP_KEYWORD             "add_group"
#define DEL_GROUP_KEYWORD             "del_group"
#define NAMES_KEYWORD                 "names"
#define ADD_KEYWORD                   "add"
#define DEL_KEYWORD                   "del"
#define DEVICES_KEYWORD               "devices"
#define DEFAULT_KEYWORD               "Default"
#define UNREGISTER_KEYWORD            "unregister"

#define AT_GROUP_NAME                 "AT_NODE_GROUP"
//#define VI_NODE_WWN                   "21:00:00:00:00:00:00:00"
#define VI_NODE_WWN                   "50:02:38:31:00:00:00:00"

#define DUMMY_LUN_NAME                "DUMMY_LUN_ZERO"
#define GROUP_NAME_PREFIX             "group_"

#define DUMMY_WWN                     "00:00:00:00:00:00:00:00"

extern void VacpStart();
extern void VacpStop();

const char    SERVICE_NAME[256]       ="";
const char    SERVICE_TITLE[256]      ="";
const char    SERVICE_DESC[256]       ="";


class fabricagent : public CService,public ACE_Event_Handler
{
    public:


        fabricagent ();
        ~fabricagent ();



        virtual int handle_signal(int signum,siginfo_t * = 0,ucontext_t * = 0);
        int parse_args(int argc, ACE_TCHAR* argv[]);
        void display_usage(char *exename = NULL);

        int install_service();
        int remove_service();
        int start_service();
        int run_startup_code();
        static int StartWork(void *data);
        static int StopWork(void *data);
        static int ServiceEventHandler(signed long,void *);
        const bool isInitialized() const;

        int FindHBA(string portName, string nodeName, string & hostName);
        int EnableTargetMode(string portName, string nodeName);
        int DisableTargetMode(string portName, string nodeName);	
        void issueLip(string hbaName);

        int ExportDeviceLun (string deviceLunName, unsigned long long deviceLunNo, unsigned long long lunSize, string deviceID, int & resultState, bool);
        int RemoveDeviceLun (string deviceLunName, unsigned long long deviceLunNo, unsigned long long lunSize, string deviceID, int & resultState);

        bool CheckScstGroup(string groupName);
        bool CreateScstGroup(string groupName);
        void DeleteScstGroup(string groupName);
        bool CheckWwnInScstGroup(string groupName, string wwn);
        bool AddWwnToScstGroup(string groupName, string wwn);
        bool RemoveWwnFromScstGroup(string groupName, string wwn);

        bool CheckLunInScstGroup(std::string lunName, unsigned long long lunNo, string groupName, unsigned long long & oldLunNo);
        bool AddLunToScstGroup(std::string lunName, unsigned long long lunNo, string groupName);
        bool RemoveLunFromScstGroup(std::string lunName, unsigned long long lunNo, string groupName);
        bool RemoveLunFromAllScstGroups(string atLunName);

        void DeleteBitMapFilesOnReboot();
        void CleanUpEmptyScstGroups();
        void CleanupScst();
        void CleanupATLuns();

        bool Append(string fileName, string command);

        bool service_started;
    protected:
        virtual int RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data);
        virtual int RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data);


    protected:
        //
        // Helper methods
        //
        bool init();
        int RegisterHost();
        void ProcessAtOperations();
        bool ProcessAtOperationsOnReboot();
        void PrintAtOperations(list<ATLunOperations> & atLunOpList);
        void InvokeAtOperations(list<ATLunOperations> & atLunOpList);
        void InvokePrismAtOperations(list<PrismATLunOperations> & atLunOpList);
        void InvokeDeviceLunOperations(list<DeviceLunOperations> & deviceLunOpList, std::list<AccessCtrlGroupInfo> & groupInfoList);
        void PrintTargetModeOperations(list<TargetModeOperation> & tmList);
        void InvokeTargetModeOperations(list<TargetModeOperation> & tmList);
        bool isInstalled();

        bool checkBlockSizeMatch(string LunName, int blockSize, int & oldBlockSize);
        bool checkLunNBlksMatch(string LunName, unsigned long long nBlks, unsigned long long & oldNBlks);
        bool CheckATLun(string atLunName, unsigned long long lunSize, int blockSize, unsigned long long& oldLunSize, int& oldBlockSize);
        int CreateATLun(string atLunName, unsigned long long lunSize, int blockSize, unsigned long long startingPhysicalOffset);
        int ExportATLun (string atLunName, unsigned long long lunSize, unsigned long long startingPhysicalOffset, int blockSize, AcgInfoList acgList, int enablePassthrough, string PTPath);
        bool DeleteATLun(string atLunName);
        int RemoveATLun (string atLunName, unsigned long long lunSize, int blockSize, AcgInfoList acgList);
        int ExportScstLun (string atLunName, unsigned long long atLunNo, string groupName, string wwn);
        int UnexportScstLun (string atLunName, unsigned long long atLunNo, string groupName, string wwn);
        int AddPTPath(string atLunName, string PTPath);

        bool IsAtLunPresent(const std::string atLunName);
        bool ClearDiffsOnATLun(const std::string atLunName);
        bool GetATLunAttr(const std::string atLunName, const std::string attrName, std::string & attrValue);
        bool SetATLunAttr(const std::string atLunName, const std::string attrName, std::string attrValue);

        int start_daemon();
        int run_startup();

        void ProcessMessageLoop();
        void onFailoverCxSignal(ReachableCx r);
        void RegisterHostIfNeeded();
        void ProcessAtOperationsIfRequired();
        void ReportInitiatorsIfRequired();
        void CleanUp();

    public:
        int quit;

    private:
        //
        // Cached state from CX and registry
        //
        bool m_bInitialized;

        //SwitchRpcConfigurator* m_configurator;    
        RpcConfigurator* m_configurator;    
        bool m_bRemoteCxReachable;

        ACE_Sig_Handler sig_handler_;
};

#endif /* FABRICAGENT_H */


