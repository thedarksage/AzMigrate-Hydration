#ifndef __LOG_H_
#define __LOG_H_

#include <iostream>
#include <fstream>
#include <string>
#include <time.h>
#include <stdio.h>
#define MAX_LOG_SIZE 20*1024*1024
#include "Mutex.h"

using namespace std;

#ifdef WIN32
#define LOG_FILE "../var/Message.log"
#else
#define LOG_FILE "/home/svsystems/var/Message.log"
#endif

/*
#ifdef __WIN32
//Supported with visual studio 2005
#define DR_INFO(module,strFormat,...) \
		Logger::Log(__FILE__,__LINE__,0,module,strFormat,__VA_ARGS__);

#define DR_WARNING(module,strFormat,...) \
		Logger::Log(__FILE__,__LINE__,1,module,strFormat,__VA_ARGS__);

#define DR_ERROR(module,strFormat,...) \
		Logger::Log(__FILE__,__LINE__,2,module,strFormat,__VA_ARGS__);
#else

#define DR_INFO(module,strFormat,vlist...) \
		Logger::Log(__FILE__,__LINE__,0,module,strFormat , ## vlist);

#define DR_WARNING(module,strFormat...) \
		Logger::Log(__FILE__,__LINE__,1,module,strFormat , ## vlist);

#define DR_ERROR(module,strFormat...) \
		Logger::Log(__FILE__,__LINE__,2,module,strFormat , ## vlist);
#endif
*/

class Logger{
	static ofstream infile;

	static bool bCreated;

	static Mutex mutex;

public:
	
	static void OpenLog()
	{
		if(!bCreated)
		{
			infile.open(LOG_FILE, ios::app);
			bCreated = true;
		}
	}

    static  long getFileSize();
                                                                                                                              
	static void Log(char *fileName,int lineNo,int intPriority,int intModule, const char* strEnter,...);

	static void Log(int intPriority,int intModule, const char* strEnter,...);

	static void Log(int intPriority,int intModule, long intEnter);

	static void CloseLog();
};

#endif
