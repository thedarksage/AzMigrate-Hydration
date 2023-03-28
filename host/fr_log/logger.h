#ifndef LOGGER__H
#define LOGGER__H
/* Log levels are defined in portable.h */

void CloseDebug();
void SetLogLevel(int level);
void SetLogFileName(const char* fileName);
void SetLogInitSize(int size);
void DebugPrintf( int nLogLevel, const char* format, ... );
void DebugPrintf( const char* format, ... );

#endif
