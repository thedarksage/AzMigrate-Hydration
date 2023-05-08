#include <iostream>
#include <signal.h>

void waitForQuit()
{
    sigset_t signalSet;
    
    sigemptyset(&signalSet);
    
    sigaddset(&signalSet, SIGINT);
    sigaddset(&signalSet, SIGTERM);
    if (0 != pthread_sigmask(SIG_BLOCK, &signalSet, NULL)) {
        return;
    }

    std::cout << "to quit: enter ctrl-c or issue kill from another shell to this process\n" << std::endl;

    int sigNumber;
    while (true) {
        sigwait(&signalSet, &sigNumber);
        if (SIGINT == sigNumber || SIGTERM == sigNumber) {
            break;
        }
    }
}
