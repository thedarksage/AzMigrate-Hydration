//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : testinmdistributor.cpp
// 
// Description: 
// 

#include <iostream>
#include <ace/OS.h>
#include "additiondistributor.h"
#include "echoworker.h"
#include "logger.h"
#include "configurator2.h"

Configurator* TheConfigurator = 0; // singleton

bool ShouldQuit(int waitsecs);

int main(int, char**)
{    
    try {
        SetLogFileName("testinmdistributor.log");

        ACE::init();

        AdditionDistributor ad;
        EchoWorker w(&ad), x(&ad);
        InmDistributor::Workers_t ws;
        ws.push_back(&w);
        ws.push_back(&x);
         
        if (ad.Create(ws))
        {
            int i = 5, j = 7;
            ad.Distribute(&i, sizeof i, &ShouldQuit, 5);
            ad.Distribute(&j, sizeof j, &ShouldQuit, 5);

            while (12 != ad.Sum())
            {
                std::cout << "sum is not yet updated.. waiting\n";
                ACE_OS::sleep(1);
            }
            std::cout << "sum updated now\n";

            ad.Destroy();
        }
        else
        {
            std::cerr << "failed to create a distributor\n";
        }
         
        
    } catch (std::exception& e) {
        std::cout << "caught " << e.what() << '\n';
    } catch (...) {
        std::cout << "caught unknow exception\n";
    }

    return 0;
}


bool ShouldQuit(int waitsecs)
{
    return false;
}

