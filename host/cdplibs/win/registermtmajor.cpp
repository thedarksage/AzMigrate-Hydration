#include "cdpcli.h"
#include "registermt.h"
#include "registermtmajor.h"

#include "defaultdirs.h"
#include "securityutils.h"

#include "localconfigurator.h"
#include "talwrapper.h"
#include "configuratorrpc.h"
#include "unmarshal.h"
#include "serializevolumegroupsettings.h"
#include "createpaths.h"
#include "converterrortostringminor.h"
#include "importpfxcert.h"
#include "getpassphrase.h"

#include <sstream>

namespace MTRegistration
{
	bool registerMT()
	{
		bool rv = true;
		try
		{
			std::string certLocation = securitylib::getMTCertPath();
			std::string hiveLocation = securitylib::getMTHivePath();
			
			DebugPrintf(SV_LOG_INFO, "fetching certificate from Configuration server ...\n");
			FetchMTRegistrationDetailsFromCS(certLocation, hiveLocation);
			DebugPrintf(SV_LOG_INFO, "importing registry keys ...\n");
			importMTRegistryHive(hiveLocation);
			DebugPrintf(SV_LOG_INFO, "importing certificate ...\n");
			rv = securitylib::importPfxCert(certLocation, securitylib::getPassphrase());
			DebugPrintf(SV_LOG_INFO, "Registration suceeded.\n");
		}
		catch (ContextualException& ce)
		{
			rv = false;
			std::cerr << FUNCTION_NAME << " encountered exception " << ce.what() << "\n";
		}
		catch (std::exception const& e)
		{
			rv = false;
			std::cerr << FUNCTION_NAME << " encountered exception " << e.what() << "\n";
		}
		catch (...)
		{
			rv = false;
			std::cerr << FUNCTION_NAME << " encountered unknown exception.\n";
		}

		return rv;
	}



	void FetchMTRegistrationDetailsFromCS(const std::string & certPath, const std::string & hivePath)
	{
		LocalConfigurator lc;
		std::string m_hostid = lc.getHostId();
		TalWrapper m_tal;

        std::string biosId("");
        GetBiosId(biosId);
        std::string m_dpc =  marshalCxCall("::getVxAgent", m_hostid, biosId);

		std::string debug = m_tal(marshalCxCall(m_dpc, "getMtregistrationDetails"));
		DebugPrintf(SV_LOG_DEBUG, "%s serialized data from cs: %s\n", FUNCTION_NAME, debug.c_str()) ;
		MTRegistrationDetails mtRegistrationDetails = unmarshal<MTRegistrationDetails>(debug);

		std::string certContent = securitylib::base64Decode(mtRegistrationDetails.certData.c_str(), 
			mtRegistrationDetails.certData.size());

		std::string HiveContent = securitylib::base64Decode(mtRegistrationDetails.registryData.c_str(),
			mtRegistrationDetails.registryData.size());

		writeMTRegistrationContent(certPath, certContent);
		writeMTRegistrationContent(hivePath, HiveContent);
	}

	void writeMTRegistrationContent(const std::string & location, const std::string & content)
	{

		std::string reply;
		securitylib::backup(location);
		CreatePaths::createPathsAsNeeded(location);
		std::ofstream outFile(location.c_str(), std::ios::binary | std::ios::trunc);
		if (!outFile.good()) {
			reply += "error opening file ";
			reply += location;
			reply += ": ";
			reply += boost::lexical_cast<std::string>(errno);
			throw ERROR_EXCEPTION << reply;
		}

		outFile << content;
		outFile.close();

		// FIXME: enable once IIS is figured out
		//securitylib::setPermissions(certificateFileName, securitylib::SET_PERMISSION_BOTH);
		
		std::ifstream inFile(location.c_str(), std::ios::binary);
		if (!inFile.good()) {
			reply += "error opening file ";
			reply += location;
			reply += " to verify contents: ";
			reply += boost::lexical_cast<std::string>(errno);
			throw ERROR_EXCEPTION << reply;
		}
		
		inFile.seekg(0, inFile.end);
		std::streampos length = inFile.tellg();
		inFile.seekg(0, inFile.beg);
		std::vector<char> verifyContent((size_t)length);
		inFile.read(&verifyContent[0], length);
		if (content.length() == length && (0 == memcmp(content.c_str(),&verifyContent[0],length))) {		
			reply += "successfully wrote  ";
			reply += location;
			DebugPrintf(SV_LOG_DEBUG, "%s\n", reply.c_str());
		}
		else{

			reply += "error writing ";
			reply += location;
			throw ERROR_EXCEPTION << reply;
		}
	}

	bool SetPrivilege(LPCSTR privilege, bool set)
	{
		DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
		bool rv = true;

		TOKEN_PRIVILEGES tp;
		HANDLE hToken;
		LUID luid;


		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		{
			std::string err;
			convertErrorToString(GetLastError(), err);
			throw ERROR_EXCEPTION << "open process token failed with error: " << err;
		}


		ON_BLOCK_EXIT(&CloseHandle, hToken);
		if (!LookupPrivilegeValue(NULL, privilege, &luid))
		{
			std::string err;
			convertErrorToString(GetLastError(), err);
			throw ERROR_EXCEPTION << "lookup process privilege failed with error: " << err;
		}

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;

		if (set)
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else
			tp.Privileges[0].Attributes = 0;

		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
		if (GetLastError() != ERROR_SUCCESS)
		{
			std::string err;
			convertErrorToString(GetLastError(), err);
			throw ERROR_EXCEPTION << "set process privilege failed with error: " << err;
		}


		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return rv;
	}


	void importMTRegistryHive(const std::string & hivePath)
	{
		bool	rv = true;
		DWORD	d;
		HKEY	hhKey;


		SetPrivilege(SE_RESTORE_NAME, true);
		SetPrivilege(SE_BACKUP_NAME, true);

		const std::string MTRegPath = "SOFTWARE\\Microsoft\\Windows Azure Backup\\Config\\InMageMT";
		d = RegCreateKeyEx(HKEY_LOCAL_MACHINE, MTRegPath.c_str(), 0, NULL, REG_OPTION_BACKUP_RESTORE,
			KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &hhKey, NULL);
		
		if(d != ERROR_SUCCESS)  {
			std::string err;
			convertErrorToString(d, err);
			throw ERROR_EXCEPTION << "registry key creation failed with error: " << err << "(" << d << ")";
		}

		ON_BLOCK_EXIT(&RegCloseKey, hhKey);
		d = RegRestoreKey(hhKey, hivePath.c_str(), REG_FORCE_RESTORE);
		if (d != ERROR_SUCCESS) {
			std::string err;
			convertErrorToString(d, err);
			throw ERROR_EXCEPTION << "registry import failed with error: " << err << "(" << d << ")";
		}

		// in case of exception, priveleges will not be reset
		// this is ok for now, as process is going to exit on exception
		SetPrivilege(SE_RESTORE_NAME, false);
		SetPrivilege(SE_BACKUP_NAME, false);
	}
}
