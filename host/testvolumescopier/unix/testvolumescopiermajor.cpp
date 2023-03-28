#include <iostream>
#include <string>
#include <sstream>
#include "volumereporter.h"
#include "logger.h"
#include "portablehelpers.h"
#include "localconfigurator.h"


bool DoPostCopyOperations(const std::string &sourcename, const std::string &targetname, 
                          const VolumeReporter::VolumeReport_t &vr)
{
    bool bdone = true;

    if (vr.m_IsMounted && !vr.m_FileSystem.empty() && !vr.m_Mountpoint.empty())
    {
        std::cout << "Enter a mount point to mount target\n";
        std::string mpt;
        std::cin >> mpt;
        if (!mpt.empty())
        {
            if (SVMakeSureDirectoryPathExists(mpt.c_str()).failed())
            {
                std::cerr << "could not make directory " << mpt << '\n';
                bdone = false;
            }
            else if (UnhideDrive_RW(targetname.c_str(), mpt.c_str(), vr.m_FileSystem.c_str()).failed())
            {
                std::cerr << "Failed to mount " << targetname << " on mount point " << mpt << " with file system " << vr.m_FileSystem << '\n';
                bdone = false;
            }
        }
        else
        {
            std::cerr << "specify proper mountpoint\n";
            bdone = false;
        }
    }

    return bdone;
}
