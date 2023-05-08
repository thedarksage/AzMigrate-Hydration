#include "Log.h"
#include "util.h"
#include <stdarg.h>
#include <cstring>
#include <ace/OS_NS_Thread.h>
#include <ace/OS_NS_unistd.h>
#include "inmsafecapis.h"

long Logger::getFileSize()
{      
      int size;  
      std::ifstream f;

      f.open(LOG_FILE, std::ios_base::binary | std::ios_base::in);

      if (!f.good() || !f.is_open())
        {
          return 0;
        }
	else
        {
         f.seekg(0, std::ios_base::beg);
         std::ifstream::pos_type begin_pos = f.tellg();
         f.seekg(0, std::ios_base::end);
         size  = f.tellg() - begin_pos;
        }
      f.close();
    return size;
}

void Logger::CloseLog(){
        mutex.lock();
	if(infile.is_open()){
	infile.close();
        }
        mutex.unlock(); 
}

void Logger::Log(char *fileName,int lineNo,int intPriority,int intModule, const char* strEnter,...)
{
	long	cTime;
	string	currTime;
	string	strPriority;
	string	strModule;

	cTime		= time(NULL);
	currTime	= convertTimeToString(cTime);

	switch (intPriority)
	{
		case 0:
			strPriority = "INFO";
			break;

		case 1:
			strPriority = "WARNING";
			break;

		case 2:
			strPriority = "ERROR";
			break;

		case 3:
			strPriority = "STATUS";
			break;

		default:
			strPriority = "UNKNOWN BUG";
			break;
	}

	switch (intModule)
	{
		case 0:
			strModule = "FILE REPLICATION";
			break;

		case 1:
			strModule = "SCHEDULAR";
			break;

		case 2:
			strModule = "SNAPSHOT";
			break;

		default:
			strModule = "DEFAULT";
			break;
	}

	va_list arglist;
	va_start(arglist, strEnter);     /* Initialize variable arguments. */

	//char szTmp[1024] = "";
	string strFormated = "";

	long start	= 0;
	unsigned int i		=0;

	for(i = 0; i < strlen(strEnter); i++)
	{
		if(strEnter[i] == '%')
		{
			char szPre[1024];
			const char *ptr = strEnter;

			ptr = ptr + start;
			inm_strncpy_s(szPre,ARRAYSIZE(szPre), ptr, i - start);
			szPre[i - start] = '\0';
			strFormated += szPre;

			int iTmp		= 0;
			long lTmp		= 0;
			float fTmp		= 0;
			char *strTmp	;

			char szBuffer[1024];

			switch(strEnter[i+1])
			{
			case 'd':
				iTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", iTmp);
				strFormated += szBuffer;
				break;

			case 'f':
				fTmp = (float) va_arg(arglist, double);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%f", fTmp);
				strFormated += szBuffer;
				break;

			case 's':
				strTmp = va_arg(arglist, char*);
				strFormated += strTmp;
				break;

			case 'l':
				if(strEnter[i + 2] == 'd')
				{
					lTmp = va_arg(arglist, long);
					inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%ld", lTmp);
					strFormated += szBuffer;
					i++;
					break;
				}

				if(strEnter[i + 2] == 'f')
				{
					fTmp = (float) va_arg(arglist, double);
					inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", fTmp);
					strFormated += szBuffer;
					i++;
					break;
				}

			case 'c':
				iTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", iTmp);
				strFormated += szBuffer;
				break;

			case 'x':
				lTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", lTmp);
				strFormated += szBuffer;
				break;
			}

			i++;
			start = i + 1;
		}
	}

	if(start != i)
	{
		const char *ptr = strEnter;
		ptr += start;
		strFormated += ptr;
	}
       
       long size = getFileSize();
       
       if(size > MAX_LOG_SIZE)
       {
        CloseLog();
        infile.open(LOG_FILE, ios::trunc);

       }  


	mutex.lock();
	
//	infile << "\t"<< currTime << "\t"<< "|" "\t"<< strPriority << "\t"<< "|" << "\t"<< strModule << "\t\t"<< "|" << "\t\t"<< szTmp << "\t\t\t\t"<< "|";
	infile	<< ACE_OS::getpid()   << "|"
		    << ACE_OS::thr_self() << "|" \
		    << currTime		<< "|" \
			<< strPriority	<< "|" \
			<< strModule	<< "|" \
			<< fileName		<< "|" \
			<< lineNo		<< "|" \
			<< strFormated  << endl;
			//<< szTmp <<endl;
	infile.flush();

	mutex.unlock();
}

void Logger::Log(int intPriority, int intModule, const char* strEnter, ...)
{
	long	cTime;
	string	currTime;
	string	strPriority;
	string	strModule;

	cTime		= time(NULL);
	currTime	= convertTimeToString(cTime);

	switch (intPriority)
	{
		case 0:
			strPriority = "INFO";
			break;
			
		case 1:
			strPriority = "WARNING";
			break;

		case 2:
			strPriority = "ERROR";
			break;

		case 3:
			strPriority = "STATUS";
			break;

		default:
			strPriority = "UNKNOWN BUG";
			break;
	}

	switch (intModule)
	{
		case 0:
			strModule = "FILE REPLICATION";
			break;

		case 1:
			strModule = "SCHEDULAR";
			break;

		case 2:
			strModule = "SNAPSHOT";
			break;

		default:
			strModule = "DEFAULT";
			break;
	}

	va_list arglist;
	va_start(arglist, strEnter);     /* Initialize variable arguments. */

	//char szTmp[1024] = "";
	string strFormated = "";

	long start	= 0;
	unsigned int i = 0;

	for(i = 0; i < strlen(strEnter); i++)
	{
		if(strEnter[i] == '%')
		{
			char szPre[1024];
			const char *ptr = strEnter;

			ptr = ptr + start;
			inm_strncpy_s(szPre,ARRAYSIZE(szPre), ptr, i - start);
			szPre[i - start] = '\0';

			strFormated += szPre;

			int iTmp		= 0;
			long lTmp		= 0;
			float fTmp		= 0;
			char *strTmp	;

			char szBuffer[1024];

			switch(strEnter[i + 1])
			{
			case 'd':
				iTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", iTmp);
				strFormated += szBuffer;
				break;

			case 'f':
				fTmp = (float) va_arg(arglist, double);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%f", fTmp);
				strFormated += szBuffer;
				break;

			case 's':
				{
					strTmp = va_arg(arglist, char*);
					strFormated += strTmp;
					break;
				}

			case 'l':
				if(strEnter[i + 2] == 'd')
				{
					lTmp = va_arg(arglist, long);
					inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%ld", lTmp);
					strFormated += szBuffer;
					i++;
					break;
				}

				if(strEnter[i + 2] == 'f')
				{
					fTmp = (float) va_arg(arglist, double);
					inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", fTmp);
					strFormated += szBuffer;
					i++;
					break;
				}

			case 'c':
				iTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", iTmp);
				strFormated += szBuffer;
				break;

			case 'x':
				lTmp = va_arg(arglist, int);
				inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "%d", lTmp);
				strFormated += szBuffer;
				break;
			}

			i++;
			start = i + 1;
		}
	}

	if(start != i)
	{
		const char *ptr = strEnter;
		ptr += start;
		strFormated += ptr;
	}

 
       long size = getFileSize();

       if(size > MAX_LOG_SIZE)
       {
        CloseLog();
        infile.open(LOG_FILE, ios::trunc);

       }
 

	mutex.lock();

	infile << ACE_OS::getpid() << ":" << ACE_OS::thr_self() << ":" << currTime << ":" << strPriority << ":" << strModule << ":" << /*szTmp*/ strFormated << "\n";
	infile.flush();

	mutex.unlock();
}

void Logger::Log(int intPriority, int intModule, long intEnter)
{
	string	currTime;
	long	cTime;

	cTime		= time(NULL);
	currTime	= convertTimeToString(cTime);

	string strPriority;
	string strModule;

	switch (intPriority)
	{
		case 0:
			strPriority = "INFO";
			break;

		case 1:
			strPriority = "WARNING";
			break;

		case 2:
			strPriority = "ERROR";
			break;

		case 3:
			strPriority = "STATUS";
			break;

		default:
			strModule = "UNKNOWN BUG";
			break;
	}

	switch (intModule)
	{
		case 0:
			strModule = "FILE REPLICATION";
			break;

		case 1:
			strModule = "SCHEDULAR";
			break;

		case 2:
			strModule = "SNAPSHOT SCHEDULING";
			break;

		default:
			strModule = "DEFAULT";
			break;
	}

      
       long size = getFileSize();

       if(size > MAX_LOG_SIZE)
       {
        CloseLog(); 
        infile.open(LOG_FILE, ios::trunc); 
      }


	mutex.lock();

	infile << ACE_OS::getpid() << ":" << ACE_OS::thr_self() << ":" << currTime << ":" << strPriority << ":" << strModule << ":" << intEnter << "\n";
	infile.flush();

	mutex.unlock();
}

