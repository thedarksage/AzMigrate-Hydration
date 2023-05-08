
///
/// \file extractcabfiles.cpp
///
/// \brief
///

#include <windows.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <fdi.h>
#include <string>
#include <set>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "scopeguard.h"
#include "extractcabfiles.h"

namespace HostToTarget {

    // callback I/O functions needed for FDI routines
    FNALLOC(memAlloc)
    {
        return malloc(cb);
    }
    FNFREE(memFree)
    {
        free(pv);
    }
    FNOPEN(fileOpen)
    {
        return _open(pszFile, oflag, pmode);
    }
    FNREAD(fileRead)
    {
        return _read(hf, pv, cb);
    }
    FNWRITE(fileWrite)
    {
        return _write(hf, pv, cb);
    }
    FNCLOSE(fileClose)
    {
        return _close(hf);
    }
    FNSEEK(fileSeek)
    {
        return _lseek(hf, dist, seektype);
    }

    int extractCabFile(CabExtractInfo* cabInfo, char const* fileName)
    {
        if (0 == cabInfo->m_extractFiles.erase(fileName)) {
            return 0; // do not extract
        }
        std::string out(cabInfo->m_destination + fileName);
        return fileOpen((LPSTR)out.c_str(), _O_BINARY | _O_CREAT | _O_WRONLY | _O_SEQUENTIAL, _S_IREAD | _S_IWRITE);
    }

    std::string fdiErrorToString(FDIERROR err)
    {
        switch (err) {
            case FDIERROR_NONE:
                return "No error";

            case FDIERROR_CABINET_NOT_FOUND:
                return "Cabinet not found";

            case FDIERROR_NOT_A_CABINET:
                return "Not a cabinet";

            case FDIERROR_UNKNOWN_CABINET_VERSION:
                return "Unknown cabinet version";

            case FDIERROR_CORRUPT_CABINET:
                return "Corrupt cabinet";

            case FDIERROR_ALLOC_FAIL:
                return "Memory allocation failed";

            case FDIERROR_BAD_COMPR_TYPE:
                return "Unknown compression type";

            case FDIERROR_MDI_FAIL:
                return "Failure decompressing data";

            case FDIERROR_TARGET_FILE:
                return "Failure writing to target file";

            case FDIERROR_RESERVE_MISMATCH:
                return "Cabinets in set have different RESERVE sizes";

            case FDIERROR_WRONG_CABINET:
                return "Cabinet returned on fdintNEXT_CABINET is incorrect";

            case FDIERROR_USER_ABORT:
                return "User aborted";

            default:
                return "Unknown error";
        }
    }

    FNFDINOTIFY(fdiNotifyCallback)
    {
        switch (fdint) {
            case fdintCOPY_FILE: // file to be copied
                return extractCabFile((CabExtractInfo*)pfdin->pv, pfdin->psz1);
            case fdintCLOSE_FILE_INFO: // close the file, set relevant info
                fileClose(pfdin->hf);
                return TRUE;
            default:
                break;
        }
        return FALSE;
    }
    void fdiCleanup(HFDI hfdi)
    {
        if (NULL != hfdi) {
            FDIDestroy(hfdi);
        }
    }

    bool extractCabFiles(CabExtractInfo* cabInfo)
    {
        HFDI   hfdi;
        ERF    erf;

        // set up context
        hfdi = FDICreate(memAlloc, memFree, fileOpen, fileRead, fileWrite, fileClose, fileSeek, cpuUNKNOWN, &erf);
        if (NULL == hfdi) {
            // set error
            return false;
        }
        ON_BLOCK_EXIT(&fdiCleanup, hfdi);

        // make sure it is a cab file
        FDICABINETINFO fdici;
        std::string cab(cabInfo->m_path + cabInfo->m_name);
        int hf = fileOpen((LPSTR)cab.c_str(), _O_BINARY | _O_RDONLY | _O_SEQUENTIAL, 0);
        if (-1 == hf) {
            // set error
            return false;
        }
        BOOL rc = FDIIsCabinet(hfdi, hf, &fdici);
        _close(hf);
        if (!rc) {
            return false;
        }

        // extract files
        if (!FDICopy(hfdi, (LPSTR)(cabInfo->m_name.c_str()), (LPSTR)(cabInfo->m_path.c_str()), 0, fdiNotifyCallback, NULL, cabInfo)) {
            // set error
            return false;
        }
        return cabInfo->m_extractFiles.empty(); // if not all files were extracted need to try a different cab for remainging files
    }

    bool getCsdVersion(ORHKEY hiveKey, std::wstring& csdVersion)
    {
        DWORD rc;
        DWORD type;
        std::vector<wchar_t> data(1024);
        DWORD dataLength = (DWORD)data.size();
        do {
            data.resize(dataLength);
            rc = ORGetValue(hiveKey, L"Microsoft\\Windows NT\\CurrentVersion", L"CSDVersion", &type, &data[0], &dataLength);
        } while (ERROR_MORE_DATA == rc);
        if (ERROR_SUCCESS != rc) {
            return false;
        }
        csdVersion = &data[0];
        return true;
    }

    bool getCabFilePath(std::string const& systemVolume, std::string const& cab, bool x64, std::string& cabFilePath)
    {
        std::string cabDir(systemVolume);
        cabDir += "windows\\driver cache\\";
        if (x64) {
            cabFilePath = cabDir;
            cabFilePath += "amd64\\";
            std::string cabFullPath(cabFilePath + cab);
            try {
                if (boost::filesystem::exists(cabFullPath)) {
                    return true;
                }
            } catch (...) {
            }
            // TODO: could it be something like x64 for intel chips?
        } else {
            cabFilePath = cabDir;
            cabFilePath += "i386\\";
            std::string cabFullPath(cabFilePath + cab);
            try {
                if (boost::filesystem::exists(cabFullPath)) {
                    return true;
                }
            } catch (...) {
            }
        }
        cabFilePath = cabDir;
        std::string cabFullPath(cabFilePath + cab);
        try {
            return boost::filesystem::exists(cabFilePath);
        } catch (...) {
            return false;
        }
    }

    int getSpNumber(std::wstring& csdVersion)
    {
        std::wstring::size_type idx = csdVersion.size() - 1;
        while (L' ' == csdVersion[idx]) {
            --idx;
        }
        while (L' ' != csdVersion[idx]) {
            --idx;
        }
        ++idx;
        try {
            return boost::lexical_cast<int>(csdVersion.substr(idx));
        } catch (...) {
            return -1;
        }
    }

    ERROR_RESULT copyDriverFile(HostToTargetInfo const& info, ORHKEY hiveKey, std::string const& driverFileName, bool x64)
    {
        std::wstring csdVersion;
        if (!getCsdVersion(hiveKey, csdVersion)) {
            return ERROR_RESULT(ERROR_INTERNAL_REG_GET_VALUE, ERROR_SUCCESS, "Microsoft\\Windows NT\\CurrentVersion\\CSDVersion");
        }
        if (boost::algorithm::icontains(csdVersion, L"service pack")) {
            int spNumber = getSpNumber(csdVersion);
            while (spNumber > 0) {
                std::string spFile("sp");
                spFile += boost::lexical_cast<std::string>(spNumber);
                spFile += ".cab";
                if (extractDriverFile(info.m_systemVolume, spFile, x64, driverFileName)) {
                    return ERROR_RESULT(ERROR_OK, ERROR_SUCCESS);
                }
                --spNumber;
            }
        }
        if (extractDriverFile(info.m_systemVolume, "driver.cab", x64, driverFileName)) {
            return ERROR_RESULT(ERROR_OK, ERROR_SUCCESS);
        }
        return ERROR_RESULT(ERROR_INTERNAL_INTEL_IDE_SYS, ERROR_SUCCESS);
    }

    bool extractDriverFile(std::string const& systemVolume, std::string const& cab, bool x64, std::string const& driverFileName)
    {
        std::string driverDestination(systemVolume);
        driverDestination += "windows\\system32\\drivers\\";
        driverDestination += driverFileName;
        try {
            if (boost::filesystem::exists(driverDestination)) {
                return true; // file already installed just use the one that is there
            }
        } catch (...) {
        }

        CabExtractInfo cabInfo;
        cabInfo.m_name = cab;
        if (!getCabFilePath(systemVolume, cab, x64, cabInfo.m_path)) {
            return false;
        }
        cabInfo.m_destination = systemVolume;
        cabInfo.m_destination += "tmpdriverextract\\";
        boost::filesystem::remove_all(cabInfo.m_destination);
        boost::filesystem::create_directory(cabInfo.m_destination);
        cabInfo.m_extractFiles.insert(driverFileName);
        if (!extractCabFiles(&cabInfo)) {
            return false;
        }
        try {
            boost::filesystem::remove(driverDestination);
            boost::filesystem::rename(cabInfo.m_destination + driverFileName, driverDestination);
        } catch (...) {
        }
        boost::filesystem::remove_all(cabInfo.m_destination);
        return true;
    }

} // namespace HostToTarget
