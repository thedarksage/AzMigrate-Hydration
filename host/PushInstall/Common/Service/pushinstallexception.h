#ifndef PI_EXCEPTION__H
#define PI_EXCEPTION__H

#include <exception>
#include <string>
#include <map>

class PushInstallException : std::exception
{
public:

	PushInstallException(
		std::string errorCode,
		std::string errorCodeName,
		std::string errorMessage,
		std::map<std::string, std::string> placeHolders)
		:std::exception(errorMessage.c_str()),
		ErrorCode(errorCode),
		ErrorCodeName(errorCodeName),
		ErrorMessage(errorMessage),
		PlaceHolders(placeHolders)
	{
	}

	std::string ErrorCode;
	std::string ErrorCodeName;
	std::string ErrorMessage;
	std::map<std::string, std::string> PlaceHolders;
};


#endif