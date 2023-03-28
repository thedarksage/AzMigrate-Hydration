//VSS Health Error codes for windows
#include<winerror.h>
#include<svtypes.h>

//Health alert for ASR VSS service not installed
# define ASR_VSS_PROVIDER_MISSING_ERROR_CODE REGDB_E_CLASSNOTREG

//Health alert for ASR VSS service disabled
# define ASR_VSS_SERVICE_DISABLED_ERROR_CODE ERROR_SERVICE_DISABLED

class ErrorCodeConvertor
{
private:
	const long adsiErrorCodeConstant = 0x80070000L;
	const long adsiErrorCodeConvertorConstatnt = 0xFFFFL;
public:
	long GetWin32ErrorCode(const long &errorCode) const
	{
		//check for ADSI error codes
		long errorcode = errorCode & adsiErrorCodeConstant;
		if (errorcode == adsiErrorCodeConstant)
		{
			return GetWin32ErrorCodeFromAdsiCode(errorCode);
		}
		else
		{
			return errorCode;
		}
	}

	//For conversion from adsi to win32 error code refer 
	//https://docs.microsoft.com/en-us/windows/desktop/adsi/win32-error-codes-for-adsi
	//lmerr.h error codes support not present.
	//converts adsi error code to win32 error code
	long GetWin32ErrorCodeFromAdsiCode(const long &errorCode) const
	{
		return errorCode & adsiErrorCodeConvertorConstatnt;
	}
};

