#include <string>
#include <sstream>
#include <sys/types.h>
#include "atawritecacheinformer.h"

VolumeSummary::WriteCacheState 
 AtaWriteCacheInformer::GetWriteCacheStateDirectly(const std::string &disk)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;

    /* TODO: implement */

    return state;
}

