/*
ACE 
http://ace.sourcearchive.com/documentation/5.7.7/ACE_8cpp-source.html

Copyright (c) 1995-2003 Douglas C. Schmidt and his research group at Washington University, University of California, Irvine, and [Vanderbilt University. All rights reserved. 
Since the software is open-source, free software, you are free to use, modify, copy, and distribute--perpetually and irrevocably--the software source code and object code produced from the source, as well as copy and distribute modified versions of this software. You must, however, include this copyright statement along with code built using the software.
*/

#include "ace/Auto_Ptr.h"
#include "ace/Log_Msg.h"
#include "ace/INET_Addr.h"
#include "ace/SOCK_Acceptor.h"
#include "ace/Reactor.h"

class CxCommunicator;
class ClientAcceptor : public ACE_Event_Handler
{
public:
	ClientAcceptor(CxCommunicator &cxc, ACE_Reactor *reactor = 0, int signum = SIGINT);

	virtual ~ClientAcceptor ();

	int open (const ACE_INET_Addr &listen_addr);

	// Get this handler's I/O handle.
	virtual ACE_HANDLE get_handle (void) const
	{ return this->acceptor_.get_handle (); }

	// Called when a connection is ready to accept.
	virtual int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);

	// Called when this handler is removed from the ACE_Reactor.
	virtual int handle_close (ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

	// Called when object is signaled by OS.
	virtual int handle_signal (int signum, siginfo_t * = 0, ucontext_t * = 0);

protected:
	ACE_SOCK_Acceptor acceptor_;
	/**
	* The reactor to which the accept handler belongs.
	*/
	ACE_Reactor *mReactor;
	CxCommunicator &cxComm;
};
