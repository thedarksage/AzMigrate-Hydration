#ifndef _MICROSOFTTESTINMAGEEXLCUDEBLOCKS_H_
#define _MICROSOFTTESTINMAGEEXLCUDEBLOCKS_H_
#include "IPlatformAPIs.h"
#include <list>
#include <map>
#include <string>
#include "svtypes.h"

#ifdef SV_WINDOWS
#ifdef MICROSOFTTESTINMAGEEXLCUDEBLOCKS_EXPORTS
#define MICROSOFTTESTINMAGEEXLCUDEBLOCKS_API __declspec(dllexport)
#else
#define MICROSOFTTESTINMAGEEXLCUDEBLOCKS_API __declspec(dllimport)
#endif
#else
#define MICROSOFTTESTINMAGEEXLCUDEBLOCKS_API
#endif


// This class is exported from the Microsoft.Test.Inmage.ExlcudeBlocks.dll
class MICROSOFTTESTINMAGEEXLCUDEBLOCKS_API IInmageExlcudeBlocks {
public:
    virtual std::list<ExtentInfo> GetExcludeBlocks(std::string diskId) = 0;
};

#endif
