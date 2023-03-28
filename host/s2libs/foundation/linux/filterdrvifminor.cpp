#include <cstring>
#include <cstdio>
#include <cerrno>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "filterdrvifminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"


std::string const FilterDrvProcGlobalFile("/proc/involflt/global");
std::string const FilterDrvSysFsDir("/sys/involflt/");


FilterDrvIfMinor::FilterDrvIfMinor()
{
}


int FilterDrvIfMinor::PassMaxTransferSizeIfNeeded(const DeviceFilterFeatures *pdevicefilterfeatures, const std::string &volume, DeviceStream &devstream,
                                                  const VolumeReporter::VolumeReport_t &vr)
{
    return SV_SUCCESS;
}


/* TODO: common code should be used as ops are similar for bsize and nblks */
bool FilterDrvIfMinor::GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, const int &errorfromgetattr, 
                                          std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());
    bool got = true;

    if (ERR_INVALID_PARAMETER == errorfromgetattr) {
        struct stat st;
        if (0 == stat(FilterDrvProcGlobalFile.c_str(), &st))
            got = GetVolumeBlockSizeFromSysFs(devicename, blksz, reasonifcant);
        else {
            int saveerrno = errno;
            std::stringstream ss;
            ss << "Failed to get blocks size from filter driver for volume " << devicename
               << " as stat on " << FilterDrvProcGlobalFile << " failed with error code " 
               << saveerrno << ", message: " << strerror(saveerrno);
            reasonifcant = ss.str();
            got = false;
        }
    } else {
        std::stringstream ss;
        ss << "Failed to get blocks size from filter driver for volume " << devicename << " with error " << Error::Msg(Error::UserToOS(errorfromgetattr));
        reasonifcant = ss.str();
        got = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMinor::GetVolumeBlockSizeFromSysFs(const std::string &devicename, SV_ULONGLONG &blksz, std::string &reasonifcant)
{
    bool got = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());

    std::string formattedname = devicename;
    std::replace(formattedname.begin(), formattedname.end(), '/' , '_');
    std::string bsizefile = FilterDrvSysFsDir;
    bsizefile += formattedname;
    bsizefile += "/VolumeBsize";
    DebugPrintf(SV_LOG_DEBUG, "getting block size from file %s\n", bsizefile.c_str());
    FILE *fp = fopen(bsizefile.c_str(), "r");
    if (fp) {
        if (1 == fscanf(fp, ULLSPEC, &blksz)) {
            DebugPrintf(SV_LOG_DEBUG, "Found block size as " ULLSPEC "\n", blksz);
        } else {
            int saveerrno = errno;
            std::stringstream ss;
            ss << "Failed to get blocks size from filter driver for volume " << devicename << ", as failed to read it from file " << bsizefile;
            if (ferror(fp))
               ss << ", with error code " << saveerrno << ", message: " << strerror(saveerrno);
            reasonifcant = ss.str();
            got = false;
        }
        fclose(fp);
    } else if (ENOENT == errno) {
        DebugPrintf(SV_LOG_DEBUG, "A path in file %s is not present. Considering this as volume not yet tracked. Setting block size to zero.\n", bsizefile.c_str());
        blksz = 0;
    } else {     
        int saveerrno = errno;
        std::stringstream ss;
        ss << "Failed to get blocks size from filter driver for volume " << devicename << ", as open failed on file " << bsizefile 
           << " with error code " << saveerrno << ", message: " << strerror(saveerrno);
        reasonifcant = ss.str();
        got = false;
    }
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMinor::GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, const int &errorfromgetattr, 
                                                 std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());
    bool got = true;

    if (ERR_INVALID_PARAMETER == errorfromgetattr) {
        struct stat st;
        if (0 == stat(FilterDrvProcGlobalFile.c_str(), &st))
            got = GetNumberOfBlocksInVolumeFromSysFs(devicename, nblks, reasonifcant);
        else {
            int saveerrno = errno;
            std::stringstream ss;
            ss << "Failed to get number of blocks from filter driver for volume " << devicename
               << " as stat on " << FilterDrvProcGlobalFile << " failed with error code " 
               << saveerrno << ", message: " << strerror(saveerrno);
            reasonifcant = ss.str();
            got = false;
        }
    } else {
        std::stringstream ss;
        ss << "Failed to get number of blocks from filter driver for volume " << devicename << " with error " << Error::Msg(Error::UserToOS(errorfromgetattr));
        reasonifcant = ss.str();
        got = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMinor::GetNumberOfBlocksInVolumeFromSysFs(const std::string &devicename, SV_ULONGLONG &nblks, std::string &reasonifcant)
{
    bool got = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());

    std::string formattedname = devicename;
    std::replace(formattedname.begin(), formattedname.end(), '/' , '_');
    std::string nblksfile = FilterDrvSysFsDir;
    nblksfile += formattedname;
    nblksfile += "/VolumeNblks";
    DebugPrintf(SV_LOG_DEBUG, "getting number of blocks from file %s\n", nblksfile.c_str());
    FILE *fp = fopen(nblksfile.c_str(), "r");
    if (fp) {
        if (1 == fscanf(fp, ULLSPEC, &nblks)) {
            DebugPrintf(SV_LOG_DEBUG, "Found number of blocks as " ULLSPEC "\n", nblks);
        } else {
            int saveerrno = errno;
            std::stringstream ss;
            ss << "Failed to get number of blocks from filter driver for volume " << devicename << ", as failed to read it from file " << nblksfile;
            if (ferror(fp))
               ss << ", with error code " << saveerrno << ", message: " << strerror(saveerrno);
            reasonifcant = ss.str();
            got = false;
        }
        fclose(fp);
    } else if (ENOENT == errno) {
        DebugPrintf(SV_LOG_DEBUG, "A path in file %s is not present. Considering this as volume not yet tracked. " 
                                  "Setting number of blocks to zero.\n", nblksfile.c_str());
        nblks = 0;
    } else {     
        int saveerrno = errno;
        std::stringstream ss;
        ss << "Failed to get number of blocks from filter driver for volume " << devicename << ", as open failed on file " << nblksfile 
           << " with error code " << saveerrno << ", message: " << strerror(saveerrno);
        reasonifcant = ss.str();
        got = false;
    }
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}
