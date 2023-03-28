#ifndef APP_CONFIG_VALUE_OBJ_H
#define APP_CONFIG_VALUE_OBJ_H

class AppConfigValueObject ;
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <string>
#include <ace/Token.h>
#include <ace/Get_Opt.h>
#include "appexception.h"
#include "svtypes.h"
#include "portable.h"
#include "logger.h"
#include<list>
#include <map>

struct VacpOptions
{
	std::string m_ApplicationType;
	std::list<std::string> m_AppVolumesList;
	std::list<std::string> m_CustomVolumesList;
	std::string m_VolumeGUID;
	std::string m_TagName;
	std::string m_WriterInstance;
	bool m_CrashConsistency;
	bool m_SkipDriverCheck;
	bool m_PerformFullBackup;
	std::string m_ProviderGUID;
	bool m_SynchronousTag;
	SV_ULONG m_Timeout;
	bool m_RemoteTag;
	std::string m_ServerDevice;
	std::string m_IPAddress;
	SV_ULONG m_port;
	std::string m_Context;
	std::string m_VacpCommand;
    VacpOptions()
    {
	    m_CrashConsistency = 0;
	    m_SkipDriverCheck = 0;
	    m_PerformFullBackup = 0;
        m_Timeout = 10;
	    m_RemoteTag = 0;
	    m_port = 80;
        m_SynchronousTag = 0;
    }
};

struct RecoveryOptions
{
	std::string m_VolumeName;
	std::string m_RecoveryPointType;
	std::string m_TagName;
	std::string m_TimeStamp;
    std::string m_TagType;
};

struct RecoveryPoint
{
	std::string m_LatestCommonTime;
	std::list<std::string>m_AppVolumesList;
	std::list<std::string>m_CustomVolumesList;
    std::map<std::string,RecoveryOptions> m_RecoveryOptions;
    std::map<std::string,std::string> m_CustomProctedPairs;
};

struct SQLInstanceNames
{
    std::list<std::string> m_InstanceNameList;
};

struct PlanPairStatus
{
	std::map<std::string, std::string> m_Pairs;
};
typedef boost::shared_ptr<AppConfigValueObject> AppConfigValueObjectPtr;

class AppConfigValueObject  
{
    private:
        static AppConfigValueObjectPtr m_cvo_obj; 
        AppConfigValueObject();

    public:
        VacpOptions m_VacpOptions;
        RecoveryPoint m_RecoveryPoint;       
        SQLInstanceNames m_SQLInstanceNames;
		PlanPairStatus m_PairStatus;
        ~AppConfigValueObject();

        static void createInstance();
        static void ParseConfFile(std::string cmdLine);
        static AppConfigValueObjectPtr getInstance() ;
        void UpdateConfFile(std::string filePath);
        void ClearLists();
        std::string getApplicationType() const;
        std::list<std::string> getApplicationVolumeList() const;
        std::list<std::string> getCustomVolumesList() const;
        std::string getVolumeGUID() const;
        std::string getTagName() const;
        std::string getWriterInstance() const;
        bool getCrashConsistency() const;
        bool getSkipDriverCheck() const;
        bool getPerformFullBackup() const;
        std::string getProviderGUID() const;
        bool getSynchronousTag() const;
        SV_ULONG getTimeout() const;
        bool getRemoteTag() const;
        std::string getServerDevice() const;
        std::string getIPAddress() const;
        SV_ULONG getport() const;
        std::string getContext() const;
        std::string getVacpCommand() const;
        std::string getLatestCommonTime() const;
        std::map<std::string,RecoveryOptions> getRecoveryOptions() const;
        std::list<std::string> getSQLInstanceNameList() const;
        std::map<std::string,std::string> getCustomProtectedPair() const;
		std::map<std::string, std::string> getPairStatus() const;

        void setApplicationType(std::string const& value);
        void setApplicationVolume(std::string const& value);
        void setCustomVolume(std::string const& value);
        void setVolumeGUID(std::string const& value);
        void setTagName(std::string const& value);
        void setWriterInstance(std::string const& value);
        void setCrashConsistency(bool const& value);
        void setSkipDriverCheck(bool const& value);
        void setPerformFullBackup(bool const& value);
        void setProviderGUID(std::string const& value);
        void setTimeout(SV_ULONG  const& value);
        void setRemoteTag(bool const& value);
        void setServerDevice(std::string const& value);
        void setIPAddress(std::string const& value);
        void setport(SV_ULONG const& value);
        void setContext(std::string const& value);
        void setVacpCommand(std::string const& value);
        void setSynchronousTag(bool const& value);
        void setLatestCommonTime(bool const& value);
        void setRecoveryOptions(RecoveryOptions const& options);
        void setSQLInstanceNameList(std::string const& str);
        void setCustomProtectedPair(std::string const& srcvol,std::string const& tgtvol);
		void setPairStatus(std::string const& volName, std::string const& repStatus);
};

#endif