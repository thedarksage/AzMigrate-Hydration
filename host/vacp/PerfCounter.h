#ifndef PERF_COUNTER__H
#define PERF_COUNTER__H    

#pragma once

#include <stdlib.h>
#include <map>
#include <string>

#include  <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>

#include "VacpCommon.h"

using namespace std;
using namespace boost::chrono;

extern void inm_printf(const char* format, ... );

class PerfCounter
{

    std::map<string, steady_clock::time_point> mStartTimes;
    std::map<string, steady_clock::time_point> mEndTimes;

    TagTelemetryInfo mtagTelInfo;

public:

    inline void StartCounter(string marker)
    {
        mStartTimes[marker] =  steady_clock::now();
    }

    inline void EndCounter(string marker)
    {
        mEndTimes[marker] = steady_clock::now();
    }

    void PrintElapsedTime(string marker)
    {
        std::map<string, steady_clock::time_point>::iterator itStartTime = mStartTimes.find(marker);
        std::map<string, steady_clock::time_point>::iterator itEndTime = mEndTimes.find(marker);

        if (itStartTime != mStartTimes.end() && itEndTime != mEndTimes.end())
        {
            duration<double> dur = itEndTime->second - itStartTime->second;
            milliseconds elapsedTime = duration_cast<milliseconds> (dur);
            std::stringstream ss;
            ss << "Elapsed Time for " << marker << " is";
            mtagTelInfo.Add(ss.str(), boost::lexical_cast<std::string>(elapsedTime.count()));
            inm_printf("Elapsed Time for %s is %lld ms\n", marker.c_str(), elapsedTime.count());
        }
    }

    void PrintAllElapsedTimes()
    {
        std::map<string, steady_clock::time_point>::iterator itEndTime = mEndTimes.begin();
        while (itEndTime != mEndTimes.end())
        {
            PrintElapsedTime((*itEndTime).first);
            itEndTime++;
        }
    }

    const TagTelemetryInfo & GetTagTelemetryInfo() const
    {
        return mtagTelInfo;
    }

    void Clear()
    {
        mStartTimes.clear();
        mEndTimes.clear();
        mtagTelInfo.Clear();
    }
};

#endif PERF_COUNTER__H
