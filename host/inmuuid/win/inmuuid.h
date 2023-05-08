#ifndef INM_UUID_H
#define INM_UUID_H

#include <string>
#include <Rpc.h>
#include "inmageex.h"

/* Link to system's Rpcrt4.lib in module project file.;
   Put host/common also in include path along with host/inmuuid/win */

inline std::string GetUuid(void)
{
    UUID a;
    RPC_STATUS s = UuidCreate(&a);
    if (RPC_S_OK != s)
        throw INMAGE_EX("UuidCreate failed with error: ")(s);

    RPC_CSTR uuidstr;
    s = UuidToString(&a, &uuidstr);
    if (RPC_S_OK != s)
        throw INMAGE_EX("UuidToString failed with error: ")(s);

    std::stringstream ssid;
    ssid << uuidstr;
    s = RpcStringFree(&uuidstr);
    if (RPC_S_OK != s)
        throw INMAGE_EX("RpcStringFree failed with error: ")(s);

    return ssid.str();
}

#endif /* INM_UUID_H */
