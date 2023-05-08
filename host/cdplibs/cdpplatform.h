//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpplatform.h
//
// Description: 
// This file would contain declarations for routines which
// requires platform specific implementation
//


#ifndef CDPPLATFORM__H
#define CDPPLATFORM__H

#include <svtypes.h>
#include <string>
#include "cdpdrtd.h"
#include <ace/OS_NS_unistd.h>
#include <ace/Process_Manager.h>

bool GetServiceState(const std::string & ServiceName, SV_ULONG & state);
bool GetDiskFreeCapacity(const std::string & dir, SV_ULONGLONG & freespace);

//bool OpenVVControlDevice(void **CtrlDevice);
bool OpenVsnapControlDevice(ACE_HANDLE &CtrlDevice);
long OpenInVirVolDriver(ACE_HANDLE &hDevice);

//void CloseVVControlDevice(void * CtrlDevice);
void CloseVVControlDevice(ACE_HANDLE &CtrlDevice);
int IssueMapUpdateIoctlToVVDriver(ACE_HANDLE& CtrlDevice, 
								  const char *VolumeName, 
								  const char *DataFileName,
								  const SV_UINT & length,
								  const SV_OFFSET_TYPE & volume_offset,
								  const SV_OFFSET_TYPE & file_offset,
								  bool Cow);

void TerminateProcess(ACE_Process_Manager *, pid_t);
void GetAgentInstallPath(std::string &);

// This is made similar to GetDiskFreeSpaceEx as on windows
bool GetDiskFreeSpaceP(const std::string & dir,SV_ULONGLONG *quota,SV_ULONGLONG *capacity,SV_ULONGLONG *ulTotalNumberOfFreeBytes);
std::string getParentProccessName();


#endif

