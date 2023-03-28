#ifndef __PAIR_DETAILS_H
#define __PAIR_DETAILS_H

#include "IBlockDevice.h"

namespace PairInfo
{
    typedef enum TEST_CASE
    {
        TC_DI,
        TC_WAL,
        TC_BARRIER_HONOUR,
        TC_NONE
    } TC;

    typedef enum PT
    {
        PT_D2D,
        PT_D2F,
        PT_F2D,
        PT_F2F
    } PAIR_TYPE;

    typedef enum
    {
        DITEST = 1,
        DIWITHOUTIR = 2,
        BARRIERHONOUR = 3
    } SubTestType_t;

    typedef enum
    {
        SEQ = 1,
        RANDOM = 2,
        MIXED = 3
    } IOType_t;

    class SubTestConfig_t
    {
    public:
        SubTestType_t subtestType;
        unsigned long chunkSize;
        IOType_t ioType;
        SubTestConfig_t(SubTestType_t tT, unsigned long cS, IOType_t iT)
        {
            subtestType= tT;
            chunkSize = cS;
            ioType = iT;
        }
    };

    class PairDetails
    {
    private:

        PT m_pairType;
        std::string m_strSourceDeviceName;
        std::string m_strTargetDeviceName;
        std::string m_strSubTest;
        std::string m_logFileName;
        IBlockDevice* m_srcDevice;
        IBlockDevice* m_tgtDevice;
        std::map<std::string, int>m_mapDiskNameToDiskNum;

        bool CreateDiskNameToNumMapping();

    public:
        TC m_testCase;
        bool testComplete;
        bool pauseDataThread;
        SubTestConfig_t *subtestConfig;
        unsigned long maxBytesPerSec;
        bool forceBitmapMode;
        bool doneForceBitmapMode;
        boost::thread dataThread;

        PairDetails(TC testCase)
        {
            m_testCase = testCase;
            testComplete = false;
            pauseDataThread = false;
            forceBitmapMode = false;
            doneForceBitmapMode = false;
            CreateDiskNameToNumMapping();
        }

        int GetDiskNumFromDiskName(const std::string & strDiskName);
        std::string GetDiskNameFromDiskNum(const int nDiskNum);
        void SetPairType(PT pt);
        PT GetPairType();
        void SetLogFileName(std::string strLog);
        std::string GetLogFileName();
        void SetSourceDevName(std::string sd);
        std::string GetSourceDevName();
        void SetTargetDevName(std::string td);
        void SetSourceDevice(IBlockDevice* pSrcDev);
        void SetSubTest(std::string st);
        std::string GetSubTest();
        IBlockDevice* GetSourceDevice();
        void SetTargetDevice(IBlockDevice* pTgtDev);
        IBlockDevice* GetTargetDevice();
        std::string GetTargetDevName();
    };

}
#endif
