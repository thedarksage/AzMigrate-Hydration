///
/// \file supportedplatforms.h
///
/// \brief
///


#ifndef INMAGE_PI_SUPPORTED_PLATFORMS_H
#define INMAGE_PI_SUPPORTED_PLATFORMS_H

#include <string>

namespace remoteApiLib {


	enum os_idx { windows_idx =1, unix_idx};

	const std::string windows_platform = "windows";
	const std::string unix_platform = "unix";
	const std::string unsupported_platform = "unsupported platform";

	inline std::string platform(enum os_idx idx) 
	{

		if (idx == windows_idx) {
			return std::string(windows_platform);
		}
		else if (idx == unix_idx){
			return std::string(unix_platform);
		} else {
			return std::string(unsupported_platform);
		}
	}

	inline std::string pathSeperator() { return "\\"; }
	inline std::string pathSeperatorToReplace() { return "/"; }


	inline std::string pathSeperator(enum os_idx idx)
	{
		if (idx == windows_idx) {
			return "\\" ;
		}
		else if (idx == unix_idx){
			return "/";
		}
		else {
			return "/";
		}
	}

	inline std::string pathSeperatorToReplace(enum os_idx idx)
	{
		if (idx == windows_idx) {
			return "/";
		}
		else if (idx == unix_idx){
			return "\\";
		}
		else {
			return "/";
		}
	}

	inline std::string NewLine(enum os_idx idx)
	{
		if (idx == windows_idx) {
			return "\r\n";
		}
		else if (idx == unix_idx){
			return "\n";
		}
		else {
			return "\n";
		}
	}

} // remoteApiLib

#endif