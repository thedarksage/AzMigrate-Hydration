#ifndef HANDLER_CURL_H
#define HANDLER_CURL_H

#include "svtypes.h"
#include <curl/curl.h>
#include <ace/Thread.h>

namespace tal
{
//
// PR # 6337
// Routine to set/get the value to be passed for curl_verbose 
//
void set_curl_verbose(bool option);
bool get_curl_verbose();

void GlobalInitialize();
void GlobalShutdown();
void thread_setup(void);
void thread_cleanup(void);
void tal_locking_callback(int mode, int type, const char *file, int line);
ACE_thread_t tal_thread_id();
}//namespace tal
#endif //HANDLER_CURL_H

