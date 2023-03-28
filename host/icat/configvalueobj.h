#ifndef CONFIG_VALUE_OBJ_H
#define CONFIG_VALUE_OBJ_H

class ConfigValueObject ;
#include "parser.h"
#include "icatexception.h"
#include <boost/shared_ptr.hpp>
#include <stdio.h>
#include <iostream>
#include <string>
#include <ace/Token.h>
#include <ace/Get_Opt.h>

#include<list>

using namespace std;

  
typedef boost::shared_ptr<ConfigValueObject> ConfigValueObjptr ;

//------- ConfigValueKeys Structures -------

struct RemoteOffice
{
    std::string m_szServerName;
	std::string m_szBranchName;
	std::string m_szSourceVolume;
    RemoteOffice()
    {
        m_szServerName = "";
        m_szBranchName = "";
        m_szSourceVolume = "";
    }
};
  
struct Hcap
{
	std::string m_szTransport;
    std::string m_szDNSname;
	std::string m_szUID; 
	std::string m_szGID;
    Hcap()
    {
        m_szTransport = "http";
        m_szDNSname = "";
        m_szUID = "0";
        m_szGID = "0";
    }
};

struct Nodes
{ 
    std::string m_szIp; 
    int m_Port; 
    Nodes()
    {
        m_szIp = "";
        m_Port = 80;
    }
};

struct ContentSource
{
    std::string m_szDirectoryName;
    std::string m_szExcludeList;
    std::list<std::string> m_szfilterTemplate;
    std::string m_szFilePattern;
    std::string m_szFilePatternOps;
    bool m_Include;
    std::string m_szFilter1;
    std::string m_szMtime;
    std::string m_szMtimeOps;
    std::string m_szFilter2;
    unsigned long long m_Size;
    std::string m_SizeOps;
    ContentSource()
    {
        m_szDirectoryName = "";
        m_szExcludeList = "";
        m_szfilterTemplate.clear();
        m_szFilePattern = "*.*";
        m_szFilePatternOps = "=";
        m_Include = true;
        m_szFilter1 = "and";
        m_szMtime = "";
        m_szMtimeOps = ">=";
        m_szFilter2 = "and";
        m_Size = 0;
        m_SizeOps = ">=";
    }
};

struct Tunables
{
    bool m_RetryOnFailure;
    int m_RetryLimit;
    int m_RetryInterval;
    bool m_ExitOnRetryExpiry; 
    int m_MaxConnects;
    int m_TCPrecvbuffer;
    int m_TCPsendbuffer;
    int m_LowspeedLimit;
    int m_LowspeedTime;
    std::string m_ArchiveRootDir;
    int m_ConnectionTimeout;
    Tunables()
    {
        m_RetryOnFailure = 1;
        m_RetryLimit = 1;           
        m_RetryInterval = 1000;        
        m_ExitOnRetryExpiry = 1;
        m_MaxConnects = 10;
        m_TCPrecvbuffer = 0;       
        m_TCPsendbuffer = 0;
        m_LowspeedLimit = 1;
        m_LowspeedTime = 180;
        m_ArchiveRootDir = "";
        m_ConnectionTimeout = 45;
    }
};

struct Config
{
	bool m_Resume;
    bool m_FromLastRun; 
    bool m_Forcerun;
    bool m_OverWrite;
    bool m_AutoGenDestDirectory;
    std::string m_szTargetDirectory;
    std::string m_LogFilePath;
    int m_logLevel;
	int m_maxfilelisters;
	int m_generatefilelist;
	std::string m_szfilelisttarget;
	std::string m_szfilelistsource;
	int m_ReadFromFile;
    Config()
    {
        m_Resume = 0;         
        m_FromLastRun = 0;
        m_Forcerun =1;
        m_OverWrite = 1;
        m_AutoGenDestDirectory = 1;
        m_szTargetDirectory = "" ;    
        m_LogFilePath= "";
        m_logLevel = 3;
		m_maxfilelisters = 1;
		m_generatefilelist = 0;
		m_ReadFromFile = 0;
    }
};

struct Delete
{
    int m_MaxLifeTime ;
    int m_MaxCopies ;
    Delete()
    {
        m_MaxLifeTime = 30;
        m_MaxCopies = 5; 
    }
};

// ----------------ConfigValueObject(CVO)------------
class ConfigValueObject  
{
    private:
        static ConfigValueObject *m_cvo_obj; 
        mutable ACE_Configuration_Heap m_heap;
    private:
        ConfigValueObject();

    public:
        time_t m_startTime ;
        RemoteOffice m_remoteOffice;
        Hcap m_hcap;
        std::list<Nodes> m_nodes;
        std::list<ContentSource> m_content_source;
        Tunables m_tunables;
        Config m_config;
        Delete m_delete;
 
    public:
        ~ConfigValueObject();
 // -------------get functions-------------
        static void createInstance(int argc, ACE_TCHAR** argv);
        static ConfigValueObject* getInstance() ;

        void dumper();
        time_t getStartTime(); 
    	std::string getServerName() const;
    	std::string getBranchName() const;
    	std::string getSourceVolume() const;
   
    	std::string getTransport() const;
        std::string getDNSname() const;
        std::string getUID() const;
        std::string getGID() const;

        list<ContentSource>& getContentSrcList() ;
        list<Nodes>& getNodeList() ;
	
    	bool getRetryOnFailure() const;
    	int getRetryLimit() const;
    	int getRetryInterval() const;
    	bool getExitOnRetryExpiry() const;
	    int getMaxConnects() const;
        int getTCPrecvbuffer() const;
        int getTCPsendbuffer() const;
        int getLowspeedLimit() const;
        int getLowspeedTime() const;
        std::string getArchiveRootDirectory() const;
        int getConnectionTimeout() const;

	    bool getResume() const;
        bool getFromLastRun() const;
        bool getForceRun() const;
        bool getOverWrite() const;
	    bool getAutoGenDestDirectory() const;
	    std::string getTargetDirectory() const;
	    std::string getLogFilePath() const;
		std::string  getFilelistToTarget() const;
		std::string  getFilelistFromSource() const;
        int getLogLevel() const;
		int getmaxfilelisters() const;
		int getgeneratefilelist() const;
        int getMaxLifeTime() const;
        int getMaxCopies() const;
        


 //------------ set functions---------------

        void setServerName(std::string const& value);
    	void setBranchName(std::string const& value);
    	void setSourceVolume(std::string const& value);
    
        void setTransport(std::string const& value);
	    void setNodes(std::string const& value);
        void setDNSname(std::string const& value);  
        void setUID(std::string const& value);  
        void setGID(std::string const& value);  

        void setRetryOnFailure(bool const& value);
	    void setRetryLimit(int const& value);
	    void setRetryInterval(int const& value);
	    void setExitOnRetryExpiry(bool const& value);   
	    void setMaxConnects(int const& value);
        void setTCPrecvbuffer(int const& value);
        void setTCPsendbuffer(int const& value);
	    void setLowspeedLimt(int const& value) ;
        void setLowspeedTime(int const& value) ;
        void setArchiveRootDirectory(std::string const& value) ;
        void setConnectionTimeout(int const& value) ;


        void setResume(bool const& value);
        void setFromLastRun(bool const& value);
        void setForceRun(bool const& value);
        void setOverWrite(bool const& value);
	    void setAutoGenDestDirectory(bool const& value);
	    void setTargetDirectory(std::string const& value);
        void setLogFilePath(std::string const& value);
        void setLogLevel(int const& value);
	    void setmaxfilelisters(int const& value);
		void setgeneratefilelist(int const& value);
		void setFilelistToTarget(std::string const& value);
        void setMaxLifeTime(int const& value);
        void setMaxCopies(int const& value);
		void setFilelistFromSource(std::string const& value);
        void addContentSource(ContentSource src);
        void addNode(Nodes node);
       
};

#endif
