#include<iostream>
#include<boost/lexical_cast.hpp>
#include"DITest.h"
#include"CDiskDevice.h"
#include"S2Agent.h"
#include"inmsafecapis.h"
#include"DataProcessor.h"


using namespace std;

int PerformDITest(int argc, char* argv[])
{
    //begin
    if (argc < 2)
    {
        cout << endl << "Usage:" << endl
        <<"-sourcedisk "<<argv[1] 
        <<" -targetDisk "<<argv[3]
        <<" -logLocation"<<argv[5]; //MCHECK:wide character usage may be required:for testing
        return -1;
    }

    std::string &targetPath = boost::lexical_cast<std::string>(argv[3]);

    CS2Agent s2Agent(DataReplication, targetPath.c_str());
    
    std::string strSrcDisk;
    std::string strTgtDiskNum;

    strSrcDisk = boost::lexical_cast<std::string>(argv[1]);
    strTgtDisk = boost::lexical_cast<std::string>(argv[3]);
    std::string strLogFilePath = boost::lexical_cast<std::string>(argv[5]);
    std::string strLogFile = strLogFilePath + "\\" + strSrcDisk + ".log";

    try
    {
        boost::shared_ptr<IPlatformAPIs>pPlatformApis;
        pPlatformApis.reset(new PlatformAPIs());

        CDiskDevice srcDisk(srcDiskNum, boost::lexical_cast<IPlatformAPIs*>(pPlatformApis));
        CDiskDevice tgtDisk(tgtDiskNum, boost::lexical_cast<IPlatformAPIs*>(pPlatformApis));

        s2Agent.AddReplicationSettings(srcDisk.GetDeviceNumber(),
            tgtDisk.GetDeviceNumber(), strLogFile);

        s2Agent.StartReplication(0, srcDisk.GetDeviceSize(), true);

        // run your workload

        if (s2Agent.Validate())
        {
            printf("Tests Passed");
        }
        else
        {
            printf("Tests Failed");
        }
    }
    catch (exception& ex)
    {
        printf("%s", ex.what());
    }

    return 0;
}
