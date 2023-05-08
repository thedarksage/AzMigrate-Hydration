//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : utildirbasename.cpp
//
// Description: an implementation of the functions dirname and
// basename using std::string that do not modify the passed in
// data and do not usie statically allocated buffers 
// 

#include "utildirbasename.h"

// TODO: if ever needed for windows, then the DirectorySeparatorDelim
// will need to be DirectorySeparatorDelim("\\/") to support both slash
// and back slash when searhing for the directory separator
std::string const  DirectorySeparatorDelim("/");
std::string const  DirectorySeparator("/");
    
std::string DirName(std::string const & name)
{
    if (name.empty()) {
         return std::string(".");
    }
    
    std::string::size_type endIdx = name.size() - 1;
    
    // TODO: if ever ported to windows then this needs an extra check
    // while (name[endIdx] == DirectorySeparatorDelim[0] || name[endIdx] == DirectorySeparator[1]) {
    while (name[endIdx] == DirectorySeparatorDelim[0]) {
        --endIdx;
    }

    if (std::string::npos == endIdx) {
        return DirectorySeparator;
    }

    std::string::size_type idx = name.substr(0, endIdx).rfind(DirectorySeparatorDelim);
    if (std::string::npos == idx) {
        return std::string(".");
    }

    if (0 == idx) {
        return DirectorySeparator;
    }
        
    return name.substr(0, idx);

}

std::string DirName(char const * name)
{
    if (0 == name) {
        return std::string(".");
    }
    
    std::string const s(name);
    return DirName(s);    
}

std::string BaseName(std::string const & name)
{
    if (name.empty()) {
        return std::string(".");
    }
    
    std::string::size_type endIdx = name.size() - 1;

    // TODO: if ever ported to windows then this needs an extra check
    // while (name[endIdx] == DirectorySeparatorDelim[0] || name[endIdx] == DirectorySeparator[1]) {
    while (name[endIdx] == DirectorySeparatorDelim[0]) {
        --endIdx;
    }

    if (std::string::npos == endIdx) {
        return DirectorySeparator;
    }

    std::string::size_type idx =  name.substr(0, endIdx).rfind(DirectorySeparatorDelim);
    if (std::string::npos == idx) {
        return name;
    }
    
    if (name.size() == 1) {
        return name;
    }

    return name.substr(idx + 1, endIdx);
}

std::string BaseName(char const * name)
{
    if (0 == name) {
        return std::string(".");
    }
    
    std::string const s(name);
    return BaseName(s);    
}
