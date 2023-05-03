#include "configvalueobj.h"

#include "logger.h"
#include "defs.h"
#include "common.h"

ConfigValueObject* ConfigValueObject::m_cvo_obj = NULL; 

/*
* Function Name: Constuctor (Special member function)
* Arguements:  default constructor.
* Return Value: No return type.
* Description: 
* Called by: 
* Calls: No calls.
* Issues Fixed:
*/

ConfigValueObject::ConfigValueObject()
{
   m_startTime = time(NULL);    
}

ConfigValueObject::~ConfigValueObject()    
{
  
}
/*
* Function Name: createInstance     
* Arguements:  1st --> no of cmd line args, 2nd --> cmdline arg values.       
* Return Value: Nothing
* Description: This will call parser component to parse given input..
* Called by: main function for only once.
* Calls: parseInput() of parser comp
* Issues Fixed:
*/


void ConfigValueObject::createInstance(int count, ACE_TCHAR** cmdLine)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    char tempString[1000];
    if(m_cvo_obj == NULL && count != 0)
    {
            DebugPrintf(SV_LOG_DEBUG, "config value currently doesnt exist. creating it for the first time\n") ;
            ConfigValueParser cvp_obj(count,cmdLine);
            try
            {
                m_cvo_obj = new (nothrow) ConfigValueObject();
                if( m_cvo_obj == NULL )
                    throw icatException("failed to allocate memory to cvo object");
                cvp_obj.parseInput(*m_cvo_obj);    
            }
            catch(InvalidInput inv)
            {
                DebugPrintf(SV_LOG_ERROR, "%s \n",inv.getInvalidInput().c_str()) ;
				inm_sprintf_s(tempString, ARRAYSIZE(tempString), "%s", inv.getInvalidInput().c_str());
                throw icatException(tempString);
            }
            catch(ParseException ipe)
            {   
                DebugPrintf(SV_LOG_ERROR, "%s \n",ipe.what()) ;
				inm_sprintf_s(tempString, ARRAYSIZE(tempString), "%s", ipe.what());
                throw icatException(tempString);
            }
    }
    else
    {
            DebugPrintf(SV_LOG_DEBUG, "ConvigValueObject is already existed..");                         
    }
        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
   

}

/*
* Function Name: getInstance     
* Arguements:  no args ..
* Return Value: return the configValue object's address
* Description: This is static function , will return the configValueObj.
* Called by: From Any Where.
* Calls: No calls
* Issues Fixed:
*/

ConfigValueObject* ConfigValueObject::getInstance()
{
    if(m_cvo_obj == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, "ConfigValueObj is not yet created.... returning null") ;
    }
    return m_cvo_obj;

}
/*
* Function Name: dumper     
* Arguements:  no args ..
* Return Value: Nothing..
* Description: This will dumps the CVO value's . This is only for debugging purpose.
* Called by: 
* Calls: No calls
* Issues Fixed:
*/
void ConfigValueObject::dumper()
{
    cout<<"\n =======remoteoffice=======\n";
    cout<<"\n key server Name \t value \t"<<getServerName();
    cout<<"\n key branch Name \t value \t"<<getBranchName();
    cout<<"\n key Source Volume \t value \t"<<getSourceVolume();
    
    cout<<"\n\n =======ArchiveRepository=======\n";
    cout<<"\n key transport  \t value \t"<<getTransport();

    list<Nodes> nodesList =  getNodeList();
    for(list<Nodes>::iterator it=nodesList.begin(); it!=nodesList.end(); it++)
	{
        cout<<"\n key Ip  \t value \t"<<it->m_szIp;
        cout<<"\n key port  \t value \t"<<it->m_Port;
    }
    cout<<"\n key DNS Name \t value \t"<<getDNSname();
    cout<<"\n key RootDir \t value \t"<<getArchiveRootDirectory();
    cout<<"\n key  uid \t value \t"<<getUID();
    cout<<"\n key gid \t value \t"<<getGID();

    cout<<"\n\n =======content.source======= \n";
    list<ContentSource> contentList =  getContentSrcList();
    std::list<std::string> filterTemplateList;
    for(list<ContentSource>::iterator it=contentList.begin(); it!=contentList.end(); it++)
	{
        cout<<"\n key DirectoryName  \t value \t"<<it->m_szDirectoryName;
        cout<<"\n key ExcludeList  \t value \t"<<it->m_szExcludeList;
        cout<<"\n key Include  \t value \t"<<it->m_Include;
        cout<<"\n key FilePattern  \t value \t"<<it->m_szFilePattern;
        cout<<"\n FilePatternOps  \t value \t"<<it->m_szFilePatternOps;
        cout<<"\n key  Filter1 \t value \t"<<it->m_szFilter1;
        cout<<"\n key Mtime  \t value \t"<<it->m_szMtime;
        cout<<"\n MtimeOps  \t value \t"<<it->m_szMtimeOps;
        cout<<"\n key Filter2  \t value \t"<<it->m_szFilter2;
        cout<<"\n key  size \t value \t"<<it->m_Size;
        cout<<"\n SizeOps \t value \t"<<it->m_SizeOps<<"\n";
        filterTemplateList = it->m_szfilterTemplate;
        for(list<string>::iterator it=filterTemplateList.begin(); it!=filterTemplateList.end(); it++)
        {
            cout<<" "<<*it<<"\t";
        }
   }
    cout<<"\n\n =======tunnables=======\n ";
    cout<<"\n key RetryOnFailure \t value \t"<<getRetryOnFailure();
    cout<<"\n key RetryLimit \t value \t"<<getRetryLimit();
    cout<<"\n key RetryInterval \t value \t"<<getRetryInterval();
    cout<<"\n key ExitOnRetryExpiry \t value \t"<<getExitOnRetryExpiry();
    cout<<"\n key Maxconnects \t value \t"<<getMaxConnects();
    cout<<"\n key TCPrecvbuffer \t value \t"<<getTCPrecvbuffer();
    cout<<"\n key TCPsendbuffer \t value \t"<<getTCPsendbuffer();
    cout<<"\n key LowSpeedLimt \t value \t"<<getLowspeedLimit();
    cout<<"\n key LowSpeedTime \t value \t"<<getLowspeedTime();
    cout<<"\n key ConnectionTimeout \t value \t"<<getConnectionTimeout();
    
    cout<<"\n\n =======config=======\n ";
    cout<<"\n key Resume \t value \t"<<getResume();
    cout<<"\n key FromLastRun \t value \t"<<getFromLastRun();
    cout<<"\n key ForceRun \t value \t"<<getForceRun();
    cout<<"\n key OverWrite \t value \t"<<getOverWrite();
    cout<<"\n key AutoGenDestDirectory \t value \t"<<getAutoGenDestDirectory();
    cout<<"\n key TargetDirectory \t value \t"<<getTargetDirectory();
    cout<<"\n key LogFilePath \t value \t"<<getLogFilePath();
    cout<<"\n key LogLevel \t value \t"<<getLogLevel(); 
    
    cout<<"\n\n =======Delete=======\n ";
    cout<<"\n key MaxLifeTime \t value \t"<<getMaxLifeTime();
    cout<<"\n key MaxCopies \t value \t"<<getMaxCopies();
}
time_t ConfigValueObject::getStartTime() 
{
    return m_startTime ;
}

std::string ConfigValueObject::getServerName() const
{ 
	return m_remoteOffice.m_szServerName;
}

std::string ConfigValueObject::getBranchName() const
{
	return m_remoteOffice.m_szBranchName;
}

std::string ConfigValueObject::getSourceVolume() const
{
    return m_remoteOffice.m_szSourceVolume;
}

std::string ConfigValueObject::getTransport() const
{
	return m_hcap.m_szTransport;
}

std::string ConfigValueObject::getDNSname()const
{
	return m_hcap.m_szDNSname;
}
std::string ConfigValueObject::getUID()const
{
	return m_hcap.m_szUID;
}
std::string ConfigValueObject::getGID()const
{
	return m_hcap.m_szGID;
}

list<Nodes>& ConfigValueObject::getNodeList()
{
    return m_nodes;
}

list<ContentSource>& ConfigValueObject::getContentSrcList() 
{
    return m_content_source;
}


bool ConfigValueObject::getRetryOnFailure() const
{
    	return m_tunables.m_RetryOnFailure;
} 

int ConfigValueObject::getRetryLimit()const 
{
	return m_tunables.m_RetryLimit;
} 

int ConfigValueObject::getRetryInterval()const
{
	DebugPrintf(SV_LOG_DEBUG, "RetryInterval is %d \n",m_tunables.m_RetryInterval) ;
	return m_tunables.m_RetryInterval;  
} 

bool ConfigValueObject::getExitOnRetryExpiry()const
{
	return m_tunables.m_ExitOnRetryExpiry;  
} 

int ConfigValueObject::getMaxConnects()const
{
    	return m_tunables.m_MaxConnects; 	
} 

int ConfigValueObject::getTCPrecvbuffer()const
{
	return m_tunables.m_TCPrecvbuffer;  
}

int ConfigValueObject::getTCPsendbuffer()const
{
	return m_tunables.m_TCPsendbuffer;  
}

int ConfigValueObject::getLowspeedLimit()const
{
	return m_tunables.m_LowspeedLimit;  
}

int ConfigValueObject::getLowspeedTime()const
{
	return m_tunables.m_LowspeedTime;  
}

std::string ConfigValueObject::getArchiveRootDirectory()const
{
	return m_tunables.m_ArchiveRootDir; 
}

int ConfigValueObject::getConnectionTimeout() const
{
	return m_tunables.m_ConnectionTimeout;     
}

bool ConfigValueObject::getResume()const
{
	return m_config.m_Resume; 
} 

std::string ConfigValueObject::getFilelistFromSource() const
{
	 return m_config.m_szfilelistsource;
}
bool ConfigValueObject::getFromLastRun()const
{
	return m_config.m_FromLastRun; 
}    

bool ConfigValueObject::getForceRun()const
{
	return m_config.m_Forcerun; 
} 

bool ConfigValueObject::getOverWrite()const
{
	return m_config.m_OverWrite; 
} 

bool ConfigValueObject::getAutoGenDestDirectory()const
{
	return m_config.m_AutoGenDestDirectory; 
} 

std::string ConfigValueObject::getTargetDirectory()const
{
	return m_config.m_szTargetDirectory; 
} 

std::string ConfigValueObject::getLogFilePath()const
{ 
	return m_config.m_LogFilePath; 
}

int ConfigValueObject::getLogLevel()const
{ 
	return m_config.m_logLevel; 
}
int ConfigValueObject::getmaxfilelisters()const
{ 
	return m_config.m_maxfilelisters; 
} 
int ConfigValueObject::getgeneratefilelist()const
{ 
	return m_config.m_generatefilelist; 
} 
std::string ConfigValueObject::getFilelistToTarget() const
{
	return m_config.m_szfilelisttarget;
}
int ConfigValueObject::getMaxLifeTime() const
{
	return m_delete.m_MaxLifeTime;     
}
int ConfigValueObject::getMaxCopies() const
{
	return m_delete.m_MaxCopies; 
}

void ConfigValueObject::setServerName(std::string const& value)
{
    m_remoteOffice.m_szServerName = value;
}

void ConfigValueObject::setBranchName(std::string const& value) 
{
	m_remoteOffice.m_szBranchName = value;
}

void ConfigValueObject::setSourceVolume(std::string const& value) 
{
    m_remoteOffice.m_szSourceVolume = value;
}
void ConfigValueObject::setFilelistFromSource(std::string const& value)
{
	 m_config.m_szfilelistsource = value;
}
void ConfigValueObject::setTransport(std::string const& value) 
{
    //we keep transport as http if a invalid transport type is got from conf file /command line option.
    if( value.compare("http") == 0 
        || value.compare("cifs") == 0 
        || value.compare("nfs") == 0 )
    {
	    m_hcap.m_szTransport = value;
    }
}

void ConfigValueObject::setNodes(std::string const& value)
{
    Nodes tempNode;
    std::string subIpList;
   
    std::string::size_type index1 = 0 ;
    std::string::size_type index2 = std::string::npos ;
    std::string::size_type index3 = std::string::npos ;

    do
    {
       index1 = index2 + 1 ;
       index2 = value.find( ',', index1 ) ;
       if( index2 == std::string::npos )
       {
           index2 = value.length() ;
       }
       
       subIpList = value.substr( index1, index2 - index1 ) ;
       index3 = subIpList.find( ':' ) ;
       if( index3 != std::string::npos )
       {
           tempNode.m_szIp = subIpList.substr( 0, index3 );
           std::string temp = subIpList.substr( index3 + 1, subIpList.length() - ( index3 + 1 ) );
           tempNode.m_Port = atoi( temp.c_str() ) ;                          
       }
       else
       {
           tempNode.m_szIp = subIpList;
           tempNode.m_Port = 80;   
       } 
       addNode(tempNode);
    }while( index2 != value.length() );  

}
void ConfigValueObject::setDNSname(std::string const& value) 
{
	m_hcap.m_szDNSname = value;
}

void ConfigValueObject::setUID(std::string const& value) 
{
	m_hcap.m_szUID = value;
}

void ConfigValueObject::setGID(std::string const& value) 
{
	m_hcap.m_szGID = value;
}

void ConfigValueObject::setRetryOnFailure(bool const& value) 
{
	m_tunables.m_RetryOnFailure = value;
} 

void ConfigValueObject::setRetryLimit(int const& value) 
{
	m_tunables.m_RetryLimit = value;
} 

void ConfigValueObject::setRetryInterval(int const& value) 
{
	m_tunables.m_RetryInterval = value;
} 


void ConfigValueObject::setExitOnRetryExpiry(bool const& value) 
{
    m_tunables.m_ExitOnRetryExpiry = value;
} 

void ConfigValueObject::setMaxConnects(int const& value) 
{
    m_tunables.m_MaxConnects = value;
} 

void ConfigValueObject::setTCPrecvbuffer(int const& value) 
{
    m_tunables.m_TCPrecvbuffer = value;
} 

void ConfigValueObject::setTCPsendbuffer(int const& value) 
{
    m_tunables.m_TCPsendbuffer = value;
} 

void ConfigValueObject::setLowspeedLimt(int const& value)
{
    m_tunables.m_LowspeedLimit= value;
}

void ConfigValueObject::setLowspeedTime(int const& value)
{
    m_tunables.m_LowspeedTime= value;
}

void ConfigValueObject::setArchiveRootDirectory(std::string const& value)
{
    m_tunables.m_ArchiveRootDir = value;
}
void ConfigValueObject::setConnectionTimeout(int const& value)
{
    m_tunables.m_ConnectionTimeout= value; 
}

void ConfigValueObject::setResume(bool const& value) 
{
    m_config.m_Resume = value;
}

void ConfigValueObject::setFromLastRun(bool const& value)
{
    m_config.m_FromLastRun = value;
}   

void ConfigValueObject::setForceRun(bool const& value) 
{
    m_config.m_Forcerun = value;
}

void ConfigValueObject::setOverWrite(bool const& value) 
{
    m_config.m_OverWrite = value;
} 

void ConfigValueObject::setAutoGenDestDirectory(bool const& value) 
{
    m_config.m_AutoGenDestDirectory = value; 
} 

void ConfigValueObject::setTargetDirectory(std::string const& value) 
{
    m_config.m_szTargetDirectory = value;
}
void ConfigValueObject::setLogFilePath(std::string const& value) 
{
    m_config.m_LogFilePath = value;
} 

void ConfigValueObject::setLogLevel(int const& value) 
{
    m_config.m_logLevel = value;
}
void ConfigValueObject::setmaxfilelisters(int const& value) 
{
    m_config.m_maxfilelisters = value;
}
void ConfigValueObject::setgeneratefilelist(int const& value) 
{
    m_config.m_generatefilelist = value;
}
void ConfigValueObject::setFilelistToTarget(std::string const& value)
{
	 m_config.m_szfilelisttarget = value;
}
void ConfigValueObject::setMaxLifeTime(int const& value)
{
    m_delete.m_MaxLifeTime= value; 
}
void ConfigValueObject::setMaxCopies(int const& value)
{
    m_delete.m_MaxCopies= value; 
}

void ConfigValueObject::addContentSource(ContentSource src)
{
	m_content_source.push_back(src);
}

void ConfigValueObject::addNode(Nodes node)
{
	m_nodes.push_back(node);
}
