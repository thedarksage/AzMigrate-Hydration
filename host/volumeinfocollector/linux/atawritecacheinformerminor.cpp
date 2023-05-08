#include <string>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <linux/hdreg.h>
#include "atawritecacheinformer.h"


VolumeSummary::WriteCacheState 
 AtaWriteCacheInformer::GetWriteCacheStateDirectly(const std::string &disk)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;
    int fd = open(disk.c_str(), O_RDONLY);

    if (-1 != fd)
    {
        #ifdef HDIO_GET_WCACHE
        long wce = 0;
        if (-1 != ioctl(fd, HDIO_GET_WCACHE, &wce))
        {
            state = wce ? VolumeSummary::WRITE_CACHE_ENABLED:
                          VolumeSummary::WRITE_CACHE_DISABLED;
        }
        #endif  /* HDIO_GET_WCACHE */
        close(fd);
    }

    return state;
}
