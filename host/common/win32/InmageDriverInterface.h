#include "portablehelpersmajor.h"

#include <set>

#define TEST_FLAG(a, f)         (((a) & (f)) == (f))
#define CLEAR_FLAG(_f, _b)      ((_f) &= ~(_b))
#define SET_FLAG(_f, _b)        ((_f) |=  (_b))

class InmageDriverInterface{
private:
    HANDLE      m_hInmageDevice;

public:
    InmageDriverInterface();
    ~InmageDriverInterface();
    bool    IsDiskRecoveryRequired();
    void    SetSanPolicyToOnlineForAllDisks();
    bool    GetDiskIndexList(std::set<SV_ULONG>&  diskIndexList);
    bool    StopFilteringAll();
    bool    GetDriverStats();
    bool    GetDriverStats(std::string   sDeviceId);
};