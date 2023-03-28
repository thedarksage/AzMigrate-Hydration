#ifndef PI_FALLBACKEXCEPTION__H
#define PI_FALLBACKEXCEPTION__H

#include <exception>
#include <string>
#include <map>

class PushInstallFallbackException : std::exception
{
public:

	PushInstallFallbackException(
		std::string errorCode,
		std::string errorCodeName,
		std::string errorMessage,
		std::map<std::string, std::string> placeHolders)
		:std::exception(errorMessage.c_str()),
		ErrorCode(errorCode),
		ErrorCodeName(errorCodeName),
		ErrorMessage(errorMessage),
		ComponentErrorCode(""),
		PlaceHolders(placeHolders)
	{
	}

	PushInstallFallbackException(
		std::string errorCode,
		std::string errorCodeName,
		std::string errorMessage,
		std::string componentErrorCode,
		std::map<std::string, std::string> placeHolders)
		:std::exception(errorMessage.c_str()),
		ErrorCode(errorCode),
		ErrorCodeName(errorCodeName),
		ErrorMessage(errorMessage),
		ComponentErrorCode(componentErrorCode),
		PlaceHolders(placeHolders)
	{
	}

	std::string ErrorCode;
	std::string ErrorCodeName;
	std::string ErrorMessage;
	std::string ComponentErrorCode;
	std::map<std::string, std::string> PlaceHolders;
};


#endif
