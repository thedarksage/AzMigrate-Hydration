/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : resyncrequestor.cpp
 *
 * Description: 
 */

#include <string>
#include <sstream>
#include <exception>

#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "resyncrequestor.h"

/********************************************************************************************
*** Note: Please write any platform specific code in ${platform}/resyncrequestor_port.cpp ***
********************************************************************************************/

const SV_ULONG ResyncRequiredError::USER_SPACE_MIN_ERROR = ResyncRequiredError::UNKNOWN;
const SV_ULONG ResyncRequiredError::USER_SPACE_MAX_ERROR = 0x500;

ResyncRequiredError::ResyncRequiredError()
{
	m_ErrorCodeToMsg.insert(std::make_pair(UNKNOWN, "Unknown error"));
	m_ErrorCodeToMsg.insert(std::make_pair(IO_OUT_OF_TRACKING_SIZE, "IO out of tracking size"));
}


bool ResyncRequiredError::IsUserSpaceErrorCode(const SV_ULONG &errorcode) const
{
	return (errorcode >= USER_SPACE_MIN_ERROR) && (errorcode <= USER_SPACE_MAX_ERROR);
}


std::string ResyncRequiredError::GetErrorMessage(const SV_ULONG &errorcode)
{
	std::string errmsg = m_ErrorCodeToMsg[UNKNOWN];
	ConstErrorCodeToMsgIter_t cit = m_ErrorCodeToMsg.find(errorcode);
	if (m_ErrorCodeToMsg.end() != cit)
		errmsg = cit->second;

	return errmsg;
}
