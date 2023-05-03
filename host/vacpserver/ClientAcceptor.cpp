/*
ACE 
http://ace.sourcearchive.com/documentation/5.7.7/ACE_8cpp-source.html

Copyright (c) 1995-2003 Douglas C. Schmidt and his research group at Washington University, University of California, Irvine, and [Vanderbilt University. All rights reserved. 
Since the software is open-source, free software, you are free to use, modify, copy, and distribute--perpetually and irrevocably--the software source code and object code produced from the source, as well as copy and distribute modified versions of this software. You must, however, include this copyright statement along with code built using the software.
*/

#include "ace/Reactor.h"
#include "ace/Mutex.h"
#include "ace/Null_Mutex.h"
#include "ace/Null_Condition.h"
#include "VacpService.h"
#include "ace/OS_NS_unistd.h"
#include "ace/os_include/os_netdb.h"
#include "ClientAcceptor.h"

#include "configurator2.h"
#include "logger.h"

#include "CxCommunicator.h"


ClientAcceptor::ClientAcceptor(CxCommunicator& cxc, ACE_Reactor *reactor, int signum) :
ACE_Event_Handler(),
mReactor(reactor == 0 ? ACE_Reactor::instance() : reactor),
acceptor_(),
cxComm(cxc) 
{
	 mReactor->register_handler (signum, this);
	 mReactor->register_handler (SIGTERM, this);
}

int
ClientAcceptor::open (const ACE_INET_Addr &listen_addr)
{
  if (this->acceptor_.open (listen_addr, 1) == -1)
  {
    DebugPrintf(SV_LOG_ERROR, "Failed to open\n");
	return -1;
  }
  return this->mReactor->register_handler
    (this, ACE_Event_Handler::ACCEPT_MASK);
}

int
ClientAcceptor::handle_input (ACE_HANDLE)
{
  VacpService *client;
  ACE_NEW_RETURN (client, VacpService(cxComm), -1);

  auto_ptr<VacpService> p (client);

  if (this->acceptor_.accept (client->peer ()) == -1)
  {
    DebugPrintf(SV_LOG_ERROR, "Failed to accept client connection\n");
	return -1;
  }
  p.release ();
  client->reactor (this->mReactor);
  if (client->open () == -1)
    client->handle_close (ACE_INVALID_HANDLE, 0);
  return 0;
}

int
ClientAcceptor::handle_signal (int signum, siginfo_t *, ucontext_t *)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
    	DebugPrintf("Stopping Service. Received signal %d\n", signum);
        mReactor->end_reactor_event_loop ();
	cxComm.signalQuit();
        return -1;
    }
    return 0;
}

ClientAcceptor::~ClientAcceptor ()
{
  this->handle_close (ACE_INVALID_HANDLE, 0);
}

int
ClientAcceptor::handle_close (ACE_HANDLE, ACE_Reactor_Mask)
{
  if (this->acceptor_.get_handle () != ACE_INVALID_HANDLE)
    {
      ACE_Reactor_Mask m = ACE_Event_Handler::ACCEPT_MASK |
                           ACE_Event_Handler::DONT_CALL;
      this->reactor ()->remove_handler (this, m);
      this->acceptor_.close ();
    }
  return 0;
}
