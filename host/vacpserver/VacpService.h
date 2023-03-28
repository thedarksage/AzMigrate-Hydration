/*
ACE 
http://ace.sourcearchive.com/documentation/5.7.7/ACE_8cpp-source.html

Copyright (c) 1995-2003 Douglas C. Schmidt and his research group at Washington University, University of California, Irvine, and [Vanderbilt University. All rights reserved. 
Since the software is open-source, free software, you are free to use, modify, copy, and distribute--perpetually and irrevocably--the software source code and object code produced from the source, as well as copy and distribute modified versions of this software. You must, however, include this copyright statement along with code built using the software.
*/

#ifndef VACP_SERVICE__H
#define VACP_SERVICE__H
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"

#include "ace/SOCK_Acceptor.h"
#include "ace/Thread_Manager.h"
#include "ace/Handle_Set.h"
#include "ace/Profile_Timer.h"
#include "ace/OS_NS_sys_select.h"
#include "ace/OS_main.h"
#include "ace/OS_NS_unistd.h"
#include "ace/OS_main.h"
#include "ace/Service_Config.h"
#include "ace/Thread_Manager.h"
#include "ace/Process_Manager.h"
#include "ace/Get_Opt.h"
#include "configurator2.h"
#include "logger.h"

#include "ace/Message_Block.h"
#include "ace/Message_Queue.h"
#include "ace/SOCK_Stream.h"


typedef struct _VACP_SERVER_RESPONSE_ 
{
		ACE_INT32 iResult;
        ACE_INT32 val;
        char str[1024];
}VACP_SERVER_RESPONSE;

#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include "volumeinfocollector.h"
#include "StreamEngine.h"
#include "cdputil.h"
#include "configurator2.h"

#include "CxCommunicator.h"
// TODO: all these should come from devicefilter.h
// currently, we are having some multiple definitions in devicefilter.h
// and user space streamengine header file
// so, for time being making copy of this
// need to resolve this asap
//
#define INMAGE_FILTER_DEVICE_NAME    "/dev/involflt"
#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP 0x0001
#define TAG_FS_CONSISTENCY_REQUIRED                   0x0002

#define FLT_IOCTL 0xfe
#define IOCTL_INMAGE_TAG_VOLUME                 _IOWR(FLT_IOCTL, TAG_CMD, unsigned long)
enum {
    STOP_FILTER_CMD = 0,
    START_FILTER_CMD, 
    START_NOTIFY_CMD,
    SHUTDOWN_NOTIFY_CMD,
    GET_DB_CMD,
    COMMIT_DB_CMD,
    SET_VOL_FLAGS_CMD,
    GET_VOL_FLAGS_CMD,
    WAIT_FOR_DB_CMD,
    CLEAR_DIFFS_CMD,
    GET_TIME_CMD,
    UNSTACK_ALL_CMD,
    SYS_SHUTDOWN_NOTIFY_CMD,
	TAG_CMD,
};


class VacpService : public ACE_Event_Handler
{

public :
	VacpService(CxCommunicator &cx):CxComm(cx){}
	int ProcessTags(char *TagDataBuffer, uint32_t, bool);

private:
	std::string GenerateSanDeviceName(std::string& devname);
	int OpenAndIssueIoctl(char *buffer);

public:
  ACE_SOCK_Stream &peer (void) { return this->sock_; }

  int open (void);

  // Get this handler's I/O handle.
  virtual ACE_HANDLE get_handle (void) const
    { return this->sock_.get_handle (); }

  // Called when input is available from the client.
  virtual int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);

  // Called when this handler is removed from the ACE_Reactor.
  virtual int handle_close (ACE_HANDLE handle,
                            ACE_Reactor_Mask close_mask);

protected:
  ACE_SOCK_Stream sock_;
  CxCommunicator &CxComm;


};
#endif
