
///
/// \file win/programfullpath.h
///
/// \brief
///

#ifndef PROGRAMFULLPATH_H
#define PROGRAMFULLPATH_H

#include <windows.h>

inline bool getProgramFullPath(std::string const& argv0, std::string& fullName)
{
    fullName.clear();
    char name[1024];
    if (0 != GetModuleFileName(0, name, sizeof(name) - 1)) {
        fullName = name;
    }    
    return !fullName.empty();
}

#endif // PROGRAMFULLPATH_H
