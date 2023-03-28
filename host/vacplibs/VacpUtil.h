#ifndef __VACP_UTIL__
#define __VACP_UTIL__
#include <svtypes.h>

#define LINE_NO			__LINE__
#define FILE_NAME		__FILE__

#ifdef SV_WINDOWS
#define FUNCTION_NAME __FUNCTION__
#else
#define FUNCTION_NAME __func__
#endif
#include <string>

class VacpUtil
{
	public:
	 	static bool TagTypeToAppName(unsigned short TagType, std::string & AppName);
	 	static bool AppNameToTagType(const std::string & AppName, unsigned short & type);
		static SV_USHORT Swap(const SV_USHORT);
		static SV_ULONG Swap(const SV_ULONG);
};
#endif
