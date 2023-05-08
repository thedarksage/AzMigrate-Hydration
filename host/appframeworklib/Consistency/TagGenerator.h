#ifndef TAG_GENERATOR_H
#define TAG_GENERATOR_H



#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>
#include "appglobals.h"
#include "controller.h"
#include "localconfigurator.h"
#include "util/common/util.h"

//#ifdef SV_WINDOWS
//#include "util/win32/vssutil.h"
//#endif

class TagGenerator ;
typedef boost::shared_ptr<TagGenerator> AppTagGeneratorPtr ;

class TagGenerator
{
private:
		SV_UINT m_uTimeOut;
		std::string m_strCommandToExecute;
		std::string m_stdOut;
	    LocalConfigurator m_objLocalConfigurator;
		std::string m_strStdOut;

		typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
		static ACE_Recursive_Thread_Mutex m_lock ;
		static SV_ULONGLONG m_FullBkpInterval ;
		static SV_ULONGLONG m_LastSuccessfulBkpTime ;
		static bool m_isFullbackupRequired ;
		static bool m_isFullbackPropertyChecked ;
public:
	TagGenerator()
	{
		
	}
	TagGenerator(std::string strCommand, SV_UINT timeout)
	{
		m_strCommandToExecute     += std::string(" "); //For Command separator
		m_strCommandToExecute += strCommand;
		m_uTimeOut = timeout ;
	}
//#ifdef SV_WINDOWS
//	SVERROR listVssProps(std::list<VssProviderProperties>& providersList);
//#endif  //Reverted the changes as part of the bug#19216 fix. These changes did not solve the issue and causing Bug#19320
	std::string getVacpPath();
	void sanitizeVacpCommmand(std::string &);
	void appendTimeStampToTagName(std::string &);
	void verifyVacpCommand(std::string &);
    void getTagName(const std::string &strVacpCommand, std::string& tagName);
    void getDeviceListStr(const std::string &strVacpCommand, std::string& devices);
	bool issueConsistenyTag(const std::string &,SV_ULONG &exitCode ) ;
    bool issueConsistenyTagForOracle(const std::string &, const std::string &dbName, SV_ULONG &exitCode ) ;
    bool issueConsistenyTagForDb2(const std::string &, const std::string &instance, const std::string &dbName, SV_ULONG &exitCode ) ;
	std::string stdOut() { return m_stdOut;}
	std::string getCommandStr()
	{
		return m_strCommandToExecute;
	}
} ;
bool timeInReadableFormat(std::string & local_time);
#endif
