#include "devdidtracker.h"
#include <cstring>

bool DevDidTracker::IsDevDidName(std::string const & name) const 
{
    bool bisdevdidname = false;
    if (0 == strncmp(name.c_str(), m_Dir.c_str(), strlen(m_Dir.c_str())))
    {
        bisdevdidname = true;
    }
    return bisdevdidname;
}
