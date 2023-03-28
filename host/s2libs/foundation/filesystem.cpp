/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : filesystem.cpp
 *
 * Description: 
 */
#include <cstdio>

#include "portablehelpers.h"
#include "filesystem.h"

/*
 * FUNCTION NAME : FileSystem
 *
 * DESCRIPTION : Constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
FileSystem::FileSystem()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : ~FileSystem
 *
 * DESCRIPTION : Destructor
 *
 * INPUT PARAMETERS :
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
FileSystem::~FileSystem()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void FileSystem::ClusterRange::Print(void) const
{
    DebugPrintf(SV_LOG_DEBUG, "Cluster Range:\n");
    DebugPrintf(SV_LOG_DEBUG, "start: " ULLSPEC "\n", m_Start);
    DebugPrintf(SV_LOG_DEBUG, "count: " ULLSPEC "\n", m_Count);
}
