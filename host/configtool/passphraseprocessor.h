#ifndef PASSPHRASEPROCESSOR__H
#define PASSPHRASEPROCESSOR__H

#include <sstream>

#include <windows.h>

#include <boost/filesystem.hpp>

#include "extendedlengthpath.h"

#include "logger.h"
#include "portable.h"
#include "ProcessMgr.h"
#include "csgetfingerprint.h"
#include "localconfigurator.h"


#define BIN_PATH "home\\svsystems\\bin"
#define GENCERT_EXE "gencert.exe"
#define GENPASSPHRASE_EXE "genpassphrase.exe"


bool ProcessCrt(const std::string& cspsInstallPath, std::stringstream& errmsg)
{
    // to do if process creation os cert using 'gencert.exe -n ps --dh'
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = TRUE;
	std::string cmd = cspsInstallPath + std::string("\\") + BIN_PATH + std::string("\\") + GENCERT_EXE + " -n ps --dh";
    DWORD exitcode;
    if (!ExecuteProc(cmd, exitcode, errmsg))
    {
        DebugPrintf(SV_LOG_ERROR, "ProcessCrt failed for command line process %s\n", cmd.c_str());
        rv = false;
    }
    else
    {
        if (exitcode != 0)
        {
            std::stringstream msg;
            msg << "ProcessCrt failed for command line process" << cmd << " with exit code " << exitcode;
            DebugPrintf(SV_LOG_ERROR, "%s \n", msg.str().c_str());
            rv = false;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "ProcessCrt return Success for command line process %s \n", cmd.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}

bool ProcessEncryptionKey(const std::string& cspsInstallPath, std::stringstream& errmsg)
{
    // to do process creation of enc key using 'genpassphrase.exe -k'
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = TRUE;
	std::string cmd = cspsInstallPath + std::string("\\") + BIN_PATH + std::string("\\") + GENPASSPHRASE_EXE + " -k";
    DWORD exitcode;
    if (!ExecuteProc(cmd, exitcode, errmsg))
    {
        DebugPrintf(SV_LOG_ERROR, "ProcessEncryptionKey failed for command line process %s\n", cmd.c_str());
        rv = false;
    }
    else
    {
        if (exitcode != 0)
        {
            std::stringstream msg;
            msg << "ProcessEncryptionKey failed for command line process" << cmd << " with exit code " << exitcode;
            DebugPrintf(SV_LOG_ERROR, "%s \n", msg.str().c_str());
            rv = false;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "ProcessEncryptionKey return Success for command line process %s \n", cmd.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}
bool validatepassphrase(std::string ipAddress, std::string port, std::string hostId, std::string passphrase, std::string &err_msg, bool & isVerifiedCreatedpassphrase)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG, "csip %s\n", ipAddress.c_str());
	DebugPrintf(SV_LOG_DEBUG, "port %s\n", port.c_str());
	DebugPrintf(SV_LOG_DEBUG, "hostId %s\n", hostId.c_str());

    bool rc;
    std::string reply;
    std::string user;
    bool verifyPassphraeOnly = false;
    bool useSsl;
    bool overwrite = true;

    if (passphrase.empty()) {
        err_msg = "Passphrase should not be empty";
        return false;
    }

    if (passphrase.length() < 16) {
        err_msg = "Passphrase length should be atleast 16 characters";
        return false;
    }

    useSsl = true;

	DebugPrintf(SV_LOG_DEBUG, "Getting csGetFingerprint.. \n");
    rc = securitylib::csGetFingerprint(reply, hostId, passphrase, ipAddress, port, verifyPassphraeOnly, useSsl, overwrite);
	DebugPrintf(SV_LOG_DEBUG, "Got csGetFingerprint successfully. \n");

    err_msg = reply;
    std::cout << "err_msg :" << err_msg << std::endl;
    isVerifiedCreatedpassphrase = true;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rc;
}
bool UpdatePassphrase(const std::string& pscsip, const std::string& pscsport, const std::string& hostid, const std::string& pphrase, std::stringstream& errmsg)
{
    //to do validate and write passphrase
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = TRUE;

    bool isVerifiedCreatedpassphrase = false;
    std::string err;
    if (validatepassphrase(pscsip, pscsport, hostid, pphrase, err, isVerifiedCreatedpassphrase))
    {
        DebugPrintf(SV_LOG_DEBUG, "validate pass phrase returned true.\n");
        if (securitylib::writePassphrase(pphrase))
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully wrote pass phrase.\n");
        }
    }
    else
    {
        rv = false;
        errmsg << "Passphrase validation failed " << err;
        DebugPrintf(SV_LOG_ERROR, "%s\n",errmsg.str().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}
bool ProcPassphrase(const std::string& cspsInstallPath, const std::string& pscsip,const std::string& pscsport,const std::string& hostid,const std::string& pphrase,std::stringstream& errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = TRUE;
    

    bool initialCertProcessed = false;

    std::string  certpath = securitylib::getCertDir() + std::string("\\") + "ps.crt";
    extendedLengthPath_t extName(ExtendedLengthPath::name(certpath));
   

    initialCertProcessed = true;

    if (!boost::filesystem::exists(extName))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s file does not exist, creating certs and encryption key\n", certpath.c_str());
		if (ProcessCrt(cspsInstallPath, errmsg))
        {
			if (!ProcessEncryptionKey(cspsInstallPath, errmsg))
            {
                rv = false;
            }
        }
        else
        {
            rv = false;
        }
    }
    if (rv)
    {
        if (!UpdatePassphrase(pscsip,pscsport,hostid,pphrase, errmsg))
        {
            rv = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}


void GetPPhrase(std::string& passphrase)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    passphrase = securitylib::getPassphrase();
    if (passphrase.empty())
        DebugPrintf(SV_LOG_DEBUG, "Did not find passphrase\n");

      DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}

#endif //PASSPHRASEPROCESSOR__H