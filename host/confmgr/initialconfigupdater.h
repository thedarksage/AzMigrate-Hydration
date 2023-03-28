#ifndef INITIALCONFIGUPDATER__H
#define INITIALCONFIGUPDATER__H
#include <Objbase.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "ace/Process_Manager.h"


//#include "confsettings.h"
#include "confpropertysettings.h"
#include "localconfigurator.h"
#include "host.h"
#include "inmuuid.h"


#define HOSTID "HostId"
#define HOST_GUID "HOST_GUID"
#define HOSTNAME "Hostname"
#define ID "id"

#define SV_SVSYSTEMS_BIN_PATH "home\\svsystems\\bin"
#define SV_CS_TRANSPORT_PATH "home\\svsystems\\transport"
#define GENPASSPHRASE_EXE "genpassphrase.exe"

#define CS_IP "CS_IP"
#define PS_IP "PS_IP"
#define PS_CS_IP "PS_CS_IP"


void GetNicInfos(NicInfos_t &nicinfos);
void PrintNicInfos(NicInfos_t &nicinfos, const SV_LOG_LEVEL logLevel = SV_LOG_DEBUG);

class CXPSOptions
{
public:
	bool setOptions(std::string const& fileName, std::string& errmsg)
	{
		boost::filesystem::path conf;
		if (fileName.empty()) {
			errmsg = "configuration file was not specified";
			return false;
		}

		conf = fileName;

		std::string lockFile(conf.string());
		lockFile += ".lck";
		if (!boost::filesystem::exists(lockFile)) {
			std::ofstream tmpLockFile(lockFile.c_str());
		}
		boost::interprocess::file_lock fileLock(lockFile.c_str());
		boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

		std::ifstream confFile(conf.string().c_str());
		if (!confFile.good()) {
			errmsg = "unable to open conf file " + conf.string() + ": " + boost::lexical_cast<std::string>(errno);
			return false;
		}

		std::string line;  // assumes conf file lines are < 1024
		while (confFile.good()) {
			std::getline(confFile, line);
			addOption(line);
		}

		return true;
	}
	void addOption(std::string line)
	{
		if ('#' != line[0] && '[' != line[0]) {
			std::string data(line);
			std::string::size_type idx = data.find_first_of("=");
			if (std::string::npos != idx) {
				std::string tag(data.substr(0, idx));
				boost::algorithm::trim(tag);

				std::string value(data.substr(idx + 1));
				boost::algorithm::trim(value);
				m_options.insert(std::make_pair(tag, value));
			}
		}
	}

	std::string getOption(std::string const& fileName, char const* option, std::string& errmsg) {
		std::string key;
		if (setOptions(fileName, errmsg))
		{
			KeyVal_t::const_iterator iter(m_options.find(option));
			if (m_options.end() != iter) {
				if (!(*iter).second.empty()) {
					key = boost::lexical_cast<std::string>((*iter).second);
				}
			}
		}
		if (!key.empty())
			DebugPrintf(SV_LOG_DEBUG, " key %s in cxps.conf is not empty, it has value %s\n", option, key.c_str());
		else
			DebugPrintf(SV_LOG_DEBUG, " key %s in cxps.conf is empty\n",option);
		return key;
	}

private:
	KeyVal_t m_options;

};

class InitialConfigUpdater{
public:
	InitialConfigUpdater(std::string amethystConfFile, std::string pushConfFile, std::string cxpsConfFile, std::string csInstallPath)
		:m_amethystConf(amethystConfFile),
		m_pushInstallConf(pushConfFile),
		m_cxpsConf(cxpsConfFile),
		m_csInstallPath(csInstallPath){}
    bool ReadAndUpdateHostId(std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;

        std::string hostid, hostIdAmtconf, hostIdPushConf,hostidcxpsconf;
        if (!ReadHostIdFromAmethystconf(hostIdAmtconf, errmsg))
        {
            errmsg << "Failed to read host id from amethyst.conf.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
            rv = FALSE;
        }

        if (!ReadHostIdFromPushConf(hostIdPushConf, errmsg))
        {
            errmsg << "Failed to read host id from pushinstaller.conf.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
            rv = FALSE;
        }

		std::string err;
		boost::shared_ptr<CXPSOptions> options(new CXPSOptions());
		hostidcxpsconf = options->getOption(m_cxpsConf, ID, err);
		if (!err.empty())
		{
			errmsg << "Failed to read host id from cxps.conf.";
			DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
			rv = FALSE;
		}
		if (rv && hostIdAmtconf.empty() && hostIdPushConf.empty() && hostidcxpsconf.empty())
        {
			DebugPrintf(SV_LOG_ERROR, "HostId in all conf files are empty updating new host id.\n");
            if (!CreateAndSaveHostId(hostid, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "CreateAndSaveHostId: Failed to update HostId .\n");
                rv = FALSE;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "CreateAndSaveHostId Succeeded: host id %s saved on amethyst.conf and pushinstaller.conf\n", hostid.c_str());
            }
        }
		bool isAllHostIdEmpty = true;
		if (hostIdAmtconf.empty() && (!hostIdPushConf.empty() || !hostidcxpsconf.empty()))
        {
			isAllHostIdEmpty = false;
        }
		if (hostIdPushConf.empty() && (!hostIdAmtconf.empty() || !hostidcxpsconf.empty()))
        {
			isAllHostIdEmpty = false;
        }
		if (hostidcxpsconf.empty() && (!hostIdAmtconf.empty() || !hostIdPushConf.empty()))
		{
			isAllHostIdEmpty = false;
		}
		if (!isAllHostIdEmpty)
		{
			errmsg << "Failed to update host id in amethyst.conf, pushinstaller.conf and cxps.conf files as in any one or more from those files host id is not empty.";
			errmsg << "To update new host id in all files please make 'HOST_ID' key's value in amethyst.conf as empty double quote(\"\"), in pushinstaller.conf make 'Hosid' key's value same as amethyst.conf,";
			errmsg << " and in cxps.conf file make 'id' key's value just empty and then run this tool again.";
			DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
			rv = FALSE;
		}
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool ReadAndUpdateIPs(std::stringstream& errmsg){

        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;

        std::string  psip, hostname;

        KeyVal_t amethystmap;
        GetKeyValuePairs(m_amethystConf, amethystmap, errmsg);

        if (!errmsg.str().empty())
        {
            rv = FALSE;
            DebugPrintf(SV_LOG_ERROR, "Failed to read key value pairs from file %s and/or %s\n", m_amethystConf.c_str(), m_pushInstallConf.c_str());
        }
        else
        {
            KeyValIter_t ait = amethystmap.find(PS_IP);
            if ( ait != amethystmap.end())
            {
              psip = ait->second;
            }

            if (psip.empty())
            {
                errmsg << "ERROR:Either'PS_IP in " << m_amethystConf  << "does not exist or it's value is not empty quoted.";
                DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
                rv = FALSE;
            }
            else if (stricmp(psip.c_str(), "\"\"") == 0)
            {
                std::string sysIp;
                sysIp = GetSystemIp();
                
				DebugPrintf(SV_LOG_DEBUG, "Updating the system ip %s to conf file %s\n", sysIp.c_str(), m_amethystConf.c_str());
                boost::shared_ptr<ConfValueObject> acvo(new ConfValueObject(m_amethystConf));
                KeyVal_t amap;
                amap.insert(std::make_pair(PS_IP, sysIp));
                bool aret = acvo->update(amap, errmsg);
                if (!aret)
                {
                    errmsg << "Failed to update PS_IP  with sysip " << sysIp << " on " << m_amethystConf << "." ;
                    DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
                    rv = FALSE;
                }
                else
                {
					DebugPrintf(SV_LOG_DEBUG, "Successfully updated ip %s on key PS_IP of %s .\n", sysIp.c_str(), m_amethystConf.c_str());
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "PS_IP in %s has an exist ip as %s\n", m_amethystConf.c_str(), psip.c_str());
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }

private:
    void GenerateUUID(std::string &hostId)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entering %s \n", __FUNCTION__);

        hostId = GetUuid();

        DebugPrintf(SV_LOG_DEBUG, "hostid %s \n", hostId.c_str());

        DebugPrintf(SV_LOG_DEBUG, "Exiting %s \n", __FUNCTION__);
    }
    bool SetHostIdInAmetystConf(const std::string& hostid, std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(m_amethystConf));
        KeyVal_t map;
        map.insert(std::make_pair(HOST_GUID, hostid));
        if (!cvo->update(map, errmsg))
        {
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool SetHostIdInPushConf(const std::string& hostid, std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(m_pushInstallConf));
        KeyVal_t map;
        map.insert(std::make_pair(HOSTID, hostid));
        if (!cvo->update(map, errmsg))
        {
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
	bool setHostIdInCxpsConf(const std::string& hostid, std::stringstream& errmsg)
	{
		bool rv = true;
		std::string cmd = std::string("\"") + m_csInstallPath + std::string("\\") + SV_CS_TRANSPORT_PATH + std::string("\\") + "cxpscli.exe" + std::string("\"");
		cmd += std::string(" ") + "--conf" + std::string(" ") + std::string("\"") + m_cxpsConf + std::string("\"");
		cmd += std::string(" ") + "--id" + std::string(" ") + hostid;
		
		DebugPrintf(SV_LOG_DEBUG, "Issuing Command : %s\n", cmd.c_str());


		std::string emsg;

		if (!execProcUsingPipe(cmd, emsg))
		{
			errmsg << emsg;
			rv = false;
			DebugPrintf(SV_LOG_ERROR, "%s\n", emsg.c_str());
		}

		return rv;
	}
    bool CreateAndSaveHostId(std::string& hostid, std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        GenerateUUID(hostid);
        if (!SetHostIdInAmetystConf(hostid, errmsg))
        {
            errmsg << "Failed to set host id in asmethyst.conf \n. ";
            DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
            rv = FALSE;
        }
        else if (!SetHostIdInPushConf(hostid, errmsg))
        {
			errmsg << "Successfully save guid to amethyst.conf file but failed to update in pushinstaller.conf and cxps.conf files.To update in all conf files,";
			errmsg << "please remove the existing value of key 'HOST_GUID' from amethyst.conf file and pur empty quote(\"\") as value and run this tool again.\n. ";
            DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
            rv = FALSE;
        }
		else if (!setHostIdInCxpsConf(hostid, errmsg))
		{
			errmsg << "Successfully save guid to amethyst.conf and PushInstaller.conf file but failed to update in cxps.conf file." << 
				"To update in all conf file, please remove the existing value of key 'HOST_GUID' from amethyst.conf file and put empty quote(\"\") as value," <<
				"and in pushinstaller.conf make 'Hosid' key's value same as amethyst.conf." <<
				" And then on run this tool again.\n. ";
			DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
			rv = FALSE;
		}

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool ReadHostIdFromAmethystconf(std::string& hostid, std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(m_amethystConf));
        if (!cvo->getValue(HOST_GUID, hostid, errmsg))
        {
            rv = FALSE;
        }
        if (hostid.empty())
        {
            errmsg << "ERROR: Either 'HOST_GUID' does not exist or it's value is not empty quoted. ";
            DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
            rv = FALSE;
        }
        else if (stricmp(hostid.c_str(), "\"\"") == 0)
        {
           /* errmsg << " ERROR: Value of the key 'HOST_GUID' is not empty quoted.\n";
            DebugPrintf(SV_LOG_ERROR, " ERROR: Value of the key 'HOST_GUID' is not empty quoted.\n");
            rv = FALSE;*/
            hostid = "";
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "HOST_GUID is not empty, it has value %s\n", hostid.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool ReadHostIdFromPushConf(std::string& hostid, std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(m_pushInstallConf));
        if (!cvo->getValue(HOSTID, hostid, errmsg))
        {
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "HostId has value %s\n", hostid.c_str());
        if (hostid.empty())
        {
            errmsg << "ERROR: Either 'HostId' does not exist or it's value is not empty quoted. ";
            DebugPrintf(SV_LOG_ERROR, " %s\n", errmsg.str().c_str());
            rv = FALSE;
        }
        else if (stricmp(hostid.c_str(), "\"\"") == 0)
        {
           /* errmsg << " ERROR: Value of the key 'HostId' is not empty quoted.\n";
            DebugPrintf(SV_LOG_ERROR, " ERROR: Value of the key 'HostId' is not empty quoted.\n");
            rv = FALSE;*/
            hostid = "";
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "HostId is not empty, it has value %s\n", hostid.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }

    bool FileExist(const std::string& Name)
    {
        return boost::filesystem::exists(Name);
    }
    bool CreateEncryptionKey(std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        std::string  encryptKeypath = securitylib::getPrivateDir() + std::string("\\") + "encryption.key";
        if (!FileExist(encryptKeypath))
        {
            ACE_Process_Options Inm_Proc_options;
            Inm_Proc_options.handle_inheritance(false);

			std::string cmd = m_csInstallPath + std::string("\\") + SV_SVSYSTEMS_BIN_PATH + std::string("\\") + GENPASSPHRASE_EXE + std::string(" ") + std::string("-k");
            Inm_Proc_options.command_line("%s %s", cmd.c_str(), "");

            ACE_Process_Manager* InmPM = ACE_Process_Manager::instance();
            if (InmPM == NULL)
            {
                errmsg << "FAILED to Get ACE Process Manager instance\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                return FALSE;
            }
            pid_t Inm_Proc_pid = InmPM->spawn(Inm_Proc_options);
            if (Inm_Proc_pid == ACE_INVALID_PID)
            {
                errmsg << "FAILED to create ACE Process for executing genpassphrase exe : " << cmd.c_str() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                return FALSE;
            }
            ACE_exitcode InmRetstatus = 0;
            pid_t rc = InmPM->wait(Inm_Proc_pid, &InmRetstatus);
            if (InmRetstatus == 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "Successfully generated the encryption.key file.\n");
            }
            else
            {
                errmsg << "FAILED: Encountered an error generating cencryption.key file.\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                rv = FALSE;
            }
        }
        else
        {
            errmsg << "Failed to create file " << encryptKeypath << " as it is already exist.\n";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool CreatePassphrase(std::stringstream& errmsg){

        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        std::string  passphrasepath = securitylib::getPrivateDir() + std::string("\\") + "connection.passphrase";
        if (!FileExist(passphrasepath))
        {
            ACE_Process_Options Inm_Proc_options;
            Inm_Proc_options.handle_inheritance(false);

			std::string cmd = m_csInstallPath + std::string("\\") + SV_SVSYSTEMS_BIN_PATH + GENPASSPHRASE_EXE;
            Inm_Proc_options.command_line("%s %s", cmd.c_str(), "");

            ACE_Process_Manager* InmPM = ACE_Process_Manager::instance();
            if (InmPM == NULL)
            {
                errmsg << "FAILED to Get ACE Process Manager instance\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                return FALSE;
            }
            pid_t Inm_Proc_pid = InmPM->spawn(Inm_Proc_options);
            if (Inm_Proc_pid == ACE_INVALID_PID)
            {
                errmsg << "FAILED to create ACE Process for executing genpassphrase exe : " << cmd.c_str() << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                return FALSE;
            }
            ACE_exitcode InmRetstatus = 0;
            pid_t rc = InmPM->wait(Inm_Proc_pid, &InmRetstatus);
            if (InmRetstatus == 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "Successfully generated the connection.passphrase file.\n");
            }
            else
            {
                errmsg << "FAILED: Encountered an error generating connection.passphrase file.\n";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                rv = FALSE;
            }
        }
        else
        {
            errmsg << "Failed to create file " << passphrasepath << " as it is already exist.\n";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }
    bool ReadKeyValueFromConfFile(const std::string& key, std::string& value, const std::string& confFile, std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = TRUE;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(confFile));
        if (!cvo->getValue(key, value, errmsg))
        {
            rv = FALSE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }

    void GetKeyValuePairs(const std::string& confFile, KeyVal_t& pairmap,std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);

        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(confFile));
        cvo->getKeyValuePairs(pairmap, errmsg);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    }
   

    std::string GetSystemIp(){
        
       DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
      
       NicInfos_t nicinfos;
       GetNicInfos(nicinfos);
       PrintNicInfos(nicinfos);
       bool foundIpV4address = false;
       std::string ip;
       for (NicInfosIter_t nicit = nicinfos.begin(); nicit != nicinfos.end(); nicit++)
       {
           Objects_t &nicconfs = nicit->second;
           for (ObjectsIter_t cit = nicconfs.begin(); cit != nicconfs.end(); cit++)
           {
               cit->Print();
               for (ConstAttributesIter_t it = cit->m_Attributes.begin(); it != cit->m_Attributes.end(); it++)
               {
                   if (stricmp(it->first.c_str(), NSNicInfo::IP_ADDRESSES) == 0)
                   {
                       std::string iparress = it->second;
                       std::vector<std::string> pairs;
                       Tokenize(iparress, pairs, ",");
                       std::vector<std::string>::const_iterator pairIter = pairs.begin();
                       while (pairIter != pairs.end())
                       {
                           if (IsValidIP(*pairIter))
                           {
                               ip = *pairIter;
                               foundIpV4address = true;
                               break;
                           }
                           pairIter++;
                       }
                       
                       DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", it->first.c_str(), it->second.c_str());
                   }
                   if (foundIpV4address)
                       break;
               }
               if (foundIpV4address)
                   break;
           }
           if (foundIpV4address)
               break;
       }

       DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
       return ip;
   }

	std::string m_amethystConf;
	std::string m_pushInstallConf;
	std::string m_cxpsConf;
	std::string m_csInstallPath;
};


#endif //INITIALCONFIGUPDATER__H