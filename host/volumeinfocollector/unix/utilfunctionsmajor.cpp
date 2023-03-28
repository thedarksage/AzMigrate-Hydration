#include <string>
#include <vector>
#include "utilfunctionsmajor.h"
#include "utildirbasename.h"
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include "localconfigurator.h"
#include "portablehelpersmajor.h"

bool parseSparseFileNameAndVolPackName(const std::string lineinconffile, std::string &sparsefilename, std::string &volpackname)
{
    bool bretval = false;
    const std::string DELIM_STR = "--/dev/volpack";
    const std::string ACTUAL_DELIM_STR = "--";
    LocalConfigurator localConfigurator;
    size_t found = 0;
    if(localConfigurator.IsVolpackDriverAvailable())
        found = lineinconffile.find(DELIM_STR);
    else
        found = lineinconffile.find(ACTUAL_DELIM_STR);

    if (std::string::npos != found)
    {
        sparsefilename = lineinconffile.substr(0, found);
        size_t volindex = found + ACTUAL_DELIM_STR.length();
        volpackname = lineinconffile.substr(volindex);
        bretval = true;
    }

    return bretval;
}


bool GetSymbolicLinksAndRealName(std::string const & deviceName,
                                                      VolumeInfoCollectorInfo::SymbolicLinks_t & symbolicLinks,
                                                      std::string & realName)
{
    struct stat64 volStat;

    realName = deviceName;

    if (-1 == lstat64(deviceName.c_str(), &volStat)) {
        return false;
    }

    if (!S_ISLNK(volStat.st_mode)) {
        return true;
    }

    std::string name(deviceName);

    char origDir[MAXPATHLEN];

    if (0 == getcwd(origDir, sizeof(origDir))) {
        return false;
    }

    boost::shared_ptr<void> resetToOrigDir(static_cast<void*>(0), boost::bind(chdir, origDir));

    ssize_t len;

    char symlnk[MAXPATHLEN];
    char curDir[MAXPATHLEN];

    do {
        if (-1 == chdir(DirName(name).c_str())) {
            return false;
        }
        if (0 == getcwd(curDir, sizeof(curDir))) {
            return false;
        }
        len = readlink(BaseName(name).c_str(), symlnk, sizeof(symlnk) - 1);
        if (-1 == len) {
            // end of links this is the real name
            realName = curDir + std::string(UNIX_PATH_SEPARATOR) + BaseName(name);
            break;
        }
        symlnk[len] = '\0';
        // make sure to add to symbolic link before setting name to symlnk
        symbolicLinks.insert(curDir + std::string(UNIX_PATH_SEPARATOR) + BaseName(name));
        name = symlnk;
    } while (true);

    return true;
}

bool AreDigits(const char *p)
{
    bool baredigits = false;
    
    if (*p)
    {
        baredigits = true;
        while (*p)
        {
            if (!isdigit(*p))
            {
                baredigits = false;
                break;
            }
            p++;
        }
    }

    return baredigits;
}

bool IsLittleEndian(void)
{
    unsigned int i = 1;
    unsigned char *byte = (unsigned char *)&i;

    bool bislittle = false;
    if (*byte == 1)
    {
        bislittle = true;
    }
 
    return bislittle;
}

 
void swap16bits(uint16_t *pdata)
{
    uint16_t x = *pdata;
    *pdata = ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8));
}
