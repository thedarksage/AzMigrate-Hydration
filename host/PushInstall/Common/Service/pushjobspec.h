///
/// \file pushspecparser.h
///
/// \brief
///

#ifndef INMAGE_PUSHJOBSPEC_H
#define INMAGE_PUSHJOBSPEC_H

#include <string>

namespace PI {

	const std::string SECTION_VER = "version";
	const std::string SECTION_GLOBAL = "global";
	const std::string SECTION_JOBDEFINITION = "jobdefinition";

	//[version]
	const std::string SPEC_VERSION = "version";
	

	//[global]
	const std::string PID_FILEPATH = "pidpath";
	const std::string STD_OUT_FILEPATH = "stdout";
	const std::string STD_ERR_FILEPATH = "stderr";
	const std::string EXIT_STATUS_FILEPATH = "exitstatusfile";
	const std::string CS_IP = "csip";
	const std::string CS_PORT = "csport";
	const std::string SECURE_COMMUNICATION = "secure";

	//[jobdefinition]
	const std::string BIOS_ID = "biosid";
	const std::string FQDN = "fqdn";
	const std::string JOB_ID = "jobid";
	const std::string JOB_TYPE = "jobtype";
	const std::string JOB_PRE_COMMAND = "precommand";
	const std::string JOB_COMMAND = "command";
	const std::string JOB_POST_COMMAND = "postcommand";
	const std::string INSTALLER_PATH = "installer";
	const std::string INSTALLATION_PATH = "installpath";
	const std::string PASSPHRASE_FILEPATH = "passphrasefile";
	const std::string CERTCONFIG_FILEPATH = "certconfigfile";
	const std::string USER_NAME = "username";
	const std::string DOMAIN_NAME = "domain";
	const std::string ENCRYPTED_PASSWORD_FILE = "encrypted_password_file";
	const std::string ENCRYPTION_KEY_FILE = "encryption_key_file";
	const std::string VERIFY_SIGNATURE = "verifysignature";
	const std::string SKIP_INSTALL = "skipinstall";

};

#endif
