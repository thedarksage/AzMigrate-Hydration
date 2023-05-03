//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : inmzpool.cpp
// 
// Description: 
// 

#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include "inmzpool.h"
#include "inmcommand.h"


bool ExecuteInmCommand(const std::string& fsmount, std::string &outputmsg, std::string& errormsg, int & ecode)
{
    bool rv = true;
    InmCommand mount(fsmount);
    InmCommand::statusType status = mount.Run();
    if (status != InmCommand::completed)
    {
        if(!mount.StdErr().empty())
        {
            errormsg = mount.StdErr();
            ecode = mount.ExitCode();
        }
        rv = false;
    }
    else if (mount.ExitCode())
    {
        errormsg = mount.StdErr();
        ecode = mount.ExitCode();
        rv = false;
    }
    else
    {
        outputmsg = mount.StdOut();
    }
    return rv;
}


bool OpenLinvsnapSyncDev(int &fd)
{
    const std::string LINVSNAP_SYNC_DEV = LINVSNAP_SYNC_DRV;
    bool bretval = false;

    do
    {
        fd = open(LINVSNAP_SYNC_DEV.c_str(), O_RDONLY);
        if (-1 != fd)
        {
            bretval = true;
            break;
        }
        else
        {
            sleep(CHECK_FREQ_FOR_SYNCDEV);
        }
    } while (true);

    if (!bretval)
    {
        //std::cerr << "Unable to open virtual snapshot driver\n";
    }

    return bretval;
}


bool CloseLinvsnapSyncDev(int &fd)
{
    bool bretval = true;

    if (0 != close(fd))
    {
        bretval = false;
    }
    else
    {
        //std::cout << "Closed linvsnap driver\n";
    }

    return bretval;
}
