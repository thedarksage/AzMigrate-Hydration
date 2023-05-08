//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : diffdatasort.cpp
// 
// Description: 
//

#include <iostream>
#include <sstream>
#include <list>

#include <ace/ACE.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_dirent.h>
#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_stdlib.h>
#include <ace/OS_NS_sys_stat.h>

#include "diffsortcriterion.h"
#include "dataprotectionexception.h"
#include "inmsafecapis.h"


typedef std::list<std::string> searchDirs_t;

void usage(char const * progName, char const * msg)
{    
    std::cout << "error - " << (0 == msg ? "" : msg) << '\n'
              << "usage: " << progName << " [-s | -e] [-p prefix] [-h] <diffdata dirs> \n"
              << "  -s             : Ignored(sorting is always based on end time)\n"
              << "  -e             : Ignored(sorting is always based on end time)\n"
              << "  -p prefix      : file name prefix to match on. If not specified sorts all files\n"
              << "  -h             : display this help message\n"
              << "  <diffdata dirs>: directories that contain the files to be sorted\n";
}

bool ParseArgs(int argc, char * argv[], searchDirs_t & searchDirs, std::string & prefix)
{    
    std::ostringstream msg;

    for (int i = 1; i < argc; ++i) {   
        if ('-' != argv[i][0]) {            
            searchDirs.push_back(std::string(argv[i]));
        } else {
            switch (argv[i][1]) {
                case 'e':       
                    break;
                case 's':            
                    break;
                case 'h':
                    usage(argv[0], 0);
                    return false;
                case 'p':
                    ++i;
                    prefix = argv[i];
                    break;
                default:
                    msg << "invalid option: " << argv[i];
                    usage(argv[0], msg.str().c_str());
                    return false;
            }
        }
    }

    return true;
}

void SortDiffData(searchDirs_t const & searchDirs, std::string const & matchPrefix)
{            
    std::ostringstream msg;  

    DiffSortCriterion::OrderedEndTime_t orderedData;   

    searchDirs_t::const_iterator bIter(searchDirs.begin());
    searchDirs_t::const_iterator eIter(searchDirs.end());
    
    for (/*empty*/; bIter != eIter; ++bIter) {
		std::string tempDir = (*bIter) + ACE_DIRECTORY_SEPARATOR_STR_A;
        ACE_DIR *dir = ACE_OS::opendir(ACE_TEXT_CHAR_TO_TCHAR(tempDir.c_str()));
        
        if (NULL == dir) {            
            // don't exit just go onto next dir as sometimes the dir
            // has not yet been created if a cluster node has not
            // become the active node
            continue;
        }    
        
        struct ACE_DIRENT *de = NULL;
                
        std::string::size_type idx;
        
        do {         
            if (NULL != (de = ACE_OS::readdir(dir))) {
				std::string entName = ACE_TEXT_ALWAYS_CHAR(de->d_name);
				if ((matchPrefix.empty() || (0 == strncmp(entName.c_str(), matchPrefix.c_str(), matchPrefix.length())))
						 && (!entName.empty() && '.' != entName[0])) {
                    std::string fileName((*bIter) + entName);
                    ACE_stat stat;
                    if (-1 == ACE_OS::stat(fileName.c_str(), &stat)) {
                        orderedData.clear();
                        msg << "unable to stat file '" << fileName << "': " << errno << '\n';                    
                        throw DataProtectionException(msg.str());
                    }      
                    std::ostringstream modifiedTime;
                    
                    modifiedTime << '_' << stat.st_ctime;
                    
                    idx = fileName.rfind(".dat");
                    if (std::string::npos == idx) {
                        orderedData.clear();
                        msg  << "invalid file format missing \".dat\" : '" << fileName << "'\n";
                        throw DataProtectionException(msg.str());
                    }
                    fileName.insert(idx, modifiedTime.str());
                    orderedData.insert(DiffSortCriterion(fileName.c_str(), std::string()));                
                }
            }
        } while(de);
        
        ACE_OS::closedir(dir);
    }
    
    DiffSortCriterion::OrderedEndTime_t::iterator iter(orderedData.begin());
    DiffSortCriterion::OrderedEndTime_t::iterator endIter(orderedData.end());

    for (/*empty*/; iter != endIter; ++iter) {
        std::string fileName((*iter).Id());
        std::string::size_type datIdx = fileName.rfind(".dat");
        std::string::size_type underscoreIdx = fileName.rfind("_");
        if (std::string::npos == datIdx || std::string::npos == underscoreIdx) {
            msg  << "bad file name '" << fileName << "'\n";
            throw DataProtectionException(msg.str());                   
        }
        fileName.erase(underscoreIdx, datIdx - underscoreIdx);
        std::cout << fileName << '\n';
    }        
}

int main(int argc, char* argv[])
{
    init_inm_safe_c_apis();
    try {             
        searchDirs_t searchDirs;

        std::string prefix;

        if (!ParseArgs(argc, argv, searchDirs, prefix)) {
            return -1;
        }

        if (searchDirs.empty()) {
            // use current directory
            searchDirs.push_back(std::string("./"));
        }
        
        SortDiffData(searchDirs, prefix);            

    } catch (DataProtectionException& dpe) {
        std::cout << "error " << dpe.what();
        return -1;
    } catch (...) {
        std::cout << "error unknown error caught\n";
        return -1;
    }
    
    return 0;
}

