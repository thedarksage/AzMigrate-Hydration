//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : utildirbasename.h
// 
// Description: an implementation of the functions dirname and
// basename using std::string that do not modify the passed in
// data and do not usie statically allocated buffers 
// 

#ifndef UTILDIRBASENAME_H
#define UTILDIRBASENAME_H

#include <string>

std::string DirName(char const * name);
std::string DirName(std::string const & name);
std::string BaseName(char const *  name);
std::string BaseName(std::string const & name);


#endif // ifndef UTILDIRBASENAME_H
