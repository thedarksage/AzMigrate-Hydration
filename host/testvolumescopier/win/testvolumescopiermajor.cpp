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
    std::string dev_name(targetname);
    std::string sparsefile;
    LocalConfigurator localConfigurator;
    bool isnewvirtualvolume = false;
    if(!localConfigurator.IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
    {
        dev_name = sparsefile;
    }

    if(!isnewvirtualvolume)
    {
        if (UnhideDrive_RW(targetname.c_str(), targetname.c_str()).failed())
        {
            bdone = false;
            std::cerr << "Failed to unhide " << targetname << " in read write mode\n";
        }
    }

    return bdone;
}
