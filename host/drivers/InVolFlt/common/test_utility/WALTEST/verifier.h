#ifndef __verifier_h_
#define __verifier_h_

extern pthread_mutex_t verifier_mutex;
extern pthread_cond_t  verifier_cond_wait;
void launch_verifier_thread(void);

#endif
