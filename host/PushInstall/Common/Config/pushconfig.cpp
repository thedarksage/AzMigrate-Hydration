//---------------------------------------------------------------
//  <copyright file="pushconfig.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  PushConfig class implementation.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------

#include "pushconfig.h"

namespace PI
{
	/// \brief singleton pushconfig instance
	static PushConfigPtr pushConfigInstance;
	
	bool isPushProxyInitialized = false;

	/// \brief mutex to initialize pushconfig implementation
	boost::mutex g_pushConfigInitMutex;

	static Lockable m_SyncLock;

	std::string PushConfig::PushServerIp()
	{
		if (!m_isPushServerIpSet)
		{
			AutoLock CreateGuard(m_SyncLock);
			if (!m_isPushServerIpSet)
			{
				m_pushserverIp = Host::GetInstance().GetIPAddress();
				m_isPushServerIpSet = true;
			}
		}

		return m_pushserverIp;
	}
}