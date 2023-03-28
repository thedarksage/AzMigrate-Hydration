// @file portable.h: defines OS portability layer for SV code
//
//
#ifndef PORTABLE__H
#define PORTABLE__H

#include "svtypes.h"

//
// Calling conventions
//
#ifndef WIN32
#define SV_CDECL
#else
#define SV_CDECL __cdecl
#endif

#ifdef SV_UNIX
#define stricmp(x,y) strcasecmp(x,y)
#define strcmpi strcasecmp
#define strnicmp strncasecmp  
#endif

#ifdef WIN32
#define NEW_LINE_CHARACTER "\r\n"
#else
#ifdef SV_UNIX
#define NEW_LINE_CHARACTER "\n"
#endif
#endif

//
// Coding decoration
//
#ifndef OPTIONAL
#define OPTIONAL
#endif

//
// Constants
//

int const SV_MAX_PATH                           = 1024;
int const SV_INTERNET_MAX_HOST_NAME_LENGTH      = 256;
int const SV_INTERNET_MAX_USER_NAME_LENGTH      = 128;
int const SV_INTERNET_MAX_PASSWORD_LENGTH       = 128;
int const SV_INTERNET_MAX_PORT_NUMBER_LENGTH    = 5;
int const SV_INTERNET_MAX_PORT_NUMBER_VALUE     = 65535;
int const SV_INTERNET_MAX_PATH_LENGTH           = 2048;
int const SV_INTERNET_MAX_SCHEME_LENGTH         = 32;          // longest protocol name length
int const SV_INTERNET_MAX_URL_LENGTH            = ( SV_INTERNET_MAX_SCHEME_LENGTH 
                                                    + sizeof( "://" ) 
                                                    + SV_INTERNET_MAX_PATH_LENGTH );
int const SV_INVALID_FILE_SIZE                  = 0xFFFFFFFF;

int const SV_DEFAULT_TRANSPORT_CONNECTION_TIMEOUT = 45;
int const SV_DEFAULT_TRANSPORT_RESPONSE_TIMEOUT  = 900;
int const SV_DEFAULT_TRANSPORT_LOWSPEED_TIME = 120;
int const SV_DEFAULT_TRANSPORT_LOWSPEED_LIMIT = 1;

const char FST_SCTR_OFFSET_510_VALUE = (char) 0x55;
const char FST_SCTR_OFFSET_511_VALUE = (char) 0xAA;

int const BS_JMP_BOOT_VALUE_0xEB = 0xEB;
int const BS_JMP_BOOT_VALUE_0xE9 = 0xE9;

int const FST_SCTR_OFFSET_510 = 510;
int const FST_SCTR_OFFSET_511 = 511;
//added for vol_guid length
int const VOL_GUID_LENGTH = 36;


//
// Debug utility functions
//
enum SV_LOG_LEVEL { SV_LOG_DISABLE, 
                    SV_LOG_FATAL, 
                    SV_LOG_SEVERE, 
                    SV_LOG_ERROR, 
                    SV_LOG_WARNING, 
                    SV_LOG_INFO, 
                    SV_LOG_DEBUG,                     
                    SV_LOG_ALWAYS };


enum SV_ALERT_TYPE { SV_ALERT_CRITICAL = 1, 
                     SV_ALERT_ERROR,                     
                     SV_ALERT_SIMPLE,
                     SV_ALERT_CRITICAL_WITH_SUBJECT };

enum SV_ALERT_MODULE { SV_ALERT_MODULE_RETENTION = 1, 
                       SV_ALERT_MODULE_ROLLBACK,                     
                       SV_ALERT_MODULE_RECOVERY,
                       SV_ALERT_MODULE_SNAPSHOT,
                       SV_ALERT_MODULE_DIFFERENTIALSYNC,
                       SV_ALERT_MODULE_CLUSTER};


enum Logger_Agent_Type
{
    VxAgentLogger,
    FxAgentLogger
};



void SetLoggerAgentType(Logger_Agent_Type);
Logger_Agent_Type GetLoggerAgentType();
void GetLoggingDetails( char const*);

void SV_CDECL DebugPrintf( const char* format, ... );
void SV_CDECL DebugPrintf( SV_LONG hr, const char* format, ... );
void SV_CDECL DebugPrintf( SV_LOG_LEVEL LogLevel, const char* format, ... );
void SV_CDECL DebugPrintf( SV_LOG_LEVEL LogLevel, SV_LONG hr, const char* format, ... );
/*Function for adding the Agent*/
void SV_CDECL DebugPrintf(char *,SV_LOG_LEVEL LogLevel, SV_LONG hr, const char* format, ...);
bool SendAlertToCx(SV_ALERT_TYPE AlertType, SV_ALERT_MODULE AlertingModule, const char *szAlertMessage);

//
// OS objects
//

class CDispatchLoop
{
public:
    //
    // Construction/destruction
    //
    static SVERROR Create( CDispatchLoop** ppDispatcher );
    virtual ~CDispatchLoop() {}

public:
    //
    // Dispatcher interface
    //
    virtual void initialize()                   { m_pImpl->initialize(); }
    virtual void dispatch( SV_ULONG delay )     { m_pImpl->dispatch( delay ); }
    virtual bool shouldExit()                   { return( m_pImpl->shouldExit() ); }

protected:
    CDispatchLoop* m_pImpl;
};

class CProcess
{
public:
    //
    // Construction/destruction
    //
    static SVERROR Create( char const* pszCommandLine, CProcess** ppProcess,
                           char const* pszStdoutLogfile = 0, char const* pszDirName = 0);
    virtual ~CProcess() { delete m_pImpl; }

public:
    //
    // Process interface
    //
    virtual bool hasExited()        { return( m_pImpl->hasExited() ); }
    virtual void waitForExit( SV_ULONG waitTime )   { return( m_pImpl->waitForExit( waitTime ) ); }
    virtual SVERROR terminate()     { return( m_pImpl->terminate() ); }
    virtual SVERROR getExitCode( SV_ULONG* pExitCode )   { return( m_pImpl->getExitCode( pExitCode ) ); }
    virtual SV_ULONG getId()        { return( m_pImpl->getId() ); }

protected:
    CProcess() : m_pImpl( 0 ) {}

protected:
    CProcess* m_pImpl;
};

#endif // PORTABLE__H
