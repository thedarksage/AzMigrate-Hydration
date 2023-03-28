#include <iostream>
#include <string>
#include <cstdlib>
#include "inmzpool.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(int argc, char **argv)
{    
    const std::string ZPOOL = "/usr/sbin/zpool";
    const std::string SPACE = " ";
    const std::string DOUBLE_QUOTE = "\'";
    std::string cmd = ZPOOL;
    int fd = -1;
    int rval = EXIT_SUCCESS;
     


    if (argc <= 1)
    {
        std::cerr << "Usage: inmzpool <arguements to zpool command>\n";
        return EXIT_FAILURE;
    }

    if (OpenLinvsnapSyncDev(fd))
    {
        cmd += SPACE;
        //Step 1: Form the arguements to zpool
        while (*++argv)
        {
            cmd += DOUBLE_QUOTE;   
            cmd += *argv;
            cmd += DOUBLE_QUOTE;
            cmd += SPACE;
        }

	    cmd.erase(cmd.find_last_not_of(SPACE)+1);

        //std::cout << "Executing the following command...\n";
        //std::cout << cmd << '\n';

        std::string err;
        std::string out;
        int exitcode = 0;

        if (ExecuteInmCommand(cmd, out, err, exitcode))
        {
            std::cout << out;
        }
        else
        {
            std::cerr << err;
            rval = exitcode;
        }
        if (!CloseLinvsnapSyncDev(fd))
        {
            std::cerr << "Unable to close with virtual snapshot driver. please retry\n";
            rval = EXIT_FAILURE;
        }
    }
    else
    {
        std::cerr << "Unable to open virtual snapshot driver. please retry\n";
        rval = EXIT_FAILURE;
    }

    return rval;
}


