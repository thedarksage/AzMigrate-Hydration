///
/// \file remoteapimajor.h
///
/// \brief
///


#ifndef INMAGE_REMOTE_API_MAJOR_H
#define INMAGE_REMOTE_API_MAJOR_H

#include <winsock2.h>

namespace remoteApiLib {

	void global_init_major()
	{
		WSADATA wsadata;
		WSAStartup(MAKEWORD(2, 0), &wsadata);
	}

	void global_exit_major()
	{
		WSACleanup();
	}

} // remoteApiLib

#endif