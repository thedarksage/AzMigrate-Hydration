
///
/// \file finddelete.h
///
/// \brief implements a helper class for client delete file requests
///

#ifndef FINDDELETE_H
#define FINDDELETE_H

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/system/error_code.hpp>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/function.hpp>

#include "extendedlengthpath.h"
#include "regexflags.h"
#include "globtoregex.h"

/// \brief helper class for deleting files and/or dirs when deleteFile is issued by a client
///
/// this class is used under the covers when one of the Client::deleteFile requests is issued
///
/// the Client APIs are described in client.h but shown here to help describe how FindDelete works
///
/// Client::deleteFile(std::string const& names, DELETE_FILE_MODE mode = FILES_ONLY)
///
/// \li calls deleteFile(dirs, std::string(), mode)
///
/// Client::deleteFile(std::string const& names, std::string const& fileSpec, int mode = FILES_ONLY)
///
/// \li \c  dirs    : semi-colon spearated list of names to be used during the deletion process
/// \li \c  fileSpec: semi-colon (':') list of file specs (glob supported syntax) to restrict which
///                   files/dirs will be deleted. It represents the part of the name after the last
///                   '/' or '\\', as such it should not have '/' nor '\\' in it.
/// \li \c  mode    : mode to be used while deleteing available modes are FILES_ONLY, RECURSE. They
///                   can be ORed togehter.
///
/// \verbatim   for each name in names                                     \endverbatim
/// \verbatim    if name is a file                                         \endverbatim
/// \verbatim      delete name  (independent of fileSpec value)            \endverbatim
/// \verbatim    else if name is dir                                       \endverbatim
/// \verbatim      apply the following action based on mode and fileSpec   \endverbatim
///
/// \verbatim mode                       filespec   action                                    \endverbatim
/// \verbatim -------------------------  ---------  -------------------------------------     \endverbatim
/// \verbatim FILES_ONLY                 empty      delete all files under given dir          \endverbatim
/// \verbatim FILES_ONLY                 non-empty  delete matching files under given dir     \endverbatim
/// \verbatim FILES_ONLY | RECURSE_DIRS  empty      delete all files under dir and            \endverbatim
/// \verbatim                                       subdirs                                   \endverbatim
/// \verbatim FILES_ONLY | RECURSE_DIRS  non-empty  delete matching files under dir and       \endverbatim
/// \verbatim                                       subdirs                                   \endverbatim
/// \verbatim RECURSE_DIRS               empty      delete everything under dir and dir       \endverbatim
/// \verbatim                                       itself                                    \endverbatim
/// \verbatim RECURSE_DIRS               non-empty  delete matching files and subdirs         \endverbatim
/// \verbatim                                       (delete everything under matching         \endverbatim
/// \verbatim                                       subdirs)                                  \endverbatim
/// \verbatim any other value            n/a        report invalid mode                       \endverbatim
///
class FindDelete
{
public:
#define MAKE_GET_FULL_PATH_CALLBACK_MEM_FUN(getFullPathMemberFunction, getFullPathPtrToObj) \
    boost::bind(getFullPathMemberFunction, getFullPathPtrToObj, _1, _2)

    typedef boost::function<void (std::string const& name, boost::filesystem::path& fullPath)> getFullPath_t; ///< getFullPath callback type

#define MAKE_CLOSE_FILE_CALLBACK_MEM_FUN(closeFileMemberFunction, closeFilePtrToObj) \
    boost::bind(closeFileMemberFunction, closeFilePtrToObj, _1)

    typedef boost::function<void (boost::filesystem::path const& name)> closeFile_t; ///< closeFile callback type

    /// \brief availble modes for deleteFile request
    enum FIND_DELETE_MODE {
        FILES_ONLY = 0x01,    ///< delete files only
        RECURSE_DIRS = 0x02   ///< recurse through subdirs
    };

    /// \brief get full path used when there is no need to prepend a full path
    static void defaultGetFullPath(std::string const& name, boost::filesystem::path& fullPath)
        {
            fullPath = name;
        }

    /// \brief default closeFile function used when closeFile is not needed
    static void defaultCloseFile(boost::filesystem::path const& name)
        {
            (void)name; // not used
            return;
        }

    static std::string remove(std::string names,
                              getFullPath_t getFullPath = FindDelete::defaultGetFullPath,
                              closeFile_t closeFile = FindDelete::defaultCloseFile)
        {
            return remove(names, std::string(), FILES_ONLY, getFullPath, closeFile);
        }

    static std::string remove(std::string names,
                              std::string const& fileSpec,
                              int mode,
                              getFullPath_t getFullPath = defaultGetFullPath,
                              closeFile_t closeFile = defaultCloseFile)
        {
            switch (mode) {
                case FILES_ONLY:
                case RECURSE_DIRS:
                case FILES_ONLY | RECURSE_DIRS:
                    break;

                default:
                {
                    std::string errStr("invalid mode: ");
                    errStr += boost::lexical_cast<std::string>(mode);
                    return errStr;
                }
            }

            if (fileSpec.empty()) {
                return removeFiles(names, mode, getFullPath, closeFile);
            } else {
                return removeFiles(names, fileSpec, mode, getFullPath, closeFile);
            }
        }

protected:
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;

    static void cleanFiles(extendedLengthPath_t const& name,
                           std::string const& fileSpec,
                           int mode,
                           closeFile_t closeFile
                           )
        {
            std::string filespecRegx;

            boost::shared_ptr<boost::regex> regx;

            if (!fileSpec.empty()) {
                Glob::globToRegex(fileSpec, filespecRegx);
                regx.reset(new boost::regex(filespecRegx, REGEX_FLAGS));
            }

            std::string nonExtName;

            // handles all empty fileSpec cases
            if (!boost::filesystem::exists(name)) {
                return;
            }
            if (boost::filesystem::is_regular_file(name)) {
                closeFile(ExtendedLengthPath::nonExtName(name.string()));
                boost::filesystem::remove(name);
            } else if (boost::filesystem::is_directory(name)) {
                extendedLengthPathDirIter_t endIter;
                for (extendedLengthPathDirIter_t iter(name); iter != endIter; ++iter) {
                    if (boost::filesystem::is_regular_file(iter->status())) {
                        closeFile(ExtendedLengthPath::nonExtName((*iter).path().string()));
                        if (fileSpec.empty() || boost::regex_match(ExtendedLengthPath::nonExtName((*iter).path().filename()), *regx)) {
                            boost::filesystem::remove((*iter).path());
                        }
                    } else if (boost::filesystem::is_directory(iter->status())) {
                        if (RECURSE_DIRS & mode) {
                            if (fileSpec.empty() || boost::regex_match(ExtendedLengthPath::nonExtName((*iter).path().filename()), *regx)) {
                                // delete everything under this dir, so no fileSpec
                                cleanFiles((*iter).path(), std::string(), mode, closeFile);
                            }
                        }
                    }
                }
                if (!(FILES_ONLY & mode)) {
                    if (fileSpec.empty() || boost::regex_match(ExtendedLengthPath::nonExtName(name.filename()), *regx)) {
                        boost::filesystem::remove(name); // dir so no need to make sure it is closed
                    }
                }
            }

        }

    static std::string removeFiles(std::string const& files,
                                   int mode,
                                   getFullPath_t getFullPath = defaultGetFullPath,
                                   closeFile_t closeFile = defaultCloseFile
                                   )
        {
            std::string errStr;
            boost::char_separator<char> sep(";");
            tokenizer_t tokens(files, sep);
            tokenizer_t::iterator filesIter(tokens.begin());
            tokenizer_t::iterator filesIterEnd(tokens.end());
            for (/* empty */; filesIter != filesIterEnd; ++filesIter) {
                try {
                    boost::filesystem::path fullName;
                    getFullPath(*filesIter, fullName);
                    extendedLengthPath_t extName(ExtendedLengthPath::name(fullName.string()));
                    cleanFiles(extName, std::string(), mode, closeFile);
                } catch (std::exception const & e) {
                    errStr +=  *filesIter;
                    errStr += " failed: ";
                    errStr += e.what();
                }
            }

            return errStr;
        }

    static std::string removeFiles(std::string const& files,
                                   std::string const& fileSpec,
                                   int mode,
                                   getFullPath_t getFullPath,
                                   closeFile_t closeFile
                                   )
        {
            std::string errStr;

            boost::char_separator<char> sep(";");
            tokenizer_t fileTokens(files, sep);
            tokenizer_t::iterator filesIter(fileTokens.begin());
            tokenizer_t::iterator filesIterEnd(fileTokens.end());
            for (/* empty */; filesIter != filesIterEnd; ++filesIter) {
                tokenizer_t fileSpecTokens(fileSpec, sep);
                tokenizer_t::iterator fileSpecIter(fileSpecTokens.begin());
                tokenizer_t::iterator fileSpecIterEnd(fileSpecTokens.end());
                for (/* empty */; fileSpecIter != fileSpecIterEnd; ++fileSpecIter) {
                    try {
                        boost::filesystem::path fullName;
                        getFullPath(*filesIter, fullName);
                        extendedLengthPath_t extName(ExtendedLengthPath::name(fullName.string()));
                        cleanFiles(extName, *fileSpecIter, mode, closeFile);
                    } catch (std::exception const & e) {
                        errStr += *filesIter;
                        errStr += " - ";
                        errStr += *fileSpecIter;
                        errStr += " failed: ";
                        errStr += e.what();
                    }
                }
            }

            return errStr;
        }

private:

};

#endif // FINDDELETE_H
