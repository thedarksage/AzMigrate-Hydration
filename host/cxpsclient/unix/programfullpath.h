
///
/// \file unix/programfullpath.h
///
/// \brief
///

#ifndef PROGRAMFULLPATH_H
#define PROGRAMFULLPATH_H

#include <string>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

inline void removeRelativePortions(std::string& fullName)
{
    boost::filesystem::path dotDot("..");
    boost::filesystem::path dot(".");
    boost::filesystem::path tmpName;
    boost::filesystem::path name(fullName);

    boost::filesystem::path::iterator iter(name.begin());
    boost::filesystem::path::iterator iterEnd(name.end());
    for (/* empty*/; iter != iterEnd; ++iter) {
        if (dotDot == *iter) {
            tmpName.remove_filename();
            // since remove_filename does not actually remove trailing slashes need
            // this test to remove them if they exist
            if (dot == tmpName.filename()) {
                tmpName.remove_filename();
            }
        } else if (dot != *iter) {
            tmpName /= *iter;
        }
    }
    fullName = tmpName.string();
}

inline bool getProgramFullPath(std::string const& argv0, std::string& fullName)
{
    // NOTE: this assume that argv0 is set to argv[0]
    // and argv[0] was set by the system to something reasonable
    fullName.clear();
    char path[1024];
    if ('/' != argv0[0]) {
        if (0 != getcwd(path, sizeof(path))) {
            fullName = path;
            fullName += '/';
            fullName += argv0;            
        }
    } else {
        fullName = argv0;
    }

    if (!fullName.empty()) {
        removeRelativePortions(fullName);
    }
        
    return !fullName.empty();
}

#endif // PROGRAMFULLPATH_H
