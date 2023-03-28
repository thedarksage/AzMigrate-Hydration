#include <string>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/vtoc.h>
#include <sys/dktp/dadkio.h>
#include "volumemanagerfunctions.h"
#include "atawritecacheinformer.h"

VolumeSummary::WriteCacheState 
 AtaWriteCacheInformer::GetWriteCacheStateDirectly(const std::string &disk)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;
    std::string rawdisk = GetRawDeviceName(disk);
    int fd = open(rawdisk.c_str(), O_RDONLY);

    if (-1 != fd)
    {
        #ifdef DIOCTL_GETWCE
        unsigned int wce = 0;
        int rval = ioctl(fd, DIOCTL_GETWCE, &wce);
        if (-1 != rval)
        {
            state = wce ? VolumeSummary::WRITE_CACHE_ENABLED:
                          VolumeSummary::WRITE_CACHE_DISABLED;
        }
        #endif /* DIOCTL_GETWCE */
        close(fd);
    }

    return state;
}
