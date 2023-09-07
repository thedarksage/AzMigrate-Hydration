//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : executecommand.h
// 
// Description: executes given command by fork, exec and returns its output
// 
// returns:
//   * true on success, results will hold output from command
//     it is up to the caller to determine if the command itself succeeded
//     or failed.
//
//   * false on errors using pipe
//
// 

#ifndef EXECUTECOMMAND_H
#define EXECUTECOMMAND_H

#include <sstream>
#include <string>

#ifdef SV_WINDOWS
#define FUNCTION_NAME __FUNCTION__
#else
#define FUNCTION_NAME __func__
#endif /* SV_WINDOWS */


bool executePipe(std::string const & command, std::stringstream & results);

#endif // ifndef EXECUTECOMMAND_H

