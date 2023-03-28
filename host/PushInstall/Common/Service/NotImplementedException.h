/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	NotImplementedException.h

Description	:   Thrown when the required routine or some functionality is not implemented

+------------------------------------------------------------------------------------+
*/

#ifndef __NOTIMPLEMENTED_EXCEPTION_H__
#define __NOTIMPLEMENTED_EXCEPTION_H__

#include <string>
#include "pushconfig.h"

namespace PI
{
	/// \brief Thrown when the required routine or some functionality is not implemented
	class NotImplementedException : public std::logic_error
	{
	public:
		NotImplementedException(const std::string &functionName, const std::string &scenario)
			: std::logic_error(
				functionName +
				"is not implemented for scenario : " +
				scenario +
				"!") { };
	};
}

#endif