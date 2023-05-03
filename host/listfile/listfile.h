
///
/// \file listfile.h
///
/// \brief helper functions for listing files/dirs based on globbing
//

#ifndef LISTFILE_H
#define LISTFILE_H

#include <string>
#include <cstddef>
#include <sstream>
#include <vector>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>

#include "globtoregex.h"
#include "regexflags.h"
#include "extendedlengthpath.h"

/// \brief help with listing files/dirs based on globbing
struct ListFile {

    typedef std::vector<std::string> globs_t; ///< for holding the filespce broken up into its parts

    /// \brief function used to list files/dirs using globbing
    ///
    /// fileList will contain the fileName and file size (if requested). It will have the following format
    /// \n
    /// name<tab>size<newline>
    static void listFileGlob(boost::filesystem::path const& fileSpecPath,  ///< file spec to match against
                             std::string & fileList,                       ///< receives matches found
                             bool includeFileSize = true                   ///< include the file size in output true: yes, false: no
                             )
        {
            fileList.clear();

            boost::filesystem::path dotDir(".");
            boost::filesystem::path slashDir("/");
            boost::filesystem::path startDir(fileSpecPath.root_path());
            if (startDir.empty()) {
                startDir = "./"; // look in current dir
            }

            globs_t globs;

            boost::filesystem::path p(fileSpecPath);
            while (p.has_parent_path()) {
                // do not bother to push dot as that is just
                // the current dir this way the extra dots do
                // not show up in the results.
                if (dotDir != p.filename() && slashDir != p.filename()) {
                    globs.push_back(p.filename().string());
                }
                p = p.parent_path();
            }

            if (!p.has_root_path()) {
                globs.push_back(p.filename().string());
            }

            recursiveListFileGlob(startDir, globs.size() - 1, globs, fileList, includeFileSize);
        }

    /// \brief checks if the given string has any glob chars
    ///
    /// \return
    /// \li \c true if at least 1 glob char found
    /// \li \c false if no glob chars found
    static bool isGlob(std::string const & glob) ///< string to test if glob or not
        {
            return std::string::npos != glob.find_first_of("[]*?");
        }

protected:

    /// \brief recursive fucntion that does the real work of finding matches
    ///
    /// fileList will contain the fileName and file size (if requested). It will have the following format
    /// \n
    /// name<tab>size<newline>
    static void recursiveListFileGlob(boost::filesystem::path const & dir,  ///< directroy to start search
                                      std::size_t globsIdx,                 ///< current index into globs
                                      globs_t const & globs,                ///< list of the fileSpce broken up into path parts
                                      std::string& fileList,                ///< receives matches found
                                      bool includeFileSize                  ///< include file size true: yes, false: noe
                                      )
        {
            boost::filesystem::path fileSpec;

            if (globs.empty() || !isGlob(globs[globsIdx])) {
                // optimization when no globing is used for this level
                // e.g. given the following spec /directory/bar/ba*
                // there is no reason to iterate all entries under directory as only bar will match
                fileSpec = dir;
                if (!globs.empty()) {
                    fileSpec /= globs[globsIdx];
                }
                if (globs.empty() || 0 == globsIdx) {
                    // if we get here then no globing so just check if exists
                    extendedLengthPath_t extName(ExtendedLengthPath::name(fileSpec.string()));
                    if (boost::filesystem::exists(extName)) {
                        if (boost::filesystem::is_directory(extName)) {
                            // want all the entries under a directory
                            extendedLengthPathDirIter_t eIter;
                            for (extendedLengthPathDirIter_t iter(extName); iter != eIter; ++iter) {
                                if (boost::filesystem::is_regular_file(iter->status())
                                    || boost::filesystem::is_directory(iter->status())
                                    ) {
                                    // MAYBE: break this up into chunks as this can get big
                                    // if there are a lot of matches
                                    addFile(ExtendedLengthPath::nonExtName((*iter).path().string()), fileList, includeFileSize);
                                }
                            }
                        } else {
                            // wanted a specific file and it was found
                            addFile(fileSpec, fileList, includeFileSize);
                        }
                    }
                } else {
                    recursiveListFileGlob(fileSpec, globsIdx - 1, globs, fileList, includeFileSize);
                }
                return;
            }

            fileSpec = dir;
            fileSpec /= globs[globsIdx];

            std::string filespecRegx;
            Glob::globToRegex(fileSpec.string(), filespecRegx);

            boost::regex regx(filespecRegx, REGEX_FLAGS);

            extendedLengthPath_t extDirName(ExtendedLengthPath::name(dir.string()));
            if (!boost::filesystem::is_directory(extDirName)) {
                return;
            }

            extendedLengthPathDirIter_t eIter;
            for (extendedLengthPathDirIter_t iter(extDirName); iter != eIter; ++iter) {
                if (0 == globsIdx) {
                    if (boost::filesystem::is_regular_file(iter->status())
                        || boost::filesystem::is_directory(iter->status())
                        ) {
                        std::string nonExtName(ExtendedLengthPath::nonExtName((*iter).path().string()));
                        if (boost::regex_match(nonExtName, regx)) {
                            // MAYBE: break this up into chunks as this can get big if there are a lot
                            // of matches
                            addFile(nonExtName, fileList, includeFileSize);
                        }
                        if (boost::filesystem::is_directory(iter->status()) && '*' == globs[globsIdx].at(globs[globsIdx].size() -1) && boost::regex_match(nonExtName, regx)) {
                            // need to collect everything in this dir
                            for (extendedLengthPathDirIter_t lastIter((*iter).path()); lastIter != eIter; ++lastIter) {
                                if (boost::filesystem::is_regular_file((*lastIter).status())
                                    || boost::filesystem::is_directory((*lastIter).status())
                                    ) {
                                    std::string nonExtName(ExtendedLengthPath::nonExtName((*lastIter).path().string()));
                                    addFile(nonExtName, fileList, includeFileSize);
                                }
                            }
                        }
                    }
                } else {
                    // there is more to check so must be a directory
                    // as files do not have entries under them
                    if (boost::filesystem::is_directory(iter->status())) {
                        std::string nonExtName(ExtendedLengthPath::nonExtName((*iter).path().string()));
                        if (boost::regex_match(nonExtName, regx)) {
                            recursiveListFileGlob(nonExtName, globsIdx - 1, globs, fileList, includeFileSize);
                        }
                    }
                }
            }
        }

    /// \brief adds the file and its size to fileList
    ///
    /// fileList will contain the fileName and file size (if requested). It will have the following format
    /// \n
    /// name<tab>size<newline>
    static void addFile(boost::filesystem::path const& file, ///< file to add to list
                        std::string& fileList,               ///< list of files
                        bool includeFileSize                 ///< include file size true: yes, false: no
                        )
        {
            bool dir;
            std::ostringstream size;
            extendedLengthPath_t extName(ExtendedLengthPath::name(file.string()));
            if (includeFileSize) {
                try {
                    if (boost::filesystem::is_regular_file(extName)) {
                        dir = false;
                        size << boost::filesystem::file_size(extName);
                    } else {
                        dir = true;
                    }
                } catch ( std::exception const & e) {
                    // it is possible that a found file can be deleted between
                    // the time it was found and trying to get its size
                    (void)e; // not used
                    return;
                }
            }

            // NOTE: server returns file names with slashes even for windows.
            // client converts to backslahes on windows if needed
            fileList += file.string();
            if (includeFileSize) {
                fileList += '\t';
                if (!dir) {
                    fileList += size.str();
                }
            }
            fileList += "\n";
        }
};

#endif // ifndef LISTFILE_H
