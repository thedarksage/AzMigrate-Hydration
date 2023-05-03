/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : replicationpairoperations.h
 *
 * Description: 
 */

#ifndef REPLICATIONPAIROPERATIONS_H
#define REPLICATIONPAIROPERATIONS_H

#include <string>

#define INMAGE_HOSTTYPE_TARGET 1
#define INMAGE_HOSTTYPE_SOURCE 2

struct Configurator;
class ReplicationPairOperations {
public: 
    bool PauseReplication(
			Configurator* cxConfigurator,
			std::string volumeName,
			int hostType,
			char const* reasonTxt,
			long errorCode);
	bool ResumeReplication(
			Configurator* cxConfigurator,
			std::string volumeName,
			int hostType,
			char const* reasonTxt);
};


#endif // ifndef REPLICATIONPAIROPERATIONS_H


