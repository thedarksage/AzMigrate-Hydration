/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	RcmJobInputValidationFailedException.h

Description	:   Exception thrown when validations fail on the input passed for Rcm job execution.

+------------------------------------------------------------------------------------+
*/

#ifndef __RCMJOB_INPUTVALIDATIONFAILED_H
#define __RCMJOB_INPUTVALIDATIONFAILED_H

#include <string>

namespace PI
{
	/// \brief Exception thrown when validations fail on the input passed for Rcm job execution.
	class RcmJobInputValidationFailedException : public std::logic_error
	{
	public:
		RcmJobInputValidationFailedException(const std::string &propertyKey, const std::string &errorMessage = "")
			: std::logic_error(
				"Job input validations failed for the key: " +
				propertyKey +
				"." +
				errorMessage) { };
	};
}

#endif