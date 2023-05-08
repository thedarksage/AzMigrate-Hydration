#include <cstdlib>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <openssl/crypto.h>
#include <ace/OS_NS_Thread.h>
#include "portable.h"
#include "Guard.h"
#include "portablehelpers.h"
#include "HandlerCurl.h"

static tal::Mutex *lock_cs;


static bool curl_verbose_option = false;

//
// PR # 6337
// Routine to set/get the value to be passed for curl_verbose 
//
void tal::set_curl_verbose(bool option)
{
    curl_verbose_option = option;
}

bool tal::get_curl_verbose()
{
    return curl_verbose_option ;
}

void tal::GlobalInitialize()
{
    curl_global_init(CURL_GLOBAL_ALL);
    thread_setup();
}

void tal::GlobalShutdown()
{
    curl_global_cleanup();
    thread_cleanup();
}

void tal ::thread_setup(void)
{
    lock_cs=new Mutex[CRYPTO_num_locks()];
    if (0 == CRYPTO_get_locking_callback()) {
        CRYPTO_set_locking_callback((void (*)(int,int,const char *,int))tal_locking_callback);
        CRYPTO_set_id_callback((unsigned long (*)())tal_thread_id);
    }
    /* id callback defined */
}

void tal ::thread_cleanup(void)

{
    CRYPTO_set_locking_callback(NULL);
    delete[]lock_cs;
}

ACE_thread_t tal::tal_thread_id()
{
    return ACE_OS::thr_self();
}

void tal :: tal_locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        lock_cs[type].Acquire();
    }
    else
    {
        lock_cs[type].Release();
    }
}
