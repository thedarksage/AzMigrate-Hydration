/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	NotInitializedException.h

Description	:   Thrown when the called object is not initialized.

+------------------------------------------------------------------------------------+
*/

#ifndef __NOTINITIALIZED_EXCEPTION_H__
#define __NOTINITIALIZED_EXCEPTION_H__

#include <string>

namespace PI
{
	/// \brief thrown when the called object is not initialized.
	class NotInitializedException : public std::logic_error
	{
	public:
		NotInitializedException(const std::string & calledObjectName) :
			std::logic_error(
			calledObjectName +
			" is not initialized properly before invoking the object's methods.") { };
	};
}

#endif