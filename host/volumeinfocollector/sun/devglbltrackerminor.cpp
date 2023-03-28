#include "devglbltracker.h"
#include <cstring>

bool DevGlblTracker::IsDevGlblName(std::string const & name) const 
{
    bool bisdevglblname = false;
    if (0 == strncmp(name.c_str(), m_Dir.c_str(), strlen(m_Dir.c_str())))
    {
        bisdevglblname = true;
    }
    return bisdevglblname;
}
