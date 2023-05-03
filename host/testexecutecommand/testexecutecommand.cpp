//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : test-volumeinfocollector.cpp
// 
// Description: 
// 

#include "executecommand.h"
#include "logger.h"
#include <iostream>
#include "portable.h"

void usage()
{
  std::cout << "Usage: testexecutecommand 'cmd1' 'cmd2' ..." << '\n';
}

int main(int argc, char* argv[])
{    
  try {
    SetLogFileName("testexecutecommand.log");

    if (1 == argc ) {
      usage();
      return false;
    }
    int argindex = 0;
    while (argindex < argc - 1) {
    argindex++;
    std::cout << '\n' << "EXECUTECOMMAND argindex:" << argindex << '\n';
    std::stringstream results;
    if (!executePipe(argv[argindex], results)) {
      DebugPrintf(SV_LOG_ERROR,"Executing %s failed with errno = %d\n", argv[argindex], errno);
    }

    if (results.str().empty()) {
      DebugPrintf(SV_LOG_DEBUG,"%s has returned empty output\n", argv[argindex]);
    }
    std::cout << "Output of " << argv[argindex] << ":" << '\n';
    while (!results.eof())
    {
      std::string line;
      std::getline(results, line);
      if (line.empty())
      {
        break;
      }
      std::cout << line << '\n';
    }
  }
  } 
  catch (std::exception& e) {
    std::cout << "caught " << e.what() << '\n';
  } 
  catch (...) {
    std::cout << "caught unknow exception\n";
  }
  return 0;
}

