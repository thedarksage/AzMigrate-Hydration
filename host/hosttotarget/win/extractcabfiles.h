
///
/// \file extractcabfiles.h
///
/// \brief
///

#ifndef EXTRACTCABFILES_H
#define EXTRACTCABFILES_H

#include<string>
#include <set>
#include <offreg.h>

#include "scopeguard.h"
#include "errorresult.h"

namespace HostToTarget {

    typedef std::set<std::string> extractFiles_t;

    struct CabExtractInfo {
        std::string m_path;
        std::string m_name;
        std::string m_destination;
        extractFiles_t m_extractFiles;
    };

    ERROR_RESULT copyDriverFile(HostToTargetInfo const& info, ORHKEY hiveKey, std::string const& driverFileName, bool x64);

    bool extractCabFiles(CabExtractInfo* cabInfo);
    bool getCsdVersion(ORHKEY hiveKey, std::wstring& csdVersion);
    bool getCabFilePath(std::string const& systemVolume, std::string const& cab, bool x64, std::string& cabFilePath);
    bool extractDriverFile(std::string const& systemVolume, std::string const& cab, bool x64, std::string const& driverFileName);

    int getSpNumber(std::wstring& csdVersion);

} // namespace HostToTarget



#endif // EXTRACTCABFILES_H
