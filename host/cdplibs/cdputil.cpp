//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdputil.cpp
//
// Description: 
//

#include "cdputil.h"
#include "VacpUtil.h"
#include "cdpglobals.h"
#include "StreamEngine.h"
#include "cdpplatform.h"
#include "error.h"
#include <ace/OS_NS_unistd.h>
#include <ace/File_Lock.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Manual_Event.h>
#include <ace/Signal.h>
#include "localconfigurator.h"
#include "VsnapUser.h"
#include "basicio.h"
#include "cdpv3metadatafile.h"

#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include "localconfigurator.h"
#include "configwrapper.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

#include "inmageex.h"
#include "inmalertdefs.h"

#include "boost/date_time/posix_time/posix_time.hpp"

using namespace std;

#include "cdpdatabase.h"

#include "setpermissions.h"


ACE_Manual_Event* g_QuitEvent = NULL;
bool g_QuitRequested = false;
bool g_clihandler = false;
ACE_Atomic_Op<ACE_Thread_Mutex, bool> CDPUtil::quitflag = false ;

void CDPUtil::set_quit_flag(bool flag)
{
    quitflag = flag ;
}

const std::string CDPUtil::CDPRESERVED_FILE = "cdpreserved.rdat";

bool CompareCDPEventByEventTime(const CDPEvent & cdpEvent1,const CDPEvent& cdpEvent2)
{
    return (cdpEvent1.c_eventtime < cdpEvent2.c_eventtime);
}

svector_t CDPUtil::split(const string arg,
                         const string delim,
                         const int numFields) 
{

    svector_t splitList;
    int delimetersFound = 0;
    string::size_type delimLength = delim.length();
    string::size_type argEndIndex   = arg.length();
    string::size_type front = 0;

    //
    // Find the location of the first delimeter.
    //
    string::size_type back = arg.find(delim, front);

    while( back != string::npos ) {
        ++delimetersFound;

        if( (numFields) && (delimetersFound >= numFields) ) {
            break;
        }

        //
        // Put the first (left most) field of the string into
        // the vector of strings.
        //
        // Example:
        //
        //      field1:field2:field3
        //      ^     ^
        //      front back
        //
        // The substr() call will take characters starting
        // at front extending to the length of (back - front).
        //
        // This puts 'field1' into the splitList.
        //
        splitList.push_back(arg.substr(front, (back - front)));

        //
        // Get the front of the second field in arg.  This is done
        // by adding the length of the delimeter to the back (ending
        // index) of the previous find() call.
        //
        // This makes front point to the first character after
        // the delimeter.
        //
        // Example:
        //
        //      'field1:field2:field3'
        //             ^^
        //         back front
        //
        //       delimLength = 1
        //
        front = back + delimLength;

        //
        // Find the location of the next delimeter.
        //
        back = arg.find(delim, front);
    }

    //
    // After we get through the entire string, there may be data at the
    // end of the arg that has not been stored.  The data will be
    // the trailing remnant (or the entire string if no delimeter was found).
    // So put the "remainder" into the splitList (this would add 'field3'
    // to the list.)
    //
    // Example:
    //
    //      'field1:field2:field3'
    //                     ^     ^
    //                 front     argEndIndex
    //
    // (argEndIndex - front) yeilds the number of elements to grab.
    //
    if( front < argEndIndex ) {
        splitList.push_back(arg.substr(front, (argEndIndex - front)));
    }

    return splitList;
}

void CDPUtil::trim(string& s, string trim_chars ) 
{
    Trim(s, trim_chars);
}

void CDPUtil::WaitForNSecs(int secs)
{
    ACE_OS::sleep(secs);
}

bool CDPUtil::ToDisplayTimeOnConsole(SV_ULONGLONG ts, string & display)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        stringstream displaytime;
        SV_TIME   svtime;

        if(!ToSVTime(ts,svtime))
        {
            stringstream l_stdfatal;
            l_stdfatal << "Specified Time " << ts  << " is invalid\n";
            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        displaytime  << svtime.wYear   << "/" 
            << svtime.wMonth  << "/" 
            << svtime.wDay    << " " 
            << svtime.wHour   << ":" 
            << svtime.wMinute << ":" 
            << svtime.wSecond << ":"
            << svtime.wMilliseconds << ":" 
            << svtime.wMicroseconds << ":"
            << svtime.wHundrecNanoseconds ;

        //(ts/10)%1000 << ":"
        //<< (ts%10);

        display = displaytime.str();

    } while ( FALSE );

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPUtil::ToDisplayTimeOnCx(SV_ULONGLONG ts, string & display)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        stringstream displaytime;
        SV_TIME   svtime;

        if(!ToSVTime(ts,svtime))
        {
            stringstream l_stdfatal;
            l_stdfatal << "Specified Time " << ts  <<  " is invalid\n";
            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        displaytime  << svtime.wYear   << "," 
            << svtime.wMonth  << "," 
            << svtime.wDay    << "," 
            << svtime.wHour   << "," 
            << svtime.wMinute << "," 
            << svtime.wSecond << ","
            << svtime.wMilliseconds << ","
            << svtime.wMicroseconds << ","
            << svtime.wHundrecNanoseconds;

        display = displaytime.str();

    } while ( FALSE );

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPUtil::InputTimeToFileTime(const string & input, SV_ULONGLONG & ts)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        try
        {
            ts = 0;

            svector_t tokens = CDPUtil::split(input, " ", 2);
            if (!tokens.size())
            {
                rv = false;
                break;
            }

            svector_t yyyymmdd  = CDPUtil::split(tokens[0], "/" , 3);
            if(yyyymmdd.size() != 3 )
            {
                rv = false;
                break;
            }

            SV_TIME svtime;
            svtime.wYear = atoi((yyyymmdd[0]).c_str());
            svtime.wMonth = atoi((yyyymmdd[1]).c_str());
            svtime.wDay = atoi((yyyymmdd[2]).c_str());

            svtime.wHour = 0;
            svtime.wMinute = 0;
            svtime.wSecond = 0;
            svtime.wMilliseconds = 0;
            svtime.wMicroseconds =0;
            svtime.wHundrecNanoseconds =0;

            if(tokens.size() == 2)
            {
                svector_t hmsmun = CDPUtil::split(tokens[1], ":");
                if (hmsmun.size() >= 1 )
                {
                    svtime.wHour = atoi(hmsmun[0].c_str());
                }
                if (hmsmun.size() >= 2 )
                {
                    svtime.wMinute = atoi(hmsmun[1].c_str());
                }
                if (hmsmun.size() >= 3 )
                {
                    svtime.wSecond = atoi(hmsmun[2].c_str());
                }
                if (hmsmun.size() >= 4 )
                {
                    svtime.wMilliseconds = atoi(hmsmun[3].c_str());
                }
                if (hmsmun.size() >= 5 )
                {
                    svtime.wMicroseconds  = atoi(hmsmun[4].c_str());
                }
                if (hmsmun.size() >= 6 )
                {
                    svtime.wHundrecNanoseconds = atoi(hmsmun[5].c_str());
                }
                if (hmsmun.size() >= 7)
                {
                    rv = false;
                    break;
                }
            }

            if (!ToFileTime(svtime, ts)) 
            {    
                rv = false;
                break;          
            }

        }catch (exception & )    {
            rv = false;
            break;
        }

    } while ( FALSE );

    if (!rv)
    {
        stringstream l_stdfatal;
        l_stdfatal << "Specified Time: " << input  << " is invalid\n"
            << "Required Time format: yyyy/mm/dd [hr][:min][:sec][:millisec][:microsec][:hundrednanosec]\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPUtil::CXInputTimeToConsoleTime(const string & input, string & consoleTime)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        try
        {
            SV_TIME svtime;      

            sscanf(input.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd", 
                &svtime.wYear, 
                &svtime.wMonth,
                &svtime.wDay,
                &svtime.wHour,
                &svtime.wMinute,
                &svtime.wSecond,
                &svtime.wMilliseconds,
                &svtime.wMicroseconds,
                &svtime.wHundrecNanoseconds);

            stringstream displaytime;
            displaytime  << svtime.wYear   << "/" 
                << svtime.wMonth  << "/" 
                << svtime.wDay    << " " 
                << svtime.wHour   << ":" 
                << svtime.wMinute << ":" 
                << svtime.wSecond << ":"
                << svtime.wMilliseconds << ":" 
                << svtime.wMicroseconds << ":"
                << svtime.wHundrecNanoseconds ;

            consoleTime = displaytime.str();
        }catch (exception & )    {
            rv = false;
            break;
        }

    } while ( FALSE );

    if (!rv)
    {
        stringstream l_stdfatal;
        l_stdfatal << "Specified Time: " << input  << " is invalid\n"
            << "Required Time format: yyyy/mm/dd [hr][:min][:sec][:millisec][:microsec][:hundrednanosec]\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        //cerr << l_stdfatal.str();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPUtil::CxInputTimeToFileTime(const string & input, SV_ULONGLONG & ts)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        try
        {
            SV_TIME svtime;      

            sscanf(input.c_str(), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd", 
                &svtime.wYear, 
                &svtime.wMonth,
                &svtime.wDay,
                &svtime.wHour,
                &svtime.wMinute,
                &svtime.wSecond,
                &svtime.wMilliseconds,
                &svtime.wMicroseconds,
                &svtime.wHundrecNanoseconds);

            if (!ToFileTime(svtime, ts)) 
            {    
                rv = false;
                break;          
            }

        }catch (exception & )    {
            rv = false;
            break;
        }

    } while ( FALSE );

    if (!rv)
    {
        stringstream l_stdfatal;
        l_stdfatal << "Specified Time: " << input  << " is invalid\n"
            << "Required Time format: yyyy/mm/dd [hr][:min][:sec][:millisec][:microsec][:hundrednanosec]\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        //cerr << l_stdfatal.str();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool IsLeapYear( int year ) {
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

//
// fileTime is 64-bit value representing the number 
// of 100-nanosecond intervals since January 1, 1601 (UTC) 
//
bool CDPUtil::ToSVTime( SV_ULONGLONG fileTime, SV_TIME& svTime )
{

    if( fileTime & 0x8000000000000000LL ) {
        return false;
    }
    long long units = fileTime; // initially in 100 nanosecond intervals

    // Milliseconds remaining after seconds are accounted for
    int const IntervalsPerSecond = 10000000;
    int const IntervalsPerMillisecond = IntervalsPerSecond / 1000;
    svTime.wMilliseconds = (static_cast<unsigned>( units % IntervalsPerSecond ) ) / IntervalsPerMillisecond;

    // Time is now in seconds
    units = units / IntervalsPerSecond;

    // How many seconds remain after days are subtracted off
    // Note: no leap seconds!
    int const SecondsPerDay = 86400;
    long days = static_cast<long>( units / SecondsPerDay );
    int secondsInDay = static_cast<int>( units % SecondsPerDay );

    // Calculate hh:mm:ss from seconds 
    int const SecondsPerHour   = 3600;
    int const SecondsPerMinute = 60;
    svTime.wHour = secondsInDay / SecondsPerHour;
    secondsInDay = secondsInDay % SecondsPerHour;
    svTime.wMinute = secondsInDay / SecondsPerMinute;
    svTime.wSecond = secondsInDay % SecondsPerMinute;

    // Calculate day of week
    int const EpochDayOfWeek = 1;   // Jan 1, 1601 was Monday
    int const DaysPerWeek = 7;
    //svTime.wDayOfWeek = ( EpochDayOfWeek + days ) % DaysPerWeek;

    // Compute year, month, and day
    int const DaysPerFourHundredYears = 365 * 400 + 97;
    int const DaysPerNormalCentury = 365 * 100 + 24;
    int const DaysPerNormalFourYears = 365 * 4 + 1;

    long leapCount = ( 3 * ((4 * days + 1227) / DaysPerFourHundredYears) + 3 ) / 4;
    days += 28188 + leapCount;
    long years = ( 20 * days - 2442 ) / ( 5 * DaysPerNormalFourYears );
    long yearDay = days - ( years * DaysPerNormalFourYears ) / 4;
    long months = ( 64 * yearDay ) / 1959;

    // Resulting month based on March starting the year.
    // Adjust for January and February by adjusting year in tandem.
    if( months < 14 ) {
        svTime.wMonth = static_cast<SV_USHORT>(months - 1);
        svTime.wYear = static_cast<SV_USHORT>(years + 1524);
    }
    else {
        svTime.wMonth = static_cast<SV_USHORT>(months - 13);
        svTime.wYear = static_cast<SV_USHORT>(years + 1525);
    }

    // Calculate day of month by using MonthLengths(n)=floor( n * 30.6 ); works for small n
    svTime.wDay = static_cast<SV_USHORT>(yearDay - (1959 * months) / 64);

    svTime.wMicroseconds = static_cast<SV_USHORT>((fileTime / 10) % 1000);
    svTime.wHundrecNanoseconds = static_cast<SV_USHORT>(fileTime % 10);

    return true;
}

bool CDPUtil::ToTimestampinHrs(const SV_ULONGLONG &in_ts, SV_ULONGLONG & out_ts)
{
    SV_TIME svtime;
    out_ts = 0;

    if(!CDPUtil::ToSVTime(in_ts,svtime))
        return false;

    svtime.wMinute = 0;
    svtime.wSecond = 0;
    svtime.wMilliseconds = 0;
    svtime.wMicroseconds = 0;
    svtime.wHundrecNanoseconds = 0;

    return CDPUtil::ToFileTime(svtime, out_ts);
}

bool CDPUtil::ToFileTime( const SV_TIME & svTime, SV_ULONGLONG & fileTime )
{
    unsigned const DaysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int const februaryAdjustment = IsLeapYear( svTime.wYear ) ? 1 : 0;

    if( svTime.wMilliseconds >= 1000 ||
        svTime.wSecond >= 60 ||
        svTime.wMinute >= 60 ||
        svTime.wHour >= 24 ||
        svTime.wMonth < 1 ||
        svTime.wMonth > 12 ||
        svTime.wDay < 1 ||
        svTime.wDay > DaysPerMonth[ svTime.wMonth - 1 ] + februaryAdjustment * ( 2 == svTime.wMonth ) ||
        svTime.wYear < 1601 ||
        svTime.wMicroseconds >= 1000 ||
        svTime.wHundrecNanoseconds >= 10 )
    {
        return false;
    }

    // Count the years from March so leap days at end of year
    // Adjust January and February by adding 12 months and subtracting one year
    unsigned month, year;
    if( svTime.wMonth < 3 ) {
        month = svTime.wMonth + 13;
        year  = svTime.wYear  - 1;
    }
    else {
        month = svTime.wMonth + 1;
        year = svTime.wYear;
    }

    unsigned leapCenturyCount = ( 3 * ( year / 100 ) + 3 ) / 4;
    unsigned day = ( 36525 * year ) / 100 - leapCenturyCount +  // corrected year * daysPerYear
        ( 1959 * month ) / 64 +     // months * DaysPerMonth
        svTime.wDay - 584817;       // normalize day to Jan 1, 1601

    int const IntervalsPerSecond = 10000000;
    int const IntervalsPerMillisecond = IntervalsPerSecond / 1000;

    fileTime = ( ( ( ( static_cast<unsigned long long>( day ) * 24 + svTime.wHour ) * 60 + svTime.wMinute ) * 60 + svTime.wSecond ) * 1000 + svTime.wMilliseconds ) 
        * IntervalsPerMillisecond 
        + svTime.wMicroseconds * 10 
        + svTime.wHundrecNanoseconds;

    return true;
}

bool CDPUtil::GetServiceState(const std::string & ServiceName, SV_ULONG & state)
{
    return ::GetServiceState(ServiceName, state);
}


ostream & operator <<( ostream & o, const SV_GUID & guid)
{
    return o << guid.Data1 << " " << guid.Data2 << " " << guid.Data3 << " " << guid.Data4 ;
}

ostream & operator <<( ostream & o, const SVD_HEADER1 & hdr)
{
    /*OOD_DESIGN1
    o << "ResyncCtr: "               << hdr.resyncCtr << endl;
    o << "DirtyBlockCtr: "           << hdr.dirtyBlockCtr << endl;
    o << "MD5 checksum: "            << hdr.MD5Checksum << endl;
    //o << "Origin HostId: "           << hdr.OriginHostId << endl;// This is in binary form. needs conversion
    o << "Origin VolumeHash: "       << hdr.OriginVolumeId << endl;*/
    o << "Unique Id: "               << hdr.SVId << endl;
    o << "MD5 checksum: "            << hdr.MD5Checksum << endl;
    o << "Origin Host: "             << hdr.OriginHost << endl;
    o << "Origin Volume: "           << hdr.OriginVolume << endl;
    o << "Origin VolumeGroup: "      << hdr.OriginVolumeGroup << endl;
    o << "Destination Host: "        << hdr.DestHost << endl;
    o << "Destination Volume: "      << hdr.DestVolume<< endl;
    o << "Destination VolumeGroup: " << hdr.DestVolumeGroup << endl;

    return o;
}

ostream & operator <<( ostream & o, const SVD_HEADER_V2 & hdr)
{
    o << "Stream Version: " << hdr.Version.Major << "." << hdr.Version.Minor << endl;
    /*OOD_DESIGN1
    o << "ResyncCtr: "               << hdr.resyncCtr << endl;
    o << "DirtyBlockCtr: "           << hdr.dirtyBlockCtr << endl;
    o << "MD5 checksum: "            << hdr.MD5Checksum << endl;
    //o << "Origin HostId: "           << hdr.OriginHostId << endl;// This is in binary form. needs conversion
    o << "Origin VolumeHash: "       << hdr.OriginVolumeId << endl;*/
    o << "Unique Id: "               << hdr.SVId << endl;
    o << "MD5 checksum: "            << hdr.MD5Checksum << endl;
    o << "Origin Host: "             << hdr.OriginHost << endl;
    o << "Origin Volume: "           << hdr.OriginVolume << endl;
    o << "Origin VolumeGroup: "      << hdr.OriginVolumeGroup << endl;
    o << "Destination Host: "        << hdr.DestHost << endl;
    o << "Destination Volume: "      << hdr.DestVolume<< endl;
    o << "Destination VolumeGroup: " << hdr.DestVolumeGroup << endl;

    return o;
}

ostream & operator <<( ostream & o, const SVD_TIME_STAMP & ts)
{
    o << "TimeStamp: "    << ts.TimeInHundNanoSecondsFromJan1601 << endl;

    SV_TIME SvTime;
    CDPUtil::ToSVTime(ts.TimeInHundNanoSecondsFromJan1601, SvTime);

    o << SvTime.wYear << "/" << SvTime.wMonth << "/" << SvTime.wDay << " " ;
    o << SvTime.wHour << ":" << SvTime.wMinute <<":" << SvTime.wSecond << " ";
    o << SvTime.wMilliseconds << ":" << (SV_LONGLONG)(ts.TimeInHundNanoSecondsFromJan1601/10)%1000 ;
    o << ":" << (SV_LONGLONG)(ts.TimeInHundNanoSecondsFromJan1601 %10) << endl;

    return o;
}

ostream & operator <<( ostream & o, const SVD_TIME_STAMP_V2 & ts)
{
    o << "TimeStamp: "    << ts.TimeInHundNanoSecondsFromJan1601 << endl;

    SV_TIME SvTime;
    CDPUtil::ToSVTime(ts.TimeInHundNanoSecondsFromJan1601, SvTime);

    o << SvTime.wYear << "/" << SvTime.wMonth << "/" << SvTime.wDay << " " ;
    o << SvTime.wHour << ":" << SvTime.wMinute <<":" << SvTime.wSecond << " ";
    o << SvTime.wMilliseconds << ":" << (SV_LONGLONG)(ts.TimeInHundNanoSecondsFromJan1601/10)%1000 ;
    o << ":" << (SV_LONGLONG)(ts.TimeInHundNanoSecondsFromJan1601 %10) << endl;

    o << "Sequence Id: " << ts.SequenceNumber << endl;

    return o;
}

ostream & operator <<( ostream & o, const SV_ULARGE_INTEGER & lodc)
{
    o << "Length of Data Changes: " << lodc.QuadPart << endl;

    return o;
}

ostream & operator <<( ostream & o, const SV_MARKER & marker)
{
    o << "Application Name: "    << marker.AppName() << endl;
    o << "Tag: "                 << marker.Tag().get()     << endl;
    o << "Tag Length: "          << marker.TagLength() << endl;

    return o;
}

//ostream & operator <<( ostream & o, const cdp_drtd_t & drtd)
//{
//    o << "Volume Offset : "    << drtd.get_volume_offset()  << endl;
//    o << "Data Length: "       << drtd.get_length()  << endl;
//    o << "File Offset: "       << drtd.get_file_offset() << endl;
//
//    return o;
//}

ostream & operator <<( ostream & o, const cdp_drtdv2_t & drtd)
{
    o << "Volume Offset : "    << drtd.get_volume_offset()  << endl;
    o << "Data Length: "       << drtd.get_length()     << endl;
    o << "File Offset: "       << drtd.get_file_offset()  << endl;
    o << "Time Delta: "        << drtd.get_timedelta() << endl;
    o << "Sequence Delta: "    << drtd.get_seqdelta() << endl;
    o << "File id: "           << drtd.get_fileid() << endl;

    return o;
}

ostream & operator <<( ostream & o, const SVD_DATA_INFO & data)
{
    o << "Data Start Offset : "    << data.drtdStartOffset  << endl;
    o << "Data Length: "           << data.drtdLength.QuadPart  << endl;
    o << "Diff End Offset: "       << data.diffEndOffset << endl;

    return o;
}

ostream & operator <<( ostream & o, const CDPV3BookMark & bkmk)
{
    std::string appname;
    std::string displaytime;
    std::string mode = ACCURACYMODE3;

    if (VacpUtil::TagTypeToAppName(bkmk.c_apptype, appname))
    {
        o << appname << "," ;
    } else
    {
        o << bkmk.c_apptype << ",";
    }

    if (CDPUtil::ToDisplayTimeOnConsole(bkmk.c_timestamp, displaytime))
    {
        o << displaytime << "," ;
    } else
    {
        o << bkmk.c_timestamp << ",";
    }


    if(bkmk.c_accuracy == 0) mode = ACCURACYMODE0;
    else if(bkmk.c_accuracy == 1) mode = ACCURACYMODE1;
    else if(bkmk.c_accuracy == 2) mode = ACCURACYMODE2;

    o << mode << ",";
    o << bkmk.c_value << "," ;

    return o;
}

ostream & operator <<( ostream & o,  const CDPV3RecoveryRange & timerange)
{
    std::string displaytime_tsfc, displaytime_tslc;
    std::string mode = ACCURACYMODE3;

    if (CDPUtil::ToDisplayTimeOnConsole(timerange.c_tsfc, displaytime_tsfc))
    {
        o << displaytime_tsfc << "," ;
    } else
    {
        o << timerange.c_tsfc << ",";
    }

    if (CDPUtil::ToDisplayTimeOnConsole(timerange.c_tslc, displaytime_tslc))
    {
        o << displaytime_tslc << "," ;
    } else
    {
        o << timerange.c_tslc << ",";
    }

    o << timerange.c_tsfcseq << ",";
    o << timerange.c_tslcseq << ",";

    if(timerange.c_accuracy == 0) mode = ACCURACYMODE0;
    else if(timerange.c_accuracy == 1) mode = ACCURACYMODE1;
    else if(timerange.c_accuracy == 2) mode = ACCURACYMODE2;

    o << mode << "," ;
    return o;
}

std::ostream & operator <<(std::ostream & o, const CsJob &job)
{
    o << "Job:" << std::endl;
    o << "--------------------" << std::endl;
    o << "Id:      " << job.Id << std::endl;
    o << "JobType: " << job.JobType << std::endl;
    o << "JobStatus:" << job.JobStatus << std::endl;
    o << "InputPayload:" << job.InputPayload << std::endl;
    o << "OutputPayload:" << job.OutputPayload << std::endl;
    o << job.Errors << std::endl;
    o << job.Context << std::endl;
    return o;
}
std::ostream & operator <<(std::ostream & o, const CsJobContext &jobContext)
{
    o << "Context:" << std::endl;
    o << "----------------" << std::endl;
    o << "ClientRequestId: " << jobContext.ClientRequestId << std::endl;
    o << "ActivityId: " << jobContext.ActivityId << std::endl;
    return o;
}

std::ostream & operator <<(std::ostream & o, const std::list<CsJobError> & errors)
{
    if (errors.empty())
    {
        o << "Errors: <None>" << std::endl;
    }
    else
    {
        std::list<CsJobError>::const_iterator iter = errors.begin();
        for (int i = 0; iter != errors.end(); iter++, i++)
        {
            o << "Job Error[" << i << "]:" << std::endl;
            o << (*iter);
        }
    }

    return o;
}

std::ostream & operator <<(std::ostream & o, const CsJobError &error)
{
    o << "ErrorCode: " << error.ErrorCode << std::endl;
    o << "Message: " << error.Message << std::endl;
    o << "PossibleCauses: " << error.PossibleCauses << std::endl;
    o << "RecommendedAction: " << error.RecommendedAction << std::endl;
    o << "MessageParams: " << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = error.MessageParams.begin();
        it != error.MessageParams.end(); ++it)
    {
        o << it->first << ":" << it->second << std::endl;
    }

    return o;
}

bool CDPUtil::getEventByIdentifier(const std::string & db, 
                                   std::string & recoverypt,
                                   const std::string & identifier)
{
    bool rv = true;
    CDPEvent commonevent;
    do
    {
        CDPMarkersMatchingCndn cndn;
        if(identifier.empty())
        {
            rv = false;
            break;
        }
        cndn.identifier(identifier);
        cndn.sortorder("ASC");
        vector<CDPEvent> events;

        if(!CDPUtil::getevents(db, cndn, events))
        {
            rv = false;
            break;
        }
        vector<CDPEvent>::reverse_iterator vr = events.rbegin();
        if(vr != events.rend())
            commonevent = *vr;
        else
            rv = false;
    }while(false);
    if(!rv)
    {
        DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
    }
    else
    {
        SV_TIME svtime;
        CDPUtil::ToSVTime(commonevent.c_eventtime,svtime);
        char buff[200];
        inm_sprintf_s(buff, ARRAYSIZE(buff), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
            svtime.wYear,
            svtime.wMonth,
            svtime.wDay,
            svtime.wHour,
            svtime.wMinute,
            svtime.wSecond,
            svtime.wMilliseconds,
            svtime.wMicroseconds,
            svtime.wHundrecNanoseconds);

        recoverypt = buff;
    }
    return rv;
}

bool CDPUtil::MostRecentConsistentPoint(const vector<string> & dbs,
                                        CDPEvent & commonevent,
                                        const SV_EVENT_TYPE & type,
                                        const SV_ULONGLONG & aftertime,
                                        const SV_ULONGLONG & beforetime
                                        )
{
    // Algorithm:
    // For each db, find the app consistency points 
    // find a intersection among them
    // pick the recent point
    // 

    bool rv = true;
    bool initialset = true;
    vector<CDPEvent> finalevents;
    commonevent.c_eventtime = 0;

    svector_t::const_iterator svi = dbs.begin();
    svector_t::const_iterator sve = dbs.end();


    CDPMarkersMatchingCndn cndn;
    cndn.mode(ACCURACYEXACT);
    cndn.type(type);

    if(beforetime != 0)
        cndn.beforeTime(beforetime);

    if(aftertime != 0)
        cndn.afterTime(aftertime);

    cndn.sortorder("ASC");

    for( ; svi != sve; ++svi)
    {
        string db = *svi;

        vector<CDPEvent> events;

        if(!CDPUtil::getevents(db, cndn, events))
        {
            rv = false;
            break;
        }

        std::sort(events.begin(),events.end(),mem_fun_ref( &CDPEvent::lt_time ));

        if(initialset)
        {
            finalevents.assign(events.begin(), events.end());
            initialset = false;
        } else
        {
            vector<CDPEvent> commonEvents;
            insert_iterator<vector<CDPEvent> > ii(commonEvents, commonEvents.begin());
            set_intersection(finalevents.begin(), finalevents.end(), events.begin(), events.end(), ii, mem_fun_ref( &CDPEvent::lt_time ));
            if(commonEvents.empty())
            {
                finalevents.clear();
                rv = false;
                break;
            } else
            {
                finalevents.assign(commonEvents.begin(),commonEvents.end());
            }
        }
    }

    if(finalevents.empty())
    {
        DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
        rv =false;
    }

    if(rv)
    {
        std::sort(finalevents.begin(),finalevents.end(),CompareCDPEventByEventTime);
        vector<CDPEvent>::iterator vb = finalevents.begin();
        vector<CDPEvent>::reverse_iterator vr = finalevents.rbegin();
        if(aftertime != 0 &&  beforetime == 0)
        {
            commonevent = *vb;
        } else
        {
            commonevent = *vr;
        }
    }

    return rv;
}
bool CDPUtil::PickLatestEvent(const vector<string> & dbs,
                              const SV_EVENT_TYPE & type,
                              const std::string & value,
                              std::string & identifier,
                              SV_ULONGLONG & ts)
{
    // Algorithm:
    // For each db, find the app consistency points 
    // find a intersection among them
    // pick the recent point
    // 

    bool rv = true;
    bool initialset = true;
    vector<CDPEvent> finalevents;
    ts = 0;
    try
    {
        svector_t::const_iterator dbCurIter = dbs.begin();
        svector_t::const_iterator dbEndIter = dbs.end();


        CDPMarkersMatchingCndn cndn;
        cndn.type(type);
        cndn.value(value);

        cndn.sortorder("ASC");

        for( ; dbCurIter != dbEndIter; ++dbCurIter)
        {
            string db = *dbCurIter;

            vector<CDPEvent> events;

            if(!CDPUtil::getevents(db, cndn, events))
            {
                rv = false;
                break;
            }

            std::sort(events.begin(),events.end(),mem_fun_ref( &CDPEvent::lt_time ));

            if(initialset)
            {
                finalevents.assign(events.begin(), events.end());
                initialset = false;
            } else
            {
                vector<CDPEvent> commonEvents;
                insert_iterator<vector<CDPEvent> > ii(commonEvents, commonEvents.begin());
                set_intersection(finalevents.begin(), finalevents.end(), events.begin(), events.end(), ii, mem_fun_ref( &CDPEvent::lt_time ));
                if(commonEvents.empty())
                {
                    finalevents.clear();
                    rv = false;
                    break;
                } else
                {
                    finalevents.assign(commonEvents.begin(),commonEvents.end());
                }
            }
        }

        if(finalevents.empty())
        {
            DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
            rv =false;
        }

        if(rv)
        {
            std::sort(finalevents.begin(),finalevents.end(),CompareCDPEventByEventTime);
            vector<CDPEvent>::reverse_iterator vr = finalevents.rbegin();
            ts = (*vr).c_eventtime;
            identifier = (*vr).c_identifier;
        }
    }
    catch(...)
    {
        rv = false;
        DebugPrintf(SV_LOG_INFO, "Failed to picking up the latest event.\n");
    }

    return rv;
}

bool CDPUtil::PickOldestEvent(const vector<string> & dbs,
                              const SV_EVENT_TYPE & type,
                              const std::string & value,
                              std::string & identifier,
                              SV_ULONGLONG & ts)
{
    // Algorithm:
    // For each db, find the app consistency points 
    // find a intersection among them
    // pick the oldest point
    // 

    bool rv = true;
    bool initialset = true;
    vector<CDPEvent> finalevents;
    ts = 0;
    try
    {
        svector_t::const_iterator dbCurIter = dbs.begin();
        svector_t::const_iterator dbEndIter = dbs.end();


        CDPMarkersMatchingCndn cndn;
        cndn.type(type);
        cndn.value(value);

        cndn.sortorder("ASC");

        for( ; dbCurIter != dbEndIter; ++dbCurIter)
        {
            string db = *dbCurIter;

            vector<CDPEvent> events;

            if(!CDPUtil::getevents(db, cndn, events))
            {
                rv = false;
                break;
            }

            std::sort(events.begin(),events.end(),mem_fun_ref( &CDPEvent::lt_time ));

            if(initialset)
            {
                finalevents.assign(events.begin(), events.end());
                initialset = false;
            } else
            {
                vector<CDPEvent> commonEvents;
                insert_iterator<vector<CDPEvent> > ii(commonEvents, commonEvents.begin());
                set_intersection(finalevents.begin(), finalevents.end(), events.begin(), events.end(), ii, mem_fun_ref( &CDPEvent::lt_time ));
                if(commonEvents.empty())
                {
                    finalevents.clear();
                    rv = false;
                    break;
                } else
                {
                    finalevents.assign(commonEvents.begin(),commonEvents.end());
                }
            }
        }

        if(finalevents.empty())
        {
            DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
            rv =false;
        }

        if(rv)
        {
            std::sort(finalevents.begin(),finalevents.end(),CompareCDPEventByEventTime);
            vector<CDPEvent>::iterator vi = finalevents.begin();
            ts = (*vi).c_eventtime;
            identifier = (*vi).c_identifier;
        }
    }
    catch(...)
    {
        rv = false;
        DebugPrintf(SV_LOG_INFO, "Failed to find the oldest matching event.\n");
    }

    return rv;
}

bool CDPUtil::FindCommonEvent(const vector<std::string> &  dbs, 
                              const SV_EVENT_TYPE & type,
                              const std::string & value,
                              std::string & identifier,
                              SV_ULONGLONG & ts)
{

    bool rv = true;
    bool multipleEvent = false;
    svector_t::const_iterator svi = dbs.begin();
    svector_t::const_iterator sve = dbs.end();


    CDPEvent commonevent;
    CDPMarkersMatchingCndn cndn;
    //cndn.mode(ACCURACYEXACT);
    cndn.type(type);
    cndn.value(value);

    commonevent.c_eventtime = 0;

    for( ; svi != sve; ++svi)
    {
        string db = *svi;


        vector<CDPEvent> events;

        if(!CDPUtil::getevents(db, cndn, events))
        {
            rv = false;
            break;
        }

        if(events.empty())
        {
            rv = false;
            break;
        }

        if(events.size() > 1 )
        {
            rv = false;
            DebugPrintf(SV_LOG_INFO, 
                "Multiple events are present with the name '%s' in the db '%s'."
                " So the operation cannot be completed.\n",
                value.c_str(),db.c_str());  
            multipleEvent = true;
            break;
        }

        vector<CDPEvent>::iterator vi = events.begin();
        if(commonevent.c_eventtime == 0)
        {
            commonevent = *vi;
        }else
        {
            if((!commonevent.c_identifier.empty()) && (commonevent.c_identifier != (*vi).c_identifier))
            {
                rv = false;
                break;
            }
            else if((commonevent.c_identifier.empty()) && (commonevent.c_eventtime != (*vi).c_eventtime))
            {
                rv = false;
                break;
            }
        }
    }

    if(!rv)
    {
        if(!multipleEvent)
        {
            DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
        }
    }else
    {
        ts = commonevent.c_eventtime;
        identifier = commonevent.c_identifier;
    }    

    return rv;
}

bool CDPUtil::isCCP(const vector<std::string> &  dbs,
                    const SV_ULONGLONG &ts,
                    bool & isconsistent,
                    bool & isavailable)
{
    bool rv = true;
    svector_t::const_iterator svi = dbs.begin();
    svector_t::const_iterator sve = dbs.end();
    isconsistent = true;
    isavailable = true;

    for( ; svi != sve; ++svi)
    {
        string db = *svi;

        bool isccp = false;
        bool isavail = false;
        if(!isCCP(db, ts, isccp,isavail))
        {
            rv = false;
            break;
        }

        if(!isccp)
        {
            isconsistent = false;
        }

        if(!isavail)
        {
            isavailable = false;
        }
    }

    return rv;
}

bool CDPUtil::isCCP(std::string const & dbname, const SV_ULONGLONG &ts, bool & isconsistent, bool & isavailable)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, dbname.c_str());

    bool rv = true;
    isconsistent = false;
    isavailable = false;

    try
    {
        do 
        {
            CDPDatabase db(dbname);
            CDPSummary summary;
            if(!db.getcdpsummary(summary))
            {
                rv = false;
                break;
            }
            string datefrom, dateto;
            if(!CDPUtil::ToDisplayTimeOnConsole(summary.start_ts, datefrom))
            {
                rv = false;
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(summary.end_ts, dateto))
            {
                rv = false;
                break;
            }

            if(ts < summary.start_ts || ts > summary.end_ts)
            {
                DebugPrintf(SV_LOG_INFO, "The specified time is not within available recovery time range(%s to %s).\n",datefrom.c_str(), dateto.c_str());
                rv = false;
                break;
            } 

            CDPTimeRangeMatchingCndn cndn;

            //cndn.mode(ACCURACYEXACT);

            CDPDatabaseImpl::Ptr dbptr = db.FetchTimeRanges(cndn);
            SVERROR hr_read;

            CDPTimeRange timerange;
            while( dbptr && (hr_read = dbptr->read(timerange)) == SVS_OK)
            {
                if(ts >= timerange.c_starttime && ts <= timerange.c_endtime)
                {
                    // found the specified time 
                    isavailable = true;

                    // it is within crash consistent interval
                    if(timerange.isAccurate())
                        isconsistent = true;
                    
                    break;
                } // else check next range

                if(ts > timerange.c_endtime) // covered all timeranges >= specified time, did not find the specified time
                    break;
            }


        } while (FALSE);
    }
    catch (std::exception & ex)
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED : %s for %s\n"
            "Caught exception %s ", FUNCTION_NAME, dbname.c_str(), ex.what());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, dbname.c_str());
    return rv;
}

void CDPUtil::TimeRangeIntersect(vector<CDPTimeRange> & lhs, vector<CDPTimeRange> &rhs, vector<CDPTimeRange> & result)
{
    // Algorithm
    // loop through and find intersection and then insert into result

    bool rv = true;
    vector<CDPTimeRange>::iterator lhs_iter = lhs.begin();
    vector<CDPTimeRange>::iterator lhs_end = lhs.end();

    for( ; lhs_iter != lhs_end ; ++lhs_iter)
    {
        CDPTimeRange lhs_tr = *lhs_iter;
        vector<CDPTimeRange>::iterator rhs_iter = rhs.begin();
        for( ; rhs_iter  != rhs.end(); ++ rhs_iter)
        {
            CDPTimeRange rhs_tr = *rhs_iter;
            if(lhs_tr.intersect(rhs_tr))
            {
                result.push_back(lhs_tr);
            }
        }
    }
}

bool CDPUtil::MostRecentCCP(const vector<std::string> &  dbs,
                            SV_ULONGLONG & ts,
                            SV_ULONGLONG & seq,
                            const SV_ULONGLONG & aftertime,
                            const SV_ULONGLONG & beforetime
                            )
{
    bool rv = true;
    SV_ULONGLONG starttime = aftertime;
    SV_ULONGLONG endtime = beforetime;
    svector_t::const_iterator svi = dbs.begin();
    svector_t::const_iterator sve = dbs.end();

    vector<CDPTimeRange> finalrange;
    bool firstset = true;
    bool fetchseq = false;
    ts = 0;
    seq = 0;
    if(aftertime != 0)
        starttime = aftertime + 1;

    if(beforetime != 0)
        endtime = beforetime -1;

    for( ; svi != sve; ++svi)
    {
        string db = *svi;

        CDPTimeRangeMatchingCndn cndn;
        cndn.mode(ACCURACYEXACT);

        vector<CDPTimeRange> currentrange;

        if(!CDPUtil::gettimeranges(db, cndn, currentrange))
        {
            rv = false;
            break;
        }

        if(firstset)
        {
            finalrange.assign(currentrange.begin(), currentrange.end());
            firstset = false;
        } else
        {
            vector<CDPTimeRange> commonrange;
            CDPUtil::TimeRangeIntersect(finalrange,currentrange, commonrange);
            if(commonrange.empty())
            {
                finalrange.clear();
                rv = false;
                break;
            } else
            {
                finalrange.assign(commonrange.begin(), commonrange.end());
            }
        }
    }

    if(finalrange.empty())
    {
        DebugPrintf(SV_LOG_INFO, "A common recovery point with the specified criteria  does not exist.\n");
        return false;
    }

    if(!rv)
        return false;

    bool found = false;
    vector<CDPTimeRange>::iterator tr_iter = finalrange.begin();
    vector<CDPTimeRange>::iterator tr_end = finalrange.end();
    for( ; tr_iter != tr_end; ++tr_iter)
    {
        CDPTimeRange tr = *tr_iter;

        //if no time range, take the latest
        if(starttime == 0  && endtime == 0)
        {
            if( (tr.c_endtime > ts) || ((tr.c_endtime == ts) && (tr.c_endseq < seq)) )
            {
                ts = tr.c_endtime;
                seq = tr.c_endseq;
                fetchseq = false;
            }
            found = true;
        }

        // if only lower bound is provided, take the nearest time to the lower bound
        else if(starttime != 0 && endtime == 0)
        {
            // ignore non intersecting range
            if(tr.c_endtime < starttime)
                continue;

            if( tr.c_starttime > starttime )
            {
                if((ts == 0) || (ts > tr.c_starttime) || ((ts == tr.c_starttime) &&(seq > tr.c_startseq)) )
                {
                    ts = tr.c_starttime;
                    seq = tr.c_startseq;
                    fetchseq = false;
                }
            } else
            {
                if(ts == 0 || ts > starttime )
                {
                    ts = starttime;
                    seq = 0;
                    fetchseq = true;
                }
            }

            found = true;
        }
        // upper bound specified. lower bound may or not be specified
        // find the nearest time to the upper bound
        else
        {
            //ignore non intersecting range
            if(tr.c_endtime < starttime)
                continue;
            if(tr.c_starttime > endtime)
                continue;

            if ( endtime < tr.c_endtime )
            {
                if(ts == 0 || ts < endtime )
                {
                    ts = endtime;
                    seq = 0;
                    fetchseq = true;
                }
            } else
            {
                if((ts == 0) ||  (ts < tr.c_endtime) || ((ts == tr.c_endtime) && (seq < tr.c_endseq)))
                {
                    ts = tr.c_endtime;
                    seq = tr.c_endseq;
                    fetchseq = false;
                }
            }

            found  = true;
        }
    }

    if(found && fetchseq)
    {
        if(!FetchSeqtoRecover(dbs,ts,seq))
        {
            DebugPrintf(SV_LOG_INFO, "A sequence point with the specified criteria  does not exist.\n");
            return false;
        }
    }

    if(!found)
    {
        DebugPrintf(SV_LOG_INFO, "A common crash consistent point could not be found with the specified criteria.\n");
    }

    return found;
}

bool CDPUtil::FetchSeqtoRecover(const std::vector<std::string> &  dbs, SV_ULONGLONG & ts,SV_ULONGLONG & seq)
{
    bool rv = true;
    svector_t::const_iterator svi = dbs.begin();
    svector_t::const_iterator sve = dbs.end();

    for( ; svi != sve; ++svi)
    {
        string db = *svi;
        SV_ULONGLONG currseq = 0;
        CDPDatabase database(db);
        if(!database.fetchseq_with_endtime_greq(ts,currseq))
        {
            rv = false;
            break;
        }

        if((seq == 0) || (currseq < seq))
        {
            seq = currseq;
        }
    }

    return rv;
}



bool CDPUtil::getNthEvent(const std::string & db,
                          const SV_EVENT_TYPE & type,
                          const SV_ULONGLONG & eventnum,
                          CDPEvent & event)
{
    bool rv = true;

    CDPMarkersMatchingCndn cndn;
    cndn.limit(eventnum);
    cndn.type(type);

    do
    {
        vector<CDPEvent> events;
        if(!CDPUtil::getevents(db, cndn, events))
        {
            rv = false;
            break;
        }
        size_t numevents = events.size();
        if(numevents != eventnum)
        {
            DebugPrintf(SV_LOG_ERROR,"please specify a valid event number.\nOnly %lu consistency event found for the specified application\n\n", (SV_ULONG)numevents);
            rv = false;
            break;
        }

        vector<CDPEvent>::reverse_iterator events_iter = events.rbegin();
        event = (*events_iter);
        rv = true;

    } while (0);

    return rv;

}
bool CDPUtil::runscript(const string & scriptname)
{
    std::string script = scriptname;

    if (script.empty() || '\0' == script[0] || "\n" == script )
        return true;

    CDPUtil::trim(script);
    string stuff_to_trim = " \n\b\t\a\r\xc" ;
    if (script.find_first_not_of(stuff_to_trim) == string::npos)
        return true;

    DebugPrintf(SV_LOG_INFO, "Running Script %s ...\n", script.c_str());
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
    ACE_Process_Options options;
    options.command_line("%s", script.c_str());

    pid_t pid = pm->spawn(options);

    if (ACE_INVALID_PID == pid) {
        std::ostringstream msg;
        msg << "script failed to run with error no: " <<  ACE_OS::last_error() << "\n";
        DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
        return false;
    }

    return CDPUtil::WaitForProcess(pm, pid);
}

bool CDPUtil::WaitForProcess(ACE_Process_Manager * pm, pid_t pid)
{
    std::ostringstream msg;

    ACE_Time_Value timeout;
    ACE_exitcode status = 0;
    //static const int TaskWaitTimeInSeconds = 5;

    while (true) 
    {

        //if (Quit(TaskWaitTimeInSeconds)) {
        //    TerminateProcess(pm, pid);
        //    msg << "script terminated beacuse of service shutdown request";
        //    m_err += msg.str();
        //    return false;
        //}

        pid_t rc = pm->wait(pid, timeout, &status); // just check for exit don't wait
        if (ACE_INVALID_PID == rc) 
        {
            msg << "script failed in with error no: " << ACE_OS::last_error() << '\n';
            DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
            return false;
        } 
        else if (rc == pid) 
        {
            if (0 != status) 
            {
                msg << "script failed with exit status " << status << '\n';
                DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
                return false;
            }
            return true;
        }

        ACE_OS::sleep(1);
    }
    return true;
}




#ifdef SV_WINDOWS
BOOL CtrlHandler( DWORD fdwCtrlType ) 
{ 
    switch( fdwCtrlType ) 
    { 
        // Handle the CTRL-C signal. 
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT: 
    case CTRL_BREAK_EVENT: 
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_QuitRequested = true;
        break;


    default:
        // should we set the quit request true ?
        g_QuitRequested = true;
        break;
    }

    return (TRUE);
}
#endif

void CDPUtil::Stop(int sig_no)
{
    //
    // Signal handlers must not call any glibc functions, such as
    // malloc, localtime_r, or fflush; deadlock will occur.
    //
    // TODO: we should also handle other signals
    g_QuitRequested = true;
}

void CDPUtil::SetupSignalHandler()
{

    ACE_Sig_Set stop_signals;
    stop_signals.empty_set();

    //  SIGHUP is a signal sent to a process when its controlling terminal is closed
#if defined (SIGHUP)
    stop_signals.sig_add(SIGHUP);
#endif

    // SIGINT is sent when the user on the process' controlling terminal 
    // presses the interrupt the running process key — typically Control-C, 
    // but on some systems, the "delete" character or "break" key.

#if defined (SIGINT)
    stop_signals.sig_add(SIGINT);
#endif

    // SIGTERM is the default signal sent to a process by the kill or killall commands. 
    // It causes the termination of a process, but unlike the SIGKILL signal, 
    // it can be caught and interpreted (or ignored) by the process. 
    // Therefore, SIGTERM is akin to asking a process to terminate nicely,
    // allowing cleanup and closure of files. For this reason, on many Unix systems 
    // during shutdown, init issues SIGTERM to all processes that are not essential 
    // to powering off, waits a few seconds, and then issues SIGKILL to forcibly 
    // terminate any such processes that remain.

#if defined (SIGTERM)
    stop_signals.sig_add(SIGTERM);
#endif

    ACE_Sig_Action sa(stop_signals, Stop);
}

bool CDPUtil::InitQuitEvent(bool callerCli, bool skipHandlerInit)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(callerCli)
        return CliInitQuitEvent(skipHandlerInit);
    else 
        return SvcInitQuitEvent();
}

bool CDPUtil::SvcInitQuitEvent()
{
    char EventName[256];
    g_clihandler = false;

    pid_t processId = ACE_OS::getpid();

    if (ACE_INVALID_PID == processId) {
        return false;
    }

    inm_sprintf_s(EventName, ARRAYSIZE(EventName), "%s%d", "InMageAgent", processId);
    if (NULL == g_QuitEvent) {
        // PR#10815: Long Path support
        g_QuitEvent = new ACE_Manual_Event(0, USYNC_PROCESS, ACE_TEXT_CHAR_TO_TCHAR(EventName));
        g_QuitRequested = false;
    }

    return true;
}

bool CDPUtil::CliInitQuitEvent(bool skipHandlerInit)
{
    g_clihandler = true;
    if (skipHandlerInit) {
        return true;
    }
#ifdef SV_WINDOWS
    if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE )) {
        DebugPrintf(SV_LOG_ERROR,"could not set up CTRL handler, error = %lu\n", 
            GetLastError());
        return false;
    }
#else 
    SetupSignalHandler();
#endif
    return true;
}

void CDPUtil::UnInitQuitEvent()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(g_clihandler) 
    {
        // TODO: we may want to reset the signal handler
        //       It is ok for time being since the process is anyway going to exit

    } else if (NULL != g_QuitEvent)
    {
        delete g_QuitEvent;
        g_QuitEvent = NULL;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


// --------------------------------------------------------------------------
// checks if service has requested a quit
// true: quit requested
// --------------------------------------------------------------------------
bool CDPUtil::QuitRequested(int waitTimeSeconds)
{
    if(g_QuitRequested)
        return true;

    if(g_clihandler)
    {
        if(waitTimeSeconds)
        {
            ACE_OS::sleep(waitTimeSeconds);
        }
        return g_QuitRequested;
    } else { // else using svc quit handling mechanism

        if(!g_QuitEvent) {
            DebugPrintf(SV_LOG_ERROR, "QuitEvent being used without initialization\n");
            return true;
        }

        ACE_Time_Value wait((time_t)waitTimeSeconds);
        if (0 == g_QuitEvent->wait(&wait, 0)) {
            g_QuitRequested = true;
            return true;
        } 
    }

    return false;
}

bool CDPUtil::Quit()
{
    return g_QuitRequested;
}

void CDPUtil::RequestQuit()
{
    g_QuitRequested = true;
}
void CDPUtil::ClearQuitRequest()
{
    g_QuitRequested = false;
}

void CDPUtil::SignalQuit()
{
    /* This mostly gets called from signal handler.
    * Hence should not call libc or debug printfs
    * DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); */
    g_QuitRequested = true;

    if(!g_clihandler)
    {
        if(!g_QuitEvent) 
        {
            /* TODO: comment this one too ? */
            /* DebugPrintf(SV_LOG_ERROR, "QuitEvent being used without initialization\n"); */
            return ;
        }

        g_QuitEvent ->signal();
    }
}

string CDPUtil::hash(const string & path)
{
    u_long hash = 0;

    for (size_t i = 0; i < path.size(); i++)
    {
        const char temp = path[i];
        hash = (hash << 4) + (temp * 13);

        u_long g = hash & 0xf0000000;

        if (g)
        {
            hash ^= (g >> 24);
            hash ^= g;
        }
    }

    char unique_name[20];
    inm_sprintf_s(unique_name, ARRAYSIZE(unique_name), "%016d", hash);
    return unique_name;
}

/*
* FUNCTION NAME : getPendingActionsDirectory()
*
* DESCRIPTION : Makes sure that the "pendingaction" directory exists in the cachedirectory and returns the path to it
*
* INPUT PARAMETERS :  none
*
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : returns the path of "pendingaction" directory
*
*/
string CDPUtil::getPendingActionsDirectory()
{
    LocalConfigurator localConfigurator;
    string cacheDir = localConfigurator.getCacheDirectory();
    cacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cacheDir += "pendingactions";
    cacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    securitylib::setPermissions(cacheDir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
    return cacheDir;
}

string CDPUtil::getPendingActionsFilePath(const std::string & volumename, const std::string & extn)
{
    string final_volname = volumename;
    // Convert the volume name to a standard format

    // remove repeating forward and backward slashes
    std::string::size_type index = 0;
    while (std::string::npos != (index = final_volname.find("\\\\", index))) {
        final_volname.erase(index, 1);
    }

    index = 0;
    while (std::string::npos != (index = final_volname.find("//", index))) {
        final_volname.erase(index, 1);
    }

    // remove any traling forward or backward slash
    std::string::size_type len = final_volname.length();
    if (len && ('\\' == final_volname[len - 1] || '/' == final_volname[len - 1])) {
        final_volname.erase(len -1);
    }

    FormatVolumeNameForCxReporting(final_volname);

    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(final_volname);
    }

    replace_nonsupported_chars(final_volname);
#ifdef SV_WINDOWS
    final_volname = ToUpper(final_volname);
#endif


    string pendingActionPath = CDPUtil::getPendingActionsDirectory();
    pendingActionPath += CDPUtil::hash(final_volname) ;
    pendingActionPath += extn;
    //DebugPrintf(SV_LOG_DEBUG, "rollback filename is %s\n", pendingActionPath.c_str());
    return pendingActionPath;
}

string CDPUtil::getPendingActionsFileBaseName(const std::string & volumename, const std::string & extn)
{
    string final_volname = volumename;
    // Convert the volume name to a standard format

    // remove repeating forward and backward slashes
    std::string::size_type index = 0;
    while (std::string::npos != (index = final_volname.find("\\\\", index))) {
        final_volname.erase(index, 1);
    }

    index = 0;
    while (std::string::npos != (index = final_volname.find("//", index))) {
        final_volname.erase(index, 1);
    }

    // remove any traling forward or backward slash
    std::string::size_type len = final_volname.length();
    if (len && ('\\' == final_volname[len - 1] || '/' == final_volname[len - 1])) {
        final_volname.erase(len -1);
    }

    FormatVolumeNameForCxReporting(final_volname);
    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(final_volname);
    }

    replace_nonsupported_chars(final_volname);
#ifdef SV_WINDOWS
    final_volname = ToUpper(final_volname);
#endif

    string pendingActionName = CDPUtil::hash(final_volname) ;
    pendingActionName += extn;
    //DebugPrintf(SV_LOG_DEBUG, "rollback filename is %s\n", pendingActionName.c_str());
    return pendingActionName;
}

SV_ULONGLONG CDPUtil::MaxSpacePerDataFile()
{ 
    LocalConfigurator localConfigurator;
    return localConfigurator.GetMaxSpacePerCdpDataFile();
}

SV_ULONGLONG CDPUtil::MaxTimeRangePerDataFile()
{ 
    LocalConfigurator localConfigurator;
    return localConfigurator.GetMaxTimeRangePerCdpDataFile();
}

/*
* FUNCTION NAME : getCurrentTimeAsString()
*
* DESCRIPTION : Gets the local time and returns it as a string
*
* INPUT PARAMETERS :  none
*
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : returns the current time as a string "(month-date-year hours:mins:secs): "
*
*/
std::string CDPUtil::getCurrentTimeAsString()
{
    time_t ltime;
    struct tm *today;
    time( &ltime );

#ifdef SV_WINDOWS
    today= localtime( &ltime );
#else
    struct tm t;
    localtime_r( &ltime, &t );
    today = &t;
#endif

    char present[30];
    memset(present,0,30);

    inm_sprintf_s(present, ARRAYSIZE(present), "(%02d-%02d-20%02d %02d:%02d:%02d): ", today->tm_mon + 1, today->tm_mday, today->tm_year - 100,
        today->tm_hour, today->tm_min, today->tm_sec);
    return present;
}

bool CDPUtil::CleanAllDirectoryContent(const string & dbpath)
{
    bool rv = true;
    do
    {
        ACE_stat statBuff;
        if( (sv_stat(getLongPathName(dbpath.c_str()).c_str(), &statBuff) != 0) )
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to stat %s, error no: %d\n",
                dbpath.c_str(), ACE_OS::last_error());
            break;
        }
        if( (statBuff.st_mode & S_IFDIR) == S_IFDIR )
        {
            ACE_DIR *pSrcDir = sv_opendir(dbpath.c_str());
            if(pSrcDir == NULL)
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to open directory %s, error no: %d\n",
                    dbpath.c_str(), ACE_OS::last_error());
                break;
            }
            struct ACE_DIRENT *dEnt = NULL;
            while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
            {
                std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
                std::string filename = dbpath;
                filename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                filename += dName;
                if ( dName == "." || dName == ".." )
                {
                    continue;
                }
                if(CDPUtil::QuitRequested())
                {
                    break;
                }
                if(!CleanAllDirectoryContent(filename))    
                {
                    rv = false;
                }            
            }
            ACE_OS::closedir(pSrcDir);
            if(ACE_OS::rmdir(getLongPathName(dbpath.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to remove directory %s, error no: %d\n",
                    dbpath.c_str(), ACE_OS::last_error());
            }
        }
        else if (ACE_OS::unlink(getLongPathName(dbpath.c_str()).c_str()) != 0)
        {
            DebugPrintf(SV_LOG_ERROR, "unlink failed, path = %s, errno = 0x%x\n", dbpath.c_str(), ACE_OS::last_error()); 
            rv = false;
        }

    }while(false);
    return rv;
}


// FUNCTION NAME :  CDPUtil::CleanPendingActionFiles
//
// DESCRIPTION: For target volume delete the files related to that replication pair from pending action directory
//
// INPUT PARAMETERS : String (target volume)
//
// OUTPUT PARAMETERS :  string (Error Message)
//
// NOTES : 
//
// return value : SVS_OK : the files are deleted, 
//                SVS_FALSE: the volume is dismounted, 
//                SVE_FAIL: not able to delete the files
//
SVERROR CDPUtil::CleanPendingActionFiles(const string & DeviceName, string & errorMessage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    SVERROR rv = SVS_OK;

    string extn = ".*";
    string PendingActionFolder = CDPUtil::getPendingActionsDirectory();
    string UnhideActionsFilePattern = CDPUtil::getPendingActionsFileBaseName(DeviceName, extn);


    if(CleanupDirectory(PendingActionFolder.c_str(), UnhideActionsFilePattern.c_str()).failed()){
        DebugPrintf(SV_LOG_ERROR, "Some files %s in the pending action directory %s could not be cleaned up\n"
            ,UnhideActionsFilePattern.c_str(),PendingActionFolder.c_str());
        errorMessage += "Some files ";
        errorMessage += UnhideActionsFilePattern;
        errorMessage += " in ";
        errorMessage += PendingActionFolder;
        errorMessage += " could not be cleaned up.Please clean up manually.";
        rv = SVE_FAIL;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "The pending action files %s inside the pending action directory %s are successfully cleanedup.\n"
            ,UnhideActionsFilePattern.c_str(),PendingActionFolder.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// FUNCTION NAME :  CDPUtil::CleanTargetCksumsFiles
//
// DESCRIPTION: For target volume delete the check sum files from cksums directory
//
// INPUT PARAMETERS : String (target volume)
//
// OUTPUT PARAMETERS :  None
//
// NOTES : 
//
// return value : true if successfully deleted, otherwise false
//
bool CDPUtil::CleanTargetCksumsFiles(const string & DeviceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    LocalConfigurator localconfigurator;
    std::string checksumDirName = localconfigurator.getTargetChecksumsDir();
    checksumDirName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    checksumDirName += DeviceName;
    checksumDirName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    string fileName = "*.*";
    if(CleanupDirectory(checksumDirName.c_str(), fileName.c_str()).failed()){
        DebugPrintf(SV_LOG_ERROR, "Some files %s in the cksums dir %s could not be cleaned up\n"
            ,fileName.c_str(),checksumDirName.c_str());
        rv = false;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "The files inside the cksums directory %s are successfully cleaned up.\n"
            ,checksumDirName.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// FUNCTION NAME :  CDPUtil::CleanRetentionPath
//
// DESCRIPTION: For target volume delete the retention path directory structure
//
// INPUT PARAMETERS : String DBName 
//
// NOTES : 
//
// return value : true if successfully deleted
//
bool CDPUtil::CleanRetentionPath(const string & dbdir)
{
    bool rv = true;
    std::string retention_id = "catalogue";
    std::string path_separator_id = ACE_DIRECTORY_SEPARATOR_STR_A;
    path_separator_id += retention_id;
    path_separator_id += ACE_DIRECTORY_SEPARATOR_STR_A;

    if(std::string::npos == dbdir.find(path_separator_id))
        return true;

    std::string current_directory =  dbdir;
    while(true)
    {
        if(ACE_OS::rmdir(getLongPathName(current_directory.c_str()).c_str()) != 0)
        {            
            if(ENOENT != ACE_OS::last_error())
            {
                DebugPrintf(SV_LOG_DEBUG, "Remove retention log directory failed, path = %s, errno = 0x%x\n",
                    current_directory.c_str(), ACE_OS::last_error());
                break;
            }
        }

        std::string dirbasename = basename_r(current_directory.c_str());
        if(dirbasename == retention_id)
        {
            break;
        }
        current_directory = dirname_r(current_directory.c_str());
    }    
    return rv;
}

// FUNCTION NAME :  CDPUtil::CleanRetentionLog
//
// DESCRIPTION: For target volume delete the retentioninformation
//
// INPUT PARAMETERS : String DBName 
//
// OUTPUT PARAMETERS :  string (Error Message)
//
// NOTES : 
//
// return value : true if successfully deleted, otherwise false
//
bool CDPUtil::CleanRetentionLog(const string dbname, string & errorMessage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    // PR#10815: Long Path support
    std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
    DirName(dbname.c_str(), &vdb_dir[0], vdb_dir.size());
    DebugPrintf(SV_LOG_DEBUG, "The deleted log path dir is %s\n", vdb_dir.data());
    if(!CleanAllDirectoryContent(std::string(vdb_dir.data())))
    {
        DebugPrintf(SV_LOG_ERROR, "Some files inside the retention directory %s could not be cleaned up\n", vdb_dir.data());
        errorMessage += "Some files in the directory path ";
        errorMessage += std::string(vdb_dir.data());
        errorMessage += "could not be cleaned up. Please clean up manually."; 
        rv = false;
    }
    else
    {
        if(CDPUtil::QuitRequested())
        {
            rv = false;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "The files in retention directory %s are successfully cleaned up.\n", vdb_dir.data());
            //clean up the directory structure
            if(CleanRetentionPath(std::string(vdb_dir.data())))
                rv = true;
            else
                rv = false;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// FUNCTION NAME :  CDPUtil::CleanCache
//
// DESCRIPTION: For target volume delete the cache derectory files
//
// INPUT PARAMETERS : String DBName 
//
// OUTPUT PARAMETERS :  string (Error Message)
//
// NOTES : 
//
// return value : true if successfully deleted, otherwise false
//
bool CDPUtil::CleanCache(const string & DeviceName, string & errorMessage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    LocalConfigurator localConfigurator;
    const std::string diffMatchFiles = "completed_ediffcompleted_diff_*.dat*" ;
    const std::string tmpMatchFiles  = "tmp_completed_ediffcompleted_diff_*.dat*";
    std::string cacheLocation = localConfigurator.getDiffTargetCacheDirectoryPrefix();
    cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
    cacheLocation += localConfigurator.getHostId();
    cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
    cacheLocation += GetVolumeDirName(DeviceName);

    SVERROR sve = CleanupDirectory(cacheLocation.c_str(), tmpMatchFiles.c_str());
    if (sve.failed()) 
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,"%s encountered error (%s) when performing cleanup on %s for %s\n",FUNCTION_NAME, sve.toString(), cacheLocation.c_str(), DeviceName.c_str());
    }

    sve = CleanupDirectory(cacheLocation.c_str(), diffMatchFiles.c_str());
    if (sve.failed()) 
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,"%s encountered error (%s) when performing cleanup on %s for %s\n",FUNCTION_NAME, sve.toString(), cacheLocation.c_str(), DeviceName.c_str());
    }
    if(!rv)
    {
        errorMessage += "Some files in the cache path ";
        errorMessage += cacheLocation;
        errorMessage += "could not be cleaned up. Please clean up manually."; 
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


int CDPUtil::disable_signal(int sigmin, int sigmax)
{
#ifndef SV_WINDOWS

    sigset_t signal_set;
    if (ACE_OS::sigemptyset (&signal_set) == - 1)
        return 1;

    for (int i = sigmin; i <= sigmax; i++)
        ACE_OS::sigaddset (&signal_set, i);

    //  Put the <signal_set>.
    if (ACE_OS::pthread_sigmask (SIG_BLOCK, &signal_set, 0) != 0)
        return 1;
#else
    ACE_UNUSED_ARG (sigmin);
    ACE_UNUSED_ARG (sigmax);
#endif 

    return 0;
}


/*
* FUNCTION NAME :  CDPUtil::moveRetentionLog
*
* DESCRIPTION : this function do the following
*                1. Check the pair is in pause maintenance state or not
*                2. Copy the retention from old location to new location
*                3. Update the new retention database for the new data directories
*                4. Delete the all the Vsnaps
*                5. Delete old retention log
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::moveRetentionLog(Configurator& configurator, std::map<std::string,std::string> const& moveparams)
{
    /* rv value "0 is for True", "1 is for False", "2 is for Quit Request" */
    int rv = 0; 
    bool returnValue = true;
    bool found = false;
    bool moveoperation_samevolumes = false;
    std::string deviceName;
    std::string curRetentionDir;
    std::string newRetentionDir;
    std::string curRetentionRoot;
    std::string newRetentionRoot;
    std::string db_name;
    std::string errMsg;
    do
    {

        std::map<std::string, std::string>::const_iterator it = moveparams.find("vol");
        if(it != moveparams.end())
        {
            deviceName = (*it).second;
        }
        else
        {
            errMsg = "The target volume is not specified.";
            DebugPrintf(SV_LOG_ERROR, "%s\n",errMsg.c_str());
            rv = 1;
            break;
        }
        it = moveparams.find("oldretentiondir");
        if(it !=  moveparams.end())
        {
            curRetentionDir = (*it).second;
        }
        DebugPrintf(SV_LOG_DEBUG,"current retention dir %s\n", curRetentionDir.c_str());

        it = moveparams.find("newretentiondir");
        if(it != moveparams.end())
        {
            newRetentionDir = (*it).second;
        }
        DebugPrintf(SV_LOG_DEBUG,"new retention dir %s\n", newRetentionDir.c_str());

        if(curRetentionDir.empty() || newRetentionDir.empty())
        {
            errMsg = "Move retention failed due to current retention directory or new retention directory is empty.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }

        if(!GetVolumeRootPath(curRetentionDir,curRetentionRoot))
        {
            errMsg = "Unable to determine the root of ";
            errMsg += curRetentionDir;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }

        if(!GetVolumeRootPath(newRetentionDir,newRetentionRoot))
        {
            errMsg = "Unable to determine the root of ";
            errMsg += newRetentionDir;
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }


        LocalConfigurator localconfigurator;
        bool allow_rootvolume_retention = localconfigurator.allowRootVolumeForRetention();
        if(!allow_rootvolume_retention)
        {
            if(curRetentionRoot == "/")
            {
                errMsg = "Move retention failed due to current retention path "; 
                errMsg += curRetentionDir;
                errMsg += " is mounted on /.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
                rv = 1;
                break;
            }
            
            if(newRetentionRoot == "/")
            {
                errMsg = "Move retention failed due to new retention path "; 
                errMsg += newRetentionDir;
                errMsg += " is mounted on /.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
                rv = 1;
                break;
            }
        }

        if(CDPUtil::QuitRequested(0))
        {
            DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So aborting the operation...\n" );
            rv = 2;
            break;
        }
        //Verify the pair is in pause for maintenance state
        bool shouldRun = false;
        std::string  operation = "moveretention";

        if(!ShouldOperationRun(configurator, deviceName,operation, shouldRun))
        {
            errMsg = "Unable to fetch the state information for the volume ";
            errMsg += deviceName;
            errMsg += ". Since there is no data available for this replication pair.";
            errMsg += "So the operation move retention aborted.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }
        if(!shouldRun)
        {
            errMsg = "The state of target volume ";
            errMsg += deviceName;
            errMsg += "is other than the state pause by maintenance.So the operation move retention aborted.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }
        db_name = curRetentionDir;
        db_name += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        db_name +="cdpv1.db";
        std::string new_dbName = newRetentionDir;
        new_dbName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        new_dbName +="cdpv1.db";

        //Copy the retention logs to new location.
        if(CDPUtil::QuitRequested(0))
        {
            DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So aborting the operation...\n");
            rv = 2;
            break;
        }

        //Delete Virtual Snapshots corresponding to the pair.
        VsnapMgr *vsnap;
#ifdef SV_WINDOWS
        WinVsnapMgr wmgr;
        vsnap = &wmgr;
#else
        UnixVsnapMgr umgr;
        vsnap = &umgr;
#endif
        DebugPrintf(SV_LOG_DEBUG, "Vsnaps for %s are being removed as retention logs are being moved\n", deviceName.c_str());
        bool nofail = true;
        if(!vsnap->UnMountVsnapVolumesForTargetVolume(deviceName, nofail))
        {
            errMsg = "Failed to delete vsnaps for ";
            errMsg += deviceName;
            errMsg += ". So the operation move retention operation aborted.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = 1;
            break;
        }
        
        if(CDPUtil::QuitRequested(0))
        {
            DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So aborting the operation...\n");
            rv = 2;
            break;
        }

        DebugPrintf(SV_LOG_DEBUG,"curRetentionDirRoot is %s\n", curRetentionRoot.c_str());
        DebugPrintf(SV_LOG_DEBUG,"newRetentionDirRoot is %s\n", newRetentionRoot.c_str());
        
        if( curRetentionRoot == newRetentionRoot ) 
        {
            std::string dirpath;
            std::string err;
            moveoperation_samevolumes = true;
            ACE_stat statBuff = {0};
            std::string Non_Sparse_dbname = newRetentionDir;
            Non_Sparse_dbname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            Non_Sparse_dbname +="cdpv1.db";
            std::string Sparse_dbname = newRetentionDir;
            Sparse_dbname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            Sparse_dbname +="cdpv3.db";
    
            if( (sv_stat(getLongPathName(curRetentionDir.c_str()).c_str(), &statBuff) == 0) ) 
            {
                dirpath = dirname_r(newRetentionDir.c_str());
                SVERROR rc = SVMakeSureDirectoryPathExists( dirpath.c_str() ) ;
                if( rc.failed() )
                {
                    errMsg = "newRetentionDir creation failed";
                    errMsg += newRetentionDir;
                    errMsg += "So the operation move retention aborted.";
                    DebugPrintf( SV_LOG_ERROR, "Directory creation %s failed with error %s\n",
                    dirpath.c_str(), rc.toString() ) ;
                    rv = 1;
                    break;
                }

                DebugPrintf(SV_LOG_DEBUG,"MoveDirContents function, dirpath %s\n", dirpath.c_str());

                if ( !InmRename(curRetentionDir.c_str(), newRetentionDir.c_str(), err) ) 
                {
                    DebugPrintf(SV_LOG_ERROR,"InmRename failed %s\n", err.c_str());    
                    moveoperation_samevolumes = false;
                }
            }
            else 
            {
                bool newRetentionDir_Non_Sparse_dbname = retentionFileExists(Non_Sparse_dbname);
                bool newRetentionDir_Sparse_dbname = retentionFileExists(Sparse_dbname);
                if((newRetentionDir_Non_Sparse_dbname) || (newRetentionDir_Sparse_dbname))
                {        
                    DebugPrintf (SV_LOG_DEBUG,"curRetentionDir %s doesn't exist, checking dbname on newRetentionDir %s.However Moveretention operation completed for samevolumes.\n",
                        curRetentionDir.c_str(), newRetentionDir.c_str());
                    rv = 0;
                    break;
                }
            }
        }
        

        if ( !moveoperation_samevolumes )
        {
            if(!CopyDirectoryContents(curRetentionDir,newRetentionDir))
            {
                errMsg = "There may not be enough free space available in ";
                errMsg += newRetentionDir;
                errMsg += " or failed to copy the retention log. So the operation move retention aborted.";
                DebugPrintf(SV_LOG_ERROR,
                    "Failed to copy the retention log %s to %s.\n",
                    curRetentionDir.c_str(),newRetentionDir.c_str());
                string error;
                if(!CDPUtil::CleanRetentionLog(new_dbName,error))
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Failed to clean the path %s with error %s\n",
                        newRetentionDir.c_str(),error.c_str());
                }
                rv = 1;
                break;
            }
        }

        //Update the new retention database for the new data directories
        if(!UpdateRetentionDataDir(new_dbName,newRetentionDir))
        {
            errMsg = "Failed to update the data log path in database. So the operation move retention aborted.";
            DebugPrintf(SV_LOG_ERROR,
                "Failed to update the data log path %s in database %s.\n",
                newRetentionDir.c_str(),new_dbName.c_str());
            string error;
            if(!CDPUtil::CleanRetentionLog(new_dbName,error))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "Failed to clean the path %s with error %s\n",
                    newRetentionDir.c_str(),error.c_str());
            }
            rv = 1;
            break;
        }

        if(CDPUtil::QuitRequested(0)) 
        {
            DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So aborting the operation...\n" );
            rv = 2;
            break;
        }
                    
    } while(false);

    if(rv != 2)
    {
        std::stringstream msg;
        std::string respmsg;
        if(errMsg.empty())
        {
            errMsg = "0";
        }
        DebugPrintf(SV_LOG_DEBUG, "rv = %d\n", rv);
        msg << "pause_components=yes;components=affected;pause_components_status=completed;pause_components_message =0;";
        msg << "move_retention=yes;move_retention_old_path=" << curRetentionDir;
        msg << ";move_retention_path=" << newRetentionDir << ";move_retention_status=";
        if( rv == 0 )
        {

            msg << "completed;move_retention_message=" << errMsg << ";";
        }
        else
        {
            msg << "failed;move_retention_message=" << errMsg << ";";
        }
        respmsg = msg.str();
        int hosttype = 1;
        DebugPrintf(SV_LOG_DEBUG, 
            "sending resp: %s for the move retention operation for the device %s \n",
            respmsg.c_str(),deviceName.c_str());
        returnValue = sendAndWaitForCxStateToUpdate(configurator, deviceName,hosttype,respmsg);
    }

    if(CDPUtil::QuitRequested(0))
    {
        DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So aborting the operation...\n" );
        rv = 2;
    }

    //Delete old retention logs and database
    if( rv == 0 )
    {
        std::string errormsg;
        if ( !moveoperation_samevolumes )
        {
            if(!CDPUtil::CleanRetentionLog(db_name,errormsg))
            {
                errMsg = "Move retention successful. Failed to clean the path ";
                errMsg += curRetentionDir;
                errMsg += " with error ";
                errMsg += errormsg;
                errMsg += ". Manualy delete the files.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            }
        }
    }
    return returnValue;
    
}



/*
* FUNCTION NAME :  CDPUtil::PolicyMgrTask
*
* DESCRIPTION : Whether a operation should run or not 
*
* INPUT PARAMETERS : std::string volume, std::string operation
*                    
*                    
* OUTPUT PARAMETERS : bool shouldrun
*                     
*
* NOTES :
*   
*
* return value : true if successfully fetched, false otherwise
*
*/
bool CDPUtil::ShouldOperationRun(Configurator& configurator, const std::string& volume, const std::string& operation, bool & shouldrun)
{
    bool rv = true;

    std::string volname = volume;
    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(volname);
    }
    else
    {
        /** 
        * This code is for solaris.
        * If the user has given 
        * Real Name of device, get 
        * the link name of it and 
        * then proceed to process.
        * since CX is having only the
        * link name for solaris agent
        */
        std::string linkname = volname;
        if (GetLinkNameIfRealDeviceName(volname, linkname))
        {
            volname = linkname;
        } 
        else
        {
            DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
            rv = false;
        }
    }
    if(volname != volume)
    {
        DebugPrintf(SV_LOG_INFO,
            " %s is a symbolic link to %s \n\n", volname.c_str(), volume.c_str());
    }

    FormatVolumeNameForCxReporting(volname);
    FirstCharToUpperForWindows(volname);

    shouldrun = configurator.shouldOperationRun(volname,operation);

    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::retentionFileExists
*
* DESCRIPTION : check whether the file exist or not
*
* INPUT PARAMETERS : FileName
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true if exist, false otherwise
*
*/
bool CDPUtil::retentionFileExists(const std::string & fileName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if (0 != sv_stat(getLongPathName(fileName.c_str()).c_str(), &statbuf))
        {
            rv = false;
            break;
        }

        if ( (statbuf.st_mode & S_IFREG)!= S_IFREG )
        {
            rv = false;
            break;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::CopyRetentionLog
*
* DESCRIPTION : this function do the following
*                1. Check the oldretention exist or not
*                2. Create the path for new retention if not exist
*                3. copy the retention files
*                
* INPUT PARAMETERS : std::string old retention path, std::string new retention path
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::CopyRetentionLog(const std::string & oldPathName, const std::string & newPathName)
{
    ACE_DIR *dirp = 0;
    dirent *dp = 0;
    std::string filePattern = "cdpv*";
    std::string db_name = oldPathName;
    db_name += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    db_name +="cdpv1.db";
    std::string nv_db_name = oldPathName;
    nv_db_name += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    nv_db_name +="cdpv3.db";
    bool oldretexist = retentionFileExists(db_name);
    bool old_nv_ret_db_exist = retentionFileExists(nv_db_name);
    if((!oldretexist) && (!old_nv_ret_db_exist))
    {
        return true;        
    }
    if(SVMakeSureDirectoryPathExists(newPathName.c_str()).failed())
    {
        std::string error = "Failed to create the path ";
        error += newPathName;
        DebugPrintf(SV_LOG_ERROR, "%s\n", error.c_str());
        return false;
    }
    // PR#10815: Long Path support
    if ((dirp = sv_opendir(oldPathName.c_str())) == NULL)
    {
        return false;
    }
    do {
        if ((dp = ACE_OS ::readdir(dirp)) != NULL)
        {
            if(RegExpMatch(filePattern.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
            {
                string FileName;
                FileName = oldPathName;
                FileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                FileName += ACE_TEXT_ALWAYS_CHAR(dp->d_name);
                ACE_stat FileStat ={0};
                // PR#10815: Long Path support
                if(sv_stat(getLongPathName(FileName.c_str()).c_str(),&FileStat)< 0)
                {
                    continue;
                }
                string newfilename = newPathName;
                newfilename += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                newfilename += ACE_TEXT_ALWAYS_CHAR(dp->d_name);
                ACE_stat NewFileStat ={0};
                /* we are reducing the copy time by checking the the file existance and comparing the size of the file */
                if(sv_stat(getLongPathName(newfilename.c_str()).c_str(),&NewFileStat)< 0)
                {
                    if(!CopyFile(FileName,newfilename))
                    {
                        (void)ACE_OS::closedir(dirp);
                        return false;
                    }
                }
                else
                {
                    if(NewFileStat.st_size < FileStat.st_size)
                    {
                        if(!CopyFile(FileName,newfilename))
                        {
                            (void)ACE_OS::closedir(dirp);
                            return false;
                        }
                    }
                }
            }
        }

    }while (dp != NULL);
    (void)ACE_OS::closedir(dirp);
    return true;
}



bool CDPUtil::CopyDirectoryContents(const std::string& src, const std::string& dest, bool recursive)
{
    bool rv = true;
    std::string filePattern = "cdp*";
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_stat statBuff = {0};
    do
    {
        if( (sv_stat(getLongPathName(src.c_str()).c_str(), &statBuff) != 0) )
        {
            DebugPrintf(SV_LOG_ERROR, "CDPUtil::CopyDirectoryContents Failed to stat file %s, error no: %d\n",
                src.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }
        if( (statBuff.st_mode & S_IFDIR) != S_IFDIR )
        {
            DebugPrintf(SV_LOG_ERROR, "CDPUtil::CopyDirectoryContents given file %s is not a directory\n",
                src.c_str());
            rv = false;
            break;
        }
        if(SVMakeSureDirectoryPathExists(dest.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to create %s, error no: %d\n",
                FUNCTION_NAME, dest.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }
#ifdef SV_WINDOWS
        LocalConfigurator localconfigurator;
        if(localconfigurator.CDPCompressionEnabled())
        {
            if(!EnableCompressonOnDirectory(dest))
            {
                stringstream l_stderr;
                l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error during enabling compression on CDP Log Directory :" 
                    << dest << " .\n"
                    << "Error Message: " << Error::Msg() << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
            }
        }
#endif

        ACE_DIR *pSrcDir = sv_opendir(src.c_str());
        if(pSrcDir == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to open directory %s, error no: %d\n",
                FUNCTION_NAME, src.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }

        struct ACE_DIRENT *dEnt = NULL;
        while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
        {
            std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
            if ( dName == "." || dName == ".." )
                continue;

            if(CDPUtil::QuitRequested(0))
            {
                break;
            }

            std::string srcPath = src + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
            std::string destPath = dest + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
            ACE_stat statbuf = {0};
            if( (sv_stat(getLongPathName(srcPath.c_str()).c_str(), &statbuf) != 0))
            {
                DebugPrintf(SV_LOG_ERROR, "CDPUtil::CopyDirectoryContents given file %s does not exist.\n",
                    srcPath.c_str());
                continue;
            }

            if((statbuf.st_mode & S_IFDIR) == S_IFDIR )
            {
                if(!recursive)
                    continue;
                if(!CopyDirectoryContents(srcPath, destPath))
                {
                    rv = false;
                    break;
                }
            }
            else if((statbuf.st_mode & S_IFREG) == S_IFREG)
            {
                //reducing the copy time by checking the the file existance in new location and comparing the size of the file
                ACE_stat NewFileStat ={0};
                if((sv_stat(getLongPathName(destPath.c_str()).c_str(), &NewFileStat) == 0) && 
                    (NewFileStat.st_size == statbuf.st_size))
                {
                    continue;
                }
                if(RegExpMatch(filePattern.c_str(), dName.c_str()))
                {
                    if(!CopyFile(srcPath,destPath))
                    {
                        DebugPrintf(SV_LOG_ERROR, "CDPUtil::CopyDirectoryContents Failed to copy file %s to %s.\n",
                            srcPath.c_str(),destPath.c_str());
                        rv = false;
                        break;
                    }
                }
            }
        }
        (void)ACE_OS::closedir(pSrcDir);

    } while(false);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::UpdateRetentionDataDir
*
* DESCRIPTION : Update the retention database for the new data directories
*
* INPUT PARAMETERS : std::string retention database name, std::string retention path name
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : Now this function is not implimented, this is for future reference
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::UpdateRetentionDataDir(const std::string & new_dbName,const std::string & newRetentionDir)
{
    bool rv = true;
    return rv;
}
/*
* FUNCTION NAME :  CDPUtil::sendAndWaitForCxStateToUpdate
*
* DESCRIPTION : 1.send the response to CX  
*                2.Wait for the state to update incase of move retention
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::sendAndWaitForCxStateToUpdate(Configurator& configurator, const std::string & deviceName,int hosttype,const std::string & responsemsg)
{
    bool rv = true;
    SV_ULONG pollingInterval = 10;
    
    do
    {
        
        if(sendMoveRetentionResponseToCX(configurator, deviceName,hosttype,responsemsg))
        {
            DebugPrintf(SV_LOG_DEBUG, 
                "The move retention message is successfully sent to cx for the device %s\n",
                deviceName.c_str());
            break;
        }
        
    }while(!CDPUtil::QuitRequested(10));
    /* wait for state to change or getting a service quit
    we are considering 2 conditions here
    1. user make a pause and did move retention and user come back to again user pause
    2. wait until the status of move retention updated in cx and that updation communicated to agent
    for not to instantiate another process from service */
    do
    {
        bool shouldRun = true;
        std::string operation = "moveretention";
        if(!ShouldOperationRun(configurator, deviceName,operation, shouldRun))
        {
            std::string errMsg = "Unable to fetch the state information for the volume ";
            errMsg += deviceName;
            errMsg += ". Since there is no data available for this replication pair in the local configurator.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.c_str());
            rv = false;
            break;
        }
        if(!shouldRun)
        {
            DebugPrintf(SV_LOG_DEBUG, 
                "The state for move retention is successfully updated for the device %s\n",
                deviceName.c_str());
            break;
        }
        DebugPrintf(SV_LOG_DEBUG, 
            "Waiting for the state to change for move retention for the device %s\n",
            deviceName.c_str());
            
    }while(!CDPUtil::QuitRequested(pollingInterval));
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::sendMoveRetentionResponseToCX
*
* DESCRIPTION : send the response to CX for move retention
*
* INPUT PARAMETERS : string volume name, int hosttype(target =1, source=2), string response message
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::sendMoveRetentionResponseToCX(Configurator& configurator, const std::string & deviceName,int hosttype, const std::string & responsemsg)
{
    bool rv = false;
    std::string cxVolName = deviceName;
    FormatVolumeNameForCxReporting(cxVolName);
    FirstCharToUpperForWindows(cxVolName);

    do
    {

        DebugPrintf(SV_LOG_DEBUG, "Sending the request to Central Management Server ...\n");
        if( !NotifyMaintenanceActionStatus(configurator, cxVolName,hosttype,responsemsg) )
        {
            DebugPrintf(SV_LOG_DEBUG, 
                "Request failed ... Communication with Central Management Server may be down.\n");
            rv = false;
            break;
        }

        DebugPrintf(SV_LOG_DEBUG, "%s", "\n\n Request has been sent successfully to CX server\n\n");
        rv = true;
    } while (0);
    
    return rv;
}




/*
* FUNCTION NAME :  CDPUtil::CopyFile
*
* DESCRIPTION :  Copy one file to other file 
*                ( if the destination file exist, it will truncate the file and write the content)
*
* INPUT PARAMETERS : std::string SourceFile, std::string DestinationFile
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPUtil::CopyFile(const std::string & SourceFile, const std::string & DestinationFile)
{
    bool rv = true;
    const std::size_t buf_sz = 65536;
    char buf[buf_sz]={0};
    ACE_HANDLE infile=0, outfile=0;
    ACE_stat from_stat = {0};
    // PR#10815: Long Path support
    if ( sv_stat(getLongPathName(SourceFile.c_str()).c_str(), &from_stat ) == 0
        && (infile = ACE_OS::open(getLongPathName(SourceFile.c_str()).c_str(), O_RDONLY )) != ACE_INVALID_HANDLE
        && (outfile = ACE_OS::open(getLongPathName(DestinationFile.c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC )) != ACE_INVALID_HANDLE )
    {
        ssize_t sz, sz_read=1, sz_write;
        while ( sz_read > 0
            && (sz_read = ACE_OS::read( infile, buf, buf_sz )) > 0 )
        {
            sz_write = 0;
            do
            {
                if ( (sz = ACE_OS::write( outfile, buf + sz_write, sz_read - sz_write )) < 0 )
                {
                    sz_read = sz; // cause read loop termination
                    break;        //  and error to be thrown after closes
                }
                sz_write += sz;
            } while ( sz_write < sz_read );
            if(CDPUtil::QuitRequested(0))
            {
                break;
            }
        }
        if ( ACE_OS::close( infile) < 0 ) 
            sz_read = -1;
        if ( ACE_OS::close( outfile) < 0 ) 
            sz_read = -1;
        if ( sz_read < 0 )
        {
            rv = false;
        }
    }
    else
    {
        rv = false;
        if ( outfile != ACE_INVALID_HANDLE) 
            ACE_OS::close( outfile );
        if ( infile != ACE_INVALID_HANDLE) 
            ACE_OS::close( infile );
    }
    return rv;    
}

std::string CDPUtil::get_last_fileapplied_info_location(const std::string& volumeName)
{
    LocalConfigurator localConfig;
    std::string appliedInfoPath = localConfig.getInstallPath();
    appliedInfoPath += ACE_DIRECTORY_SEPARATOR_STR_A;
    appliedInfoPath += "AppliedInfo";
    appliedInfoPath += ACE_DIRECTORY_SEPARATOR_STR_A;
    appliedInfoPath += localConfig.getHostId();
    appliedInfoPath += ACE_DIRECTORY_SEPARATOR_STR_A;
    appliedInfoPath += GetVolumeDirName(volumeName);
    appliedInfoPath += ACE_DIRECTORY_SEPARATOR_STR_A;
    appliedInfoPath += "cdplastprocessedfile.dat";
    return appliedInfoPath;
}


std::string CDPUtil::get_last_retentiontxn_info_location(const std::string& dbdir)
{
    std::string appliedInfoPath = dbdir;
    appliedInfoPath += ACE_DIRECTORY_SEPARATOR_STR_A;
    appliedInfoPath += "cdplastretentiontxn.dat";
    return appliedInfoPath;
}

/*
* FUNCTION NAME :  CDPUtil::get_last_fileapplied_information
*
* DESCRIPTION : retrives the applied information stored in the dat file
*
* INPUT PARAMETERS : 
*             volumeName - 
*                    
* OUTPUT PARAMETERS : 
*             appInfo - 
* 
* return value : true if successfull and false otherwise
*/
bool CDPUtil::get_last_fileapplied_information(const std::string& volumeName, CDPLastProcessedFile& appliedInfo)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string none = "none";
    memset(&appliedInfo, 0, sizeof(appliedInfo));
    inm_memcpy_s(appliedInfo.previousDiff.filename, sizeof(appliedInfo.previousDiff.filename), none.c_str(), none.length()+1);

    do
    {
        std::string appliedInfoFileName = get_last_fileapplied_info_location(volumeName);

        std::string appliedInfoFileLockName = appliedInfoFileName + ".lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(appliedInfoFileLockName.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        ACE_Guard<ACE_File_Lock> guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                FUNCTION_NAME,ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()),
                appliedInfoFileLockName.c_str());
            rv = false;
            break;
        }

        BasicIo appliedInfoFile;

        BasicIo::BioOpenMode mode =
            (BasicIo::BioReadExisting | 
            BasicIo::BioShareRead |
            BasicIo::BioBinary);


        appliedInfoFile.Open(appliedInfoFileName, mode);
        if(!appliedInfoFile.Good())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s file is not yet created.\n",
                appliedInfoFileName.c_str());
            break;
        }

        CDPLastProcessedFile appInfo;
        if(appliedInfoFile.Read( (char*)&appInfo, sizeof(CDPLastProcessedFile)) != sizeof(CDPLastProcessedFile))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to read applied information from %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }
        appliedInfoFile.Close();

        inm_memcpy_s(&appliedInfo, sizeof(appliedInfo), &appInfo, sizeof(appliedInfo));
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::update_last_fileapplied_information
*
* DESCRIPTION : updates the applied information stored in the dat file
*
* INPUT PARAMETERS : 
*             volumeName - 
*             appInfo - 
*                    
* OUTPUT PARAMETERS : none
* 
* return value : true if successfull and false otherwise
*/
bool CDPUtil::update_last_fileapplied_information(const std::string& volumeName, CDPLastProcessedFile& appliedInfo)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        std::string appliedInfoFileName = get_last_fileapplied_info_location(volumeName);

        std::string appliedInfoFileLockName = appliedInfoFileName + ".lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(appliedInfoFileLockName.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        ACE_Guard<ACE_File_Lock> guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                FUNCTION_NAME,ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()),
                appliedInfoFileLockName.c_str());
            rv = false;
            break;
        }

        BasicIo appliedInfoFile;
        BasicIo::BioOpenMode mode =
            (BasicIo::BioWriteAlways | 
            BasicIo::BioShareRead |
            BasicIo::BioBinary);

        appliedInfoFile.Open(appliedInfoFileName, mode);
        if(!appliedInfoFile.Good())
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to open %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        appliedInfo.hdr.magicBytes = APPLIED_INFO_HDR_MAGIC;
        appliedInfo.hdr.version = APPLIED_INFO_VERSION;
        if(appliedInfoFile.Write( (char*)&appliedInfo, sizeof(CDPLastProcessedFile)) != sizeof(CDPLastProcessedFile))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed writing applied information to %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        if(ACE_OS::fsync(appliedInfoFile.Handle()) == -1)
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Flush buffers failed for %s with error %d.\n",
                appliedInfoFileName.c_str(),
                ACE_OS::last_error());
            rv = false;
            break;
        }

        appliedInfoFile.Close();
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::get_last_retentiontxn
*
* DESCRIPTION : retrives the applied information stored in the dat file
*
* INPUT PARAMETERS : 
*             volumeName - 
*                    
* OUTPUT PARAMETERS : 
*             appInfo - 
* 
* return value : true if successfull and false otherwise
*/
bool CDPUtil::get_last_retentiontxn(const std::string& dbdir, CDPLastRetentionUpdate& lastRetentionUpdate)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string none = "none";
    memset(&lastRetentionUpdate, 0, sizeof(lastRetentionUpdate));
    inm_memcpy_s(lastRetentionUpdate.partialTransaction.diffFileApplied, sizeof(lastRetentionUpdate.partialTransaction.diffFileApplied), none.c_str(), none.length()+1);

    do
    {
        std::string appliedInfoFileName = get_last_retentiontxn_info_location(dbdir);

        std::string appliedInfoFileLockName = appliedInfoFileName + ".lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(appliedInfoFileLockName.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        ACE_Guard<ACE_File_Lock> guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                FUNCTION_NAME,ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()),
                appliedInfoFileLockName.c_str());
            rv = false;
            break;
        }

        BasicIo appliedInfoFile;

        BasicIo::BioOpenMode mode =
            (BasicIo::BioReadExisting | 
            BasicIo::BioShareRead |
            BasicIo::BioBinary);


        appliedInfoFile.Open(appliedInfoFileName, mode);
        if(!appliedInfoFile.Good())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s file is not yet created.\n",
                appliedInfoFileName.c_str());
            break;
        }

        CDPLastRetentionUpdate appInfo;
        if(appliedInfoFile.Read( (char*)&appInfo, sizeof(CDPLastRetentionUpdate)) != sizeof(CDPLastRetentionUpdate))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to read applied information from %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }
        appliedInfoFile.Close();

        inm_memcpy_s(&lastRetentionUpdate, sizeof(lastRetentionUpdate), &appInfo, sizeof(lastRetentionUpdate));
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::update_last_retentiontxn
*
* DESCRIPTION : updates the applied information stored in the dat file
*
* INPUT PARAMETERS : 
*             volumeName - 
*             appInfo - 
*                    
* OUTPUT PARAMETERS : none
* 
* return value : true if successfull and false otherwise
*/
bool CDPUtil::update_last_retentiontxn(const std::string& dbdir, CDPLastRetentionUpdate& appliedInfo)
{
    bool rv = true;   

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        std::string appliedInfoFileName = get_last_retentiontxn_info_location(dbdir);

        std::string appliedInfoFileLockName = appliedInfoFileName + ".lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(appliedInfoFileLockName.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        ACE_Guard<ACE_File_Lock> guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                FUNCTION_NAME,ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()),
                appliedInfoFileLockName.c_str());
            rv = false;
            break;
        }

        BasicIo appliedInfoFile;
        BasicIo::BioOpenMode mode =
            (BasicIo::BioWriteAlways | 
            BasicIo::BioShareRead |
            BasicIo::BioBinary);

        appliedInfoFile.Open(appliedInfoFileName, mode);
        if(!appliedInfoFile.Good())
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to open %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }

        appliedInfo.hdr.magicBytes = APPLIED_INFO_HDR_MAGIC;
        appliedInfo.hdr.version = APPLIED_INFO_VERSION;
        if(appliedInfoFile.Write( (char*)&appliedInfo, sizeof(CDPLastRetentionUpdate)) != sizeof(CDPLastRetentionUpdate))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed writing applied information to %s. error no:%d\n",
                appliedInfoFileName.c_str(), ACE_OS::last_error());
            rv = false;
            break;
        }


        if(ACE_OS::fsync(appliedInfoFile.Handle()) < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s fsync failed on %s. error:%d\n",
                FUNCTION_NAME, 
                appliedInfoFileName.c_str(),
                ACE_OS::last_error());
            rv = false;
        }
        else
        {
            rv = true;
            break;
        }

        appliedInfoFile.Close();
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPUtil::getevents(std::string const & dbname, CDPMarkersMatchingCndn const & cndn, std::vector<CDPEvent> & events)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    try
    {
        do 
        {
            CDPDatabase db(dbname);

            CDPDatabaseImpl::Ptr dbptr = db.FetchEvents(cndn);


            CDPEvent event;
            SVERROR hr;

            while (dbptr && (( hr = dbptr->read(event)) == SVS_OK ))
            {
                events.push_back(event);
            }

            if (!rv || hr.failed())
            {
                rv = false;
                break;
            }

        } while (0);
    }
    catch (std::exception & ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Unable to retrieve events from the database %s. Caught %s exception\n"
            ,FUNCTION_NAME, dbname.c_str(), ex.what());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

bool CDPUtil::printevents(std::string const & dbname, CDPMarkersMatchingCndn const & cndn, SV_ULONGLONG & eventCount,bool formated_display)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    do 
    {
        SV_ULONGLONG tsfc =0 , tslc = 0;
        CDPDatabase db(dbname);

        db.getrecoveryrange(tsfc, tslc);

        CDPDatabaseImpl::Ptr dbptr = db.FetchEvents(cndn);


        if(!formated_display)
        {
            cout << "-------------------------------------------------------------------------------\n";
            cout << setw(5)  << setiosflags( ios::left ) << "No."
                << setw(30) << setiosflags( ios::left ) << "TimeStamp(GMT)"
                << setw(13) << setiosflags( ios::left ) << "Accuracy"
                << setw(12) << setiosflags( ios::left ) << "Application"     
                << setiosflags( ios::left ) << "Event" << endl;        
            cout << "-------------------------------------------------------------------------------\n";
        }


        SV_ULONGLONG eventNum = 0;
        CDPEvent event;
        string appname;
        string displaytime;

        SVERROR hr;

        while (dbptr && (( hr = dbptr->read(event)) == SVS_OK ))
        {
            if (!VacpUtil::TagTypeToAppName(event.c_eventtype, appname))
            {
                rv =  false;
                break;
            }

            if (!CDPUtil::ToDisplayTimeOnConsole(event.c_eventtime, displaytime))
            {
                rv =  false;
                break;
            }
            ++eventNum;

            //to print modes
            string mode;
            if(event.c_eventmode == 0) mode = ACCURACYMODE0;
            else if(event.c_eventmode == 1) mode = ACCURACYMODE1;
            else if(event.c_eventmode == 2) mode = ACCURACYMODE2;

            string verified;
            if(event.c_verified == 0) 
            {
                verified = "No";
            }
            else
            {
                verified = "yes";
            }

            string lockstatus;
            if(event.c_lockstatus == BOOKMARK_STATE_LOCKEDBYUSER) 
            {
                lockstatus = "Locked by user";
            }
            else if(event.c_lockstatus == BOOKMARK_STATE_LOCKEDBYVSNAP) 
            {
                lockstatus = "Locked by vsnap";
            }
            else
            {
                lockstatus = "Unlocked";
            }
            if(!formated_display)
            {
                cout << setw(5)  << setiosflags( ios::left ) << eventNum
                    << setw(30) << setiosflags( ios::left ) << displaytime
                    << setw(13) << setiosflags( ios::left ) << mode
                    << setw(12) << setiosflags( ios::left ) << appname
                    << setiosflags( ios::left ) << event.c_eventvalue << endl;
            }
            else
            {
                SV_ULONGLONG event_age = tslc - event.c_eventtime;
                SV_ULONGLONG event_remaining_life = 0;
                if(event_age < event.c_lifetime)
                    event_remaining_life = (event.c_lifetime - event_age);


                cout << "Event No. = " << eventNum << "\n";
                cout << "TimeStamp(GMT) = " << displaytime << "\n";
                cout << "Accuracy = " << mode << "\n";
                cout << "Application = " << appname << "\n";
                cout << "Event = " << event.c_eventvalue << "\n";
                cout << "Identifier = " << event.c_identifier << "\n";
                cout << "Verified = " << verified << "\n";
                cout << "lockedstatus = " << lockstatus << "\n";
                cout << "Comment = " << event.c_comment << "\n";
                if(event.c_lifetime)
                {
                    if(event.c_lifetime < INM_BOOKMARK_LIFETIME_FOREVER)
                    {
                        cout << "LifeTime = " << event.c_lifetime 
                            << " ( " << CDPUtil::format_time_for_display(event.c_lifetime) << " )\n";
                        cout << "Remaining life = " << event_remaining_life
                            << " ( " << CDPUtil::format_time_for_display(event_remaining_life) << " )\n\n";
                    } else
                    {
                        cout <<"LifeTime = No limit\n\n";
                    }
                } else
                {
                    cout << "LifeTime = Not specified\n\n";
                }

            }
        }

        if (!rv || hr.failed())
        {
            rv = false;
            break;
        }

        if(!formated_display)
            cout << "Total Events:" << eventNum << "\n";
        eventCount = eventNum;

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

bool CDPUtil::displaybookmarks(std::set<CDPV3BookMark> & bookmarks, bool namevalueformat)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    do 
    {

        std::set<CDPV3BookMark>::iterator it = bookmarks.begin();

        if(!namevalueformat)
        {
            cout << "-------------------------------------------------------------------------------\n";
            cout << setw(5)  << setiosflags( ios::left ) << "No."
                << setw(30) << setiosflags( ios::left ) << "TimeStamp(GMT)"
                << setw(13) << setiosflags( ios::left ) << "Accuracy"
                << setw(12) << setiosflags( ios::left ) << "Application"     
                << setiosflags( ios::left ) << "Event" << endl;        
            cout << "-------------------------------------------------------------------------------\n";
        }


        SV_ULONGLONG eventNum = 0;
        CDPV3BookMark bkmk;
        string appname;
        string displaytime;


        while (it != bookmarks.end())
        {
            bkmk = (*it);
            ++it;

            if (!VacpUtil::TagTypeToAppName(bkmk.c_apptype, appname))
            {
                rv =  false;
                continue;
            }

            if (!CDPUtil::ToDisplayTimeOnConsole(bkmk.c_timestamp, displaytime))
            {
                rv =  false;
                continue;
            }
            ++eventNum;

            //to print modes
            string mode;
            if(bkmk.c_accuracy == 0) mode = ACCURACYMODE0;
            else if(bkmk.c_accuracy == 1) mode = ACCURACYMODE1;
            else if(bkmk.c_accuracy == 2) mode = ACCURACYMODE2;

            string verified;
            if(bkmk.c_verified == 0) 
            {
                verified = "No";
            }
            else
            {
                verified = "yes";
            }

            string lockstatus;
            if(bkmk.c_lockstatus == BOOKMARK_STATE_LOCKEDBYUSER) 
            {
                lockstatus = "Locked by user";
            }
            else if(bkmk.c_lockstatus == BOOKMARK_STATE_LOCKEDBYVSNAP) 
            {
                lockstatus = "Locked by vsnap";
            }
            else
            {
                lockstatus = "Unlocked";
            }
            if(!namevalueformat)
            {
                cout << setw(5)  << setiosflags( ios::left ) << eventNum
                    << setw(30) << setiosflags( ios::left ) << displaytime
                    << setw(13) << setiosflags( ios::left ) << mode
                    << setw(12) << setiosflags( ios::left ) << appname
                    << setiosflags( ios::left ) << bkmk.c_value << endl;
            }
            else
            {
                cout << "Event No. = " << eventNum << "\n";
                cout << "TimeStamp(GMT) = " << displaytime << "\n";
                cout << "Accuracy = " << mode << "\n";
                cout << "Application = " << appname << "\n";
                cout << "Event = " << bkmk.c_value << "\n";
                cout << "Identifier = " << bkmk.c_identifier << "\n";
                cout << "Verified = " << verified << "\n";
                cout << "lockedstatus = " << lockstatus << "\n";
                cout << "Comment = " << bkmk.c_comment << "\n\n";
            }
        }

        if (!rv)
        {
            break;
        }

        if(!namevalueformat)
            cout << "Total Bookmarks:" << eventNum << "\n";

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

bool CDPUtil::gettimeranges(std::string const & dbname, CDPTimeRangeMatchingCndn const & cndn, std::vector<CDPTimeRange> & timeranges)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    try
    {
        do 
        {
            CDPDatabase db(dbname);

            CDPDatabaseImpl::Ptr dbptr = db.FetchTimeRanges(cndn);


            CDPTimeRange timerange;
            string appname;
            string displaytime;

            SVERROR hr;

            while (dbptr && (( hr = dbptr->read(timerange)) == SVS_OK ))
            {
                timeranges.push_back(timerange);
            }

            if (!rv || hr.failed())
            {
                rv = false;
                break;
            }

        } while (0);
    }
    catch (std::exception & ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Unable to retrieve time ranges from the database %s. Caught %s exception\n"
            ,FUNCTION_NAME, dbname.c_str(), ex.what());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPUtil::getRetentionInfo
*
* DESCRIPTION : 
*   get the latest retention events and ranges(>=endTimeinNSecs)
*   from the db into RetentionInformation
*
* INPUT PARAMETERS : endTimeinNSecs
*
* OUTPUT PARAMETERS : retInfo
*
* NOTES :
*
* return value : true on success, false otherwise
*
*/

bool CDPUtil::getRetentionInfo(const std::string &dbname,
                               const SV_ULONGLONG& endTimeinNSecs, 
                               RetentionInformation& retInfo, bool dump)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        CDPDatabase db(dbname);
        CDPInformation info;
        db.get_retention_info(info);

        // fill log time range to retInfo.m_window
        std::string datefrom, dateto;
        if(!CDPUtil::ToDisplayTimeOnCx(info.m_summary.start_ts, datefrom))
        {
            rv = false;
            break;
        }
        if(!CDPUtil::ToDisplayTimeOnCx(info.m_summary.end_ts, dateto))
        {
            rv = false;
            break;
        }

        retInfo.m_window.m_startTime = datefrom;
        retInfo.m_window.m_startTimeinNSecs = info.m_summary.start_ts;
        retInfo.m_window.m_endTime = dateto;
        retInfo.m_window.m_endTimeinNSecs = info.m_summary.end_ts;

        CDPTimeRanges::iterator timeranges_iter = info.m_ranges.begin();
        CDPTimeRanges::iterator timeranges_end = info.m_ranges.end();

        for( ; timeranges_iter != timeranges_end ; ++timeranges_iter)
        {
            CDPTimeRange timerange = *timeranges_iter;
            std::string starttime, endtime;
            if(!CDPUtil::ToDisplayTimeOnCx(timerange.c_starttime, starttime))
            {
                rv = false;
                break;
            }
            if(!CDPUtil::ToDisplayTimeOnCx(timerange.c_endtime, endtime))
            {
                rv = false;
                break;
            }

            RetentionRange retRange;
            retRange.m_startTime = starttime;
            retRange.m_startTimeinNSecs = timerange.c_starttime;
            retRange.m_endTime = endtime;
            retRange.m_endTimeinNSecs = timerange.c_endtime;
            retRange.m_mode = timerange.c_mode;
            if( retRange.m_endTimeinNSecs >= endTimeinNSecs )
            {
                retInfo.m_ranges.push_back(retRange);
            }
        }


        CDPEvents::iterator events_iter = info.m_events.begin();
        CDPEvents::iterator events_end = info.m_events.end();
        for( ; events_iter != events_end ; ++events_iter)
        {
            CDPEvent event = *events_iter;
            std::string tagtime, appName;

            if(!CDPUtil::ToDisplayTimeOnCx(event.c_eventtime, tagtime))
            {
                rv = false;
                break;
            }
            if(!VacpUtil::TagTypeToAppName(event.c_eventtype, appName))
            {
                rv = false;
                break;
            }

            RetentionEvent retEvent;
            retEvent.m_EventTime = tagtime;
            retEvent.m_EventTimeinNSecs = event.c_eventtime;
            retEvent.m_appName = appName;
            retEvent.m_EventValue = event.c_eventvalue;
            retEvent.m_eventmode = event.c_eventmode;
            if( retEvent.m_EventTimeinNSecs >= endTimeinNSecs )
            {
                retInfo.m_events.push_back(retEvent);
            }
        } // end of for events iterator

        if(dump)
        {
            std::stringstream debugstream;
            debugstream << "retentionWindow:" << std::endl
                << "startTime=" << retInfo.m_window.m_startTime << "; "
                << "endTime=" << retInfo.m_window.m_endTime << "; "
                << "startTimeInSecs=" << retInfo.m_window.m_startTimeinNSecs << "; "
                << "endTimeInSecs=" << retInfo.m_window.m_endTimeinNSecs << "; "
                << std::endl;

            debugstream << "retentionRanges:" << std::endl;
            RetentionRanges::iterator retRangeIter = retInfo.m_ranges.begin();
            for(;retRangeIter != retInfo.m_ranges.end(); retRangeIter++)
            {
                debugstream << "startTime=" << retRangeIter->m_startTime << "; "
                    << "endTime=" << retRangeIter->m_endTime << "; "
                    << "startTimeInNSecs=" << retRangeIter->m_startTimeinNSecs << "; "
                    << "endTimeInNSecs=" << retRangeIter->m_endTimeinNSecs << "; "
                    << "accuracyMode=" << retRangeIter->m_mode << "; "
                    << std::endl;
            }

            debugstream << "retentionEvents:" << std::endl;
            RetentionEvents::iterator retEventIter = retInfo.m_events.begin();
            for(;retEventIter != retInfo.m_events.end(); retEventIter++)
            {
                debugstream << "eventTime=" << retEventIter->m_EventTime << "; "
                    << "eventTimeInNSecs=" << retEventIter->m_EventTimeinNSecs << "; "
                    << "applicationName=" << retEventIter->m_appName << "; "
                    << "tagName=" << retEventIter->m_EventValue << "; "
                    << "eventMode=" << retEventIter->m_eventmode << "; "
                    << std::endl;
            }
            DebugPrintf(SV_LOG_DEBUG, "%s", debugstream.str().c_str());
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


int CDPUtil::cdp_busy_handler(void * arg, int retry_count)
{
    DebugPrintf(SV_LOG_DEBUG, "FUNCTION:%s retry_count: %d\n", 
        FUNCTION_NAME, retry_count);

    if(CDPUtil::QuitRequested(1) || quitflag.value() )
        return 0;

    return 1;
}

void CDPUtil::UnmountVsnapsAssociatedWithInode(SV_ULONGLONG TimeToCheck,const std::string& volname)
{
    LocalConfigurator config;
    VsnapMgr *Vsnap;
#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    Vsnap = &obj;

#else
    UnixVsnapMgr unixobj;
    Vsnap = &unixobj;
#endif
    std::vector<VsnapPersistInfo> vsnaps;
    std::vector<VsnapPersistInfo> ::const_iterator vsnapiter ;
    do
    {
        if(!Vsnap->RetrieveVsnapInfo(vsnaps,"target",volname))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to retrieve the vsnap information for %s from persistent store.\n", volname.c_str());
            break;
        }
        vsnapiter = vsnaps.begin();
        while(vsnapiter != vsnaps.end())
        {
            if(TimeToCheck > vsnapiter->recoverytime)
            {
                VsnapVirtualVolInfo VirtualVolInfo;


                Vsnap->SetServerAddress(GetCxIpAddress().c_str());
                Vsnap->SetServerId(config.getHostId().c_str());
                Vsnap->SetServerPort(config.getHttp().port);

                VirtualVolInfo.VolumeName = vsnapiter->devicename;
                VirtualVolInfo.NoFail = true;

                VsnapVirtualInfoList UmountList;
                UmountList.push_back(&VirtualVolInfo);

                Vsnap->Unmount(UmountList, true);

                VsnapUnmountOnPruneAlert a(vsnapiter->devicename, volname);

                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
            }
            vsnapiter++;
        }
    }while (0);
}

void CDPUtil::UnmountVsnapsWithLaterRecoveryTime(SV_ULONGLONG TimeToCheck,const std::string& volname)
{
    LocalConfigurator config;
    VsnapMgr *Vsnap;
    DebugPrintf(SV_LOG_DEBUG,"ENTERING %s\n",FUNCTION_NAME);
#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    Vsnap = &obj;

#else
    UnixVsnapMgr unixobj;
    Vsnap = &unixobj;
#endif
    std::vector<VsnapPersistInfo> vsnaps;
    std::vector<VsnapPersistInfo> ::const_iterator vsnapiter ;
    do
    {
        if(!Vsnap->RetrieveVsnapInfo(vsnaps,"target",volname))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to retrieve the vsnap information for %s from persistent store.\n", volname.c_str());
            break;
        }
        vsnapiter = vsnaps.begin();
        while(vsnapiter != vsnaps.end())
        {
            if( vsnapiter->recoverytime >= TimeToCheck  )
            {
                VsnapVirtualVolInfo VirtualVolInfo;

                Vsnap->SetServerAddress(GetCxIpAddress().c_str());
                Vsnap->SetServerId(config.getHostId().c_str());
                Vsnap->SetServerPort(config.getHttp().port);

                VirtualVolInfo.VolumeName = vsnapiter->devicename;
                VirtualVolInfo.NoFail = true;

                VsnapVirtualInfoList UmountList;
                UmountList.push_back(&VirtualVolInfo);

                Vsnap->Unmount(UmountList, true);

                VsnapUnmountAlert a(vsnapiter->devicename, volname);

                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
            }
            vsnapiter++;
        }
    }while (0);
    DebugPrintf(SV_LOG_DEBUG,"LEAVING %s\n",FUNCTION_NAME);
}

void CDPUtil::UnmountVsnapsInTimeRange(const SV_ULONGLONG & start_ts,
                                       const SV_ULONGLONG & end_ts, const std::string& volname,
                                       const std::vector<CDPV3BookMark>& bookmarks_to_preserve)
{
    bool unmountvsnap = true;
    LocalConfigurator config;
    VsnapMgr* Vsnap;
    std::vector<VsnapPersistInfo> vsnaps;
    std::vector<CDPV3BookMark>::const_iterator cdpv3iter;

#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    Vsnap = &obj;
#else
    UnixVsnapMgr unixobj;
    Vsnap = &unixobj;
#endif
    do
    {
        //got the  vsnap info corresponding to target volume
        if(!Vsnap->RetrieveVsnapInfo(vsnaps, "target", volname))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to retrieve the vsnap information for %s from persistent store.\n", volname.c_str());
            break;
        }

        std::vector<VsnapPersistInfo>::const_iterator vsnapiter = vsnaps.begin();
        while(vsnapiter != vsnaps.end())
        {
            //Time stamp matches
            if(vsnapiter->recoverytime >= start_ts && vsnapiter->recoverytime < end_ts)
            {
                //
                // check if this vsnap is taken on one of preserved bookmarks
                // if it is not, go ahead and call vsnap unmount
                unmountvsnap = true;

                cdpv3iter = bookmarks_to_preserve.begin();
                while(cdpv3iter != bookmarks_to_preserve.end())
                {
                    if((vsnapiter->recoverytime == cdpv3iter->c_timestamp) && (vsnapiter->tag == cdpv3iter->c_value))
                    {
                        unmountvsnap = false;
                        break;
                    }
                    cdpv3iter++;
                }

                if(unmountvsnap)
                {
                    VsnapVirtualVolInfo VirtualVolInfo;
                    Vsnap->SetServerAddress(GetCxIpAddress().c_str());
                    Vsnap->SetServerId(config.getHostId().c_str());
                    Vsnap->SetServerPort(config.getHttp().port);

                    VirtualVolInfo.VolumeName = vsnapiter->devicename;
                    VirtualVolInfo.NoFail = true;

                    VsnapVirtualInfoList UmountList;
                    UmountList.push_back(&VirtualVolInfo);

                    Vsnap->Unmount(UmountList, true);

                    VsnapUnmountOnCoalesceAlert a(vsnapiter->devicename, volname);

                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
                }
            }
            vsnapiter++;
        }
    } while (0);
}

//#15949 :
std::string CDPUtil::getPairStateAsString(int ps)
{
    string rv;
    switch(ps)
    {
    case VOLUME_SETTINGS::DELETE_PENDING:
        rv = "DELETE_PENDING";
        break;
    case VOLUME_SETTINGS::CLEANUP_DONE:
        rv = "CLEANUP_DONE";
        break;
    case VOLUME_SETTINGS::CLEANUP_FAILED:
        rv = "CLEANUP_FAILED";
        break;
    case VOLUME_SETTINGS::PAUSE:
        rv = "PAUSE";
        break;
    case VOLUME_SETTINGS::PAIR_PROGRESS:
        rv = "PAIR_PROGRESS";
        break;
    case VOLUME_SETTINGS::LOG_CLEANUP_COMPLETE:
        rv = "LOG_CLEANUP_COMPLETE";
        break;
    case VOLUME_SETTINGS::LOG_CLEANUP_FAILED:
        rv = "LOG_CLEANUP_FAILED";
        break;
    case VOLUME_SETTINGS::UNLOCK_COMPLETE:
        rv = "UNLOCK_COMPLETE";
        break;
    case VOLUME_SETTINGS::UNLOCK_FAILED:
        rv = "UNLOCK_FAILED";
        break;
    case VOLUME_SETTINGS::CACHE_CLEANUP_COMPLETE:
        rv = "CACHE_CLEANUP_COMPLETE";
        break;
    case VOLUME_SETTINGS::CACHE_CLEANUP_FAILED:
        rv = "CACHE_CLEANUP_FAILED";
        break;
    case VOLUME_SETTINGS::VNSAP_CLEANUP_COMPLETE:
        rv = "VSNAP_CLEANUP_COMPLETE";
        break;
    case VOLUME_SETTINGS::VNSAP_CLEANUP_FAILED:
        rv = "VSNAP_CLEANUP_FAILED";
        break;
    case VOLUME_SETTINGS::PENDING_FILES_CLEANUP_COMPLETE:
        rv = "PENDING_FILES_CLEANUP_COMPLETE";
        break;
    case VOLUME_SETTINGS::PENDING_FILES_CLEANUP_FAILED:
        rv = "PENDING_FILES_CLEANUP_FAILED";
        break;
    case VOLUME_SETTINGS::PAUSE_PENDING:
        rv = "PAUSE_PENDING";
        break;
    case VOLUME_SETTINGS::RESTART_RESYNC_CLEANUP:
        rv = "RESTART_RESYNC_CLEANUP";
        break;
    case VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING:
        rv = "FLUSH_AND_HOLD_WRITES_PENDING";
        break;
    case VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES:
        rv = "FLUSH_AND_HOLD_WRITES";
        break;
    case VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING:
        rv = "FLUSH_AND_HOLD_RESUME_PENDING";
        break;
    default:
        rv = "UNKNOWN";
    }
    return rv;

}

bool CDPUtil::printCleanupStatus(std::string & cleanup_actions)
{
    const std::string delim = ";";
    std::vector<std::string> cleanup_tokens;
    Tokenize(cleanup_actions, cleanup_tokens, delim);

    cout << "   Cleanup Action: " << "\n";

    if("log_cleanup=yes" == cleanup_tokens[0])
    {
        cout << "   " << cleanup_tokens[0] << "\n";
        std::vector<std::string> log_status;
        Tokenize(cleanup_tokens[1],log_status,"=");

        cout << "   " << "log_cleanup_status=" <<  getPairStateAsString(atoi(log_status[1].c_str())) <<"\n";
        cout <<"   "<< cleanup_tokens[2] << "\n";
    }
    else
    {
        cout <<"   "<< cleanup_tokens[0] << "\n";
    }

    if("unlock_volume=yes" == cleanup_tokens[3])
    {
        cout <<"   "<< cleanup_tokens[3] << "\n";
        cout <<"   "<< cleanup_tokens[4] << "\n";
        cout <<"   "<< cleanup_tokens[5] << "\n";
        std::vector<std::string> unlock_status;
        Tokenize(cleanup_tokens[6],unlock_status,"=");
        cout <<"   "<< "unlock_vol_status=" <<  getPairStateAsString(atoi(unlock_status[1].c_str())) <<"\n";
        cout <<"   "<< cleanup_tokens[7] << "\n";
    }
    else
    {
        cout <<"   "<< cleanup_tokens[3] << "\n";
    }

    if("cache_dir_del=yes" == cleanup_tokens[8])
    {
        cout <<"   "<< cleanup_tokens[8] << "\n";
        std::vector<std::string> cache_status;
        Tokenize(cleanup_tokens[9],cache_status,"=");
        cout <<"   "<< "cache_dir_del_status=" <<  getPairStateAsString(atoi(cache_status[1].c_str())) <<"\n";

        cout <<"   "<< cleanup_tokens[10] << "\n";
    }
    else
    {
        cout <<"   "<< cleanup_tokens[8] << "\n";
    }

    if("pending_action_folder_cleanup=yes" == cleanup_tokens[11])
    {
        cout <<"   "<< cleanup_tokens[11] << "\n";
        std::vector<std::string> pending_status;
        Tokenize(cleanup_tokens[12],pending_status,"=");
        cout <<"   "<< "pending_action_folder_cleanup_status=" <<  getPairStateAsString(atoi(pending_status[1].c_str())) <<"\n";

        cout <<"   "<< cleanup_tokens[13] << "\n";
    }
    else
    {
        cout <<"   "<< cleanup_tokens[11] << "\n";
    }

    if("vsnap_cleanup=yes" == cleanup_tokens[14])
    {
        cout <<"   "<< cleanup_tokens[14] << "\n";
        std::vector<std::string> vsnap_status;
        Tokenize(cleanup_tokens[15],vsnap_status,"=");
        cout <<"   "<< "vsnap_cleanup_status=" <<  getPairStateAsString(atoi(vsnap_status[1].c_str())) <<"\n";

        cout <<"   "<< cleanup_tokens[16] << "\n";
    }
    else
    {
        cout <<"   "<< cleanup_tokens[14] << "\n";
    }

    return true;
}


bool CDPUtil::printMaintainenceStatus(std::string & maintainence_actions)
{
    const std::string delim = ";";
    std::vector<std::string> maintainence_tokens;
    Tokenize(maintainence_actions, maintainence_tokens, delim);

    cout << "   Maintainence Actions: " << "\n";

    if("pause_components=yes" == maintainence_tokens[0])
    {
        cout <<"   "<< maintainence_tokens[0] << "\n";
        cout <<"   "<< maintainence_tokens[1] << "\n";
        cout <<"   "<< maintainence_tokens[2] << "\n";
        cout <<"   "<< maintainence_tokens[3] << "\n";
    }
    else
    {
        cout <<"   "<< maintainence_tokens[0] << "\n";
    }

    return true;

}

//Utility method which converts time in hours to displayable string
// (x) years (y) weeks (z) days...
std::string CDPUtil::convertTimeToDiplayableString(unsigned int timeInHrs)
{
    stringstream policytime;
    int years = timeInHrs/(365*24);
    int months = (timeInHrs%(365*24))/(30*24);
    int week = ((timeInHrs%(365*24))%(30*24))/(7*24);
    int days = (((timeInHrs%(365*24))%(30*24))%(7*24))/24;
    int hrs = (((timeInHrs%(365*24))%(30*24))%(7*24))%24;

    if(years)
    {
        if(years >1)
        {
            policytime << years <<" years ";
        }
        else
            policytime << years <<" year ";

    }
    if(months)
    {
        if(months > 1)
        {
            policytime << months <<" months ";
        }
        else
            policytime << months <<" month ";

    }
    if(week)
    {
        if(week > 1)
        {
            policytime << week <<" weeks ";
        }
        else
            policytime << week <<" week ";
    }
    if(days)
    {
        if(days > 1)
        {
            policytime << days <<" days ";
        }
        else
            policytime << days <<" day ";
    }
    if(hrs)
    {
        if(hrs > 1)
        {
            policytime << hrs <<" hours ";
        }
        else
            policytime << hrs <<" hour ";
    }
    return policytime.str();
}

void CDPUtil::printVolumeSettings(VOLUME_GROUP_SETTINGS & vg, CDPSETTINGS_MAP & cdpSettings)
{
    string CDP_DIRECTION_STRING[] = { "UNSPECIFIED", "SOURCE", "TARGET" };
    string SYNC_MODE_STRING[] =  { "SYNC_DIFF", "SYNC_OFFLOAD", "SYNC_FAST", "SYNC_SNAPSHOT", "SYNC_DIFF_SNAPSHOT", "SYNC_QUASIDIFF", "SYNC_FAST_TBC" , "SYNC_DIRECT"  };
    string STATUS_STRING[] = { "PROTECTED", "UNPROTECTED" };
    string CDP_STATE_STRING[] =  { "DISABLED", "ENABLED", "PAUSED" };
    string SECURE_MODE_STRING[] = { "NO", "SECURE" };
    string TRANSPORT_PROTOCOL_STRING[] = { "FTP", "HTTP", "FILE", "UNKNOWN"};//0
    string COMPRESS_MODE_STRING[] = { "No Compression", "Enabled at CX-PS", "Enabled at Source"};
    string DEVICE_TYPE_STRING[] = { "DISK", "VSNAP_MOUNTED", "VSNAP_UNMOUNTED", "UNKNOWN_DEVICETYPE", "PARTITION", "LOGICAL_VOLUME",
        "VOLPACK", "CUSTOM_DEVICE", "EXTENDED_PARTITION", "DISK_PARTITION" };

    string VOLUME_STATE_STRING[] = { "VOLUME_HIDDEN", "VOLUME_VISIBLE_RO", "VOLUME_VISIBLE_RW" , "VOLUME_UNKNOWN", "VOLUME_DO_NOTHING" };//0
    string OS_VAL_STRING[] = { "UNKNOWN", "WIN_OS", "LINUX_OS", "SUN_OS", "HPUX_OS", "AIX_OS" };
    string BOOLEAN_STRING[] = { "NO", "YES" };
    string RESYNCREQ_CAUSE_STRING[] = { "SOURCE", "USER", "TARGET" };
    string POLICY_FAILACTION_STRING[] = { "Purge older retention logs" , "Pause differentials" };

    typedef std::list<VOLUME_GROUP_SETTINGS>::iterator VGITER;
    typedef VOLUME_GROUP_SETTINGS::volumes_t::iterator VITER;

    OS_VAL srcOS = OS_UNKNOWN;

    cout << "\nVOLUME_SETTINGS:" << endl;
    for(VITER viter = vg.volumes.begin();viter != vg.volumes.end(); ++viter)
    {
        VOLUME_SETTINGS vs = viter->second;
        cout << "Host Name : "<<vs.hostname<<"\t"<<"Direction: " << CDP_DIRECTION_STRING[vg.direction] <<"\t"
            << "status: " << STATUS_STRING[vg.status] << endl << endl;
        cout <<"-------------------------------------------------------------------------------------------"<<endl;
        cout <<setw(15)<<left<<" " <<setw(40)<<left<<"Source" <<"| " <<setw(40)<<left<<"Target" <<endl; 
        if(vg.direction == SOURCE)
        {
            std::list<VOLUME_SETTINGS>::iterator endptiter = vs.endpoints.begin();

            for(; endptiter != vs.endpoints.end(); ++endptiter)
            {
                cout <<"-------------------------------------------------------------------------------------------"<<endl;
                cout << setw(15) << left << "Host Name"<<setw(40)<<left<<vs.hostname <<  "| "<<setw(40)<<left<<(*endptiter).hostname << "\n";
                cout << setw(15) << left << "Volume" << setw(40) << left << viter->first << "| " <<setw(40) << left << (*endptiter).deviceName << "\n";
                cout << setw(15) << left << "Mount point" << setw(40) << left << vs.mountPoint << "| " << setw(40) << left << (*endptiter).mountPoint << "\n";
                cout << setw(15) << left << "Device Type"<<setw(40)<<left<<DEVICE_TYPE_STRING[vs.devicetype] << "| "<<setw(40)<<left<<DEVICE_TYPE_STRING[(*endptiter).devicetype] << endl;
                cout << setw(15) << left << "File System"<<setw(40) << left << vs.fstype << "| " << setw(40) << left << (*endptiter).fstype << endl;
                cout << setw(15) << left << "OS" << setw(40) << left << OS_VAL_STRING[vs.sourceOSVal] << "| " << setw(40) << left << OS_VAL_STRING[(*endptiter).sourceOSVal] << endl;
                cout << setw(15) << left << "Volume State" << setw(40) << left << VOLUME_STATE_STRING[vs.visibility] << "| " << setw(40) << left << VOLUME_STATE_STRING[(*endptiter).visibility] << endl;
                cout << setw(15) << left << "Host Id" << setw(40) << left << vs.hostId << "| "<<setw(40)<<left<<(*endptiter).hostId << endl;
                srcOS = vs.sourceOSVal;
            }

        }
        else if(vg.direction == TARGET)
        {
            std::list<VOLUME_SETTINGS>::iterator endptiter = vs.endpoints.begin();
            for(; endptiter != vs.endpoints.end(); ++endptiter)
            {
                cout <<"-------------------------------------------------------------------------------------------"<<endl;
                cout << setw(15) << left << "Host Name"<<setw(40)<<left<<(*endptiter).hostname <<  "| "<<setw(40)<<left<<vs.hostname << "\n";
                cout << setw(15) << left << "Volume" << setw(40) << left << (*endptiter).deviceName << "| " << setw(40) << left<< viter->first << "\n";
                cout << setw(15) << left << "Mount point"<<setw(40) << left << (*endptiter).mountPoint << "| " << setw(40) << left << vs.mountPoint << "\n";
                cout << setw(15) << left << "Device Type"<<setw(40)<<left<<DEVICE_TYPE_STRING[(*endptiter).devicetype] << "| "<<setw(40)<<left<<DEVICE_TYPE_STRING[vs.devicetype] << endl;
                cout << setw(15) << left << "File System"<<setw(40) << left << (*endptiter).fstype << "| " << setw(40) << left << vs.fstype << endl;
                cout << setw(15) << left << "OS" << setw(40) << left << OS_VAL_STRING[(*endptiter).sourceOSVal] << "| " << setw(40) << left << OS_VAL_STRING[vs.sourceOSVal] << endl;
                cout << setw(15) << left << "Volume State" << setw(40) << left << VOLUME_STATE_STRING[(*endptiter).visibility] << "| " << setw(40) << left << VOLUME_STATE_STRING[vs.visibility] << endl;
                cout << setw(15) << left << "Host Id"<<setw(40)<<left<<(*endptiter).hostId << "| "<<setw(40)<<left<<vs.hostId<< endl;
                srcOS = (*endptiter).sourceOSVal;
            }

        }
        cout <<"-------------------------------------------------------------------------------------------"<<endl<<endl;

        std::string resyncStartTime;
        std::string resyncEndTime;
    
        if (srcOS != SV_WIN_OS)
        {
            ToDisplayTimeOnConsole(vs.srcResyncStarttime + (SVEPOCHDIFF),resyncStartTime);
            ToDisplayTimeOnConsole(vs.srcResyncEndtime + (SVEPOCHDIFF),resyncEndTime);
        }
        else
        {
            ToDisplayTimeOnConsole(vs.srcResyncStarttime,resyncStartTime);
            ToDisplayTimeOnConsole(vs.srcResyncEndtime,resyncEndTime);
        }


        cout<< setw(40)<<left<< "Synchmode"<<": " << SYNC_MODE_STRING[vs.syncMode] << "\n"
            << setw(40)<<left<< "Pair State"<<": " << CDPUtil::getPairStateAsString(vs.pairState) << "\n";

        if(vs.pairState == VOLUME_SETTINGS::DELETE_PENDING )
        {
            printCleanupStatus(vs.cleanup_action);
        }
        else if(vs.pairState == VOLUME_SETTINGS::PAUSE_PENDING || vs.pairState == VOLUME_SETTINGS::PAUSE)
        {
            printMaintainenceStatus(vs.maintenance_action);
        }

        cout<< setw(40)<<left<< "Compression" <<": "<< COMPRESS_MODE_STRING[vs.compressionEnable] << "\n"
            << setw(40)<<left<< "Resync Required"<<": " << BOOLEAN_STRING[vs.resyncRequiredFlag] << "\n";

        if(vs.resyncRequiredFlag)
        {
            cout << setw(40)<<left<< " resync required cause"<<": " << RESYNCREQ_CAUSE_STRING[vs.resyncRequiredCause] << "\n"
                << setw(40)<<left<< " resync required timestamp"<<": " << vs.resyncRequiredTimestamp << "\n";
        }

        cout<< setw(40) << left << "Source Capacity" <<": "<< vs.sourceCapacity << "\n"
            << setw(40) << left << "Source raw size"<<": " << vs.sourceRawSize << "\n"
            << setw(40) << left << "Source Start Offset"<<": " << vs.srcStartOffset << "\n"
            << setw(40) << left << "Secure Transfer CX-PS to Destination"<<": " << BOOLEAN_STRING[vs.secureMode] << "\n"
            << setw(40) << left << "Secure Transfer from Source To CX-PS"<<": " << BOOLEAN_STRING[vs.sourceToCXSecureMode] << "\n"
            << setw(40) << left << "Job Id"<<": " << vs.jobId << "\n"
            << setw(40) << left << "Transport Protocol"<<": " << TRANSPORT_PROTOCOL_STRING[vs.transportProtocol] << "\n"
            << setw(40) << left << "RPO Threshold"<<": " << vs.rpoThreshold << "\n"
            << setw(40) << left << "Current RPO"<<": " << vs.currentRPO << "\n"
            << setw(40) << left << "Differentials pending in CX"<<": " << vs.diffsPendingInCX << "\n"
            << setw(40) << left << "Other Site Compatibility Number"<<": " << vs.OtherSiteCompatibilityNum << "\n";


        if(vs.throttleSettings.IsResyncThrottled())
        {
            cout<< setw(40) << left << "THROTTLE SETTINGS" << "\n"
                << setw(40) << left << "-----------------" << "\n"
                << setw(40) << left << " Source Throttled"<<": " << BOOLEAN_STRING[vs.throttleSettings.throttleSource] << "\n"
                << setw(40) << left << " Target Throttled"<<": " << BOOLEAN_STRING[vs.throttleSettings.throttleTarget] << "\n";

        }

        if(vs.sanVolumeInfo.isSanVolume)
        {
            cout<< setw(40) << left << "SAN VOLUME INFO" << "\n"
                << setw(40) << left << "----------------" << "\n"
                << setw(40) << left << " Physical Device Name"<<": " << vs.sanVolumeInfo.physicalDeviceName << "\n"
                << setw(40) << left << " Mirrored Device Name"<<": " << vs.sanVolumeInfo.mirroredDeviceName << "\n"
                << setw(40) << left << " Virtual Device Name"<<": " << vs.sanVolumeInfo.virtualDevicename << "\n"
                << setw(40) << left << " Virtual Name"<<": " << vs.sanVolumeInfo.virtualName << "\n"
                << setw(40) << left << " Physical Lun Id"<<": " << vs.sanVolumeInfo.physicalLunId << "\n";

        }

        if(vs.atLunStatsRequest.type)
        {
            cout<< setw(40) << left << "ATLUN STATS REQUEST" << "\n"
                << setw(40) << left << "-------------------" << "\n"
                << setw(40) << left << " Type"<<": " << vs.atLunStatsRequest.type << "\n"
                << setw(40) << left << " ATLUN Name"<<": " << vs.atLunStatsRequest.atLUNName << "\n"
                << setw(40) << left << " Physical Initiator PWWNs"<<": " "\n";
            std::list<std::string>::const_iterator pwwnstartiter = vs.atLunStatsRequest.physicalInitiatorPWWNs.begin();
            std::list<std::string>::const_iterator pwwnenditer = vs.atLunStatsRequest.physicalInitiatorPWWNs.end();
            for (std::list<std::string>::const_iterator iter = pwwnstartiter; iter != pwwnenditer; iter++)
            {
                cout << "        " << *iter << '\n';
            }

        }

        cout <<"\n\n"<< "Resync Details: "<<"\n"
            << "----------------"<<"\n"
            << setw(40)<<left<< " Resync Start time"<<": " << resyncStartTime << " ("<<vs.srcResyncStarttime<<")"<<"\n"
            << setw(40)<<left<< " Resync End time"<<": " << resyncEndTime << " ("<<vs.srcResyncEndtime<<")"<<"\n"
            << setw(40)<<left<< " Resync Start Seq"<<": " << vs.srcResyncStartSeq << "\n"
            << setw(40)<<left<< " Resync End Seq"<<": " << vs.srcResyncEndSeq << "\n"
            << setw(40) << left << "Resync Counter" <<": "<< vs.resyncCounter << "\n";

        typedef  CDPSETTINGS_MAP::const_iterator CDPSETTINGSITER;
        typedef  std::vector<cdp_policy>::const_iterator CDPPOLICYITER;

        CDPSETTINGSITER iter = cdpSettings.find(vs.deviceName);
        if(iter != cdpSettings.end())
        {
            CDP_SETTINGS cdpsettings  =iter->second;
            cout<<"\n\nRETENTION PLAN: \n";
            cout<<"---------------\n";
            stringstream policytime;
            int cdppolicytime = cdpsettings.TimePolicy()/(SECONDS_IN_AN_HOUR * HUNDREDS_OF_NANOSEC_IN_SECOND);

            cout << setw(40)<<left << " Retention"<<": " << CDP_STATE_STRING[cdpsettings.Status()] << "\n";
            if(cdpsettings.Status()) //Print details only if retention is enabled.
            {
                cout << setw(40)<<left<< " Log size limit (in MB)"<<": "  << cdpsettings.DiskSpace()/(1024*1024) << "\n"
                    << setw(40)<<left<< " Time limit"<<": "  << convertTimeToDiplayableString(cdppolicytime) << " (" << cdppolicytime <<" hrs )" << "\n"
                    << setw(40)<<left<< " Catalogue "<<": "  << cdpsettings.Catalogue() << "\n"
                    << setw(40)<<left<< " Disk Space Threshold(%)"<<": "  << cdpsettings.CatalogueThreshold() << "\n"
                    << setw(40)<<left<< " Unused Space (in MB)"<<": "  << cdpsettings.MinReservedSpace()/(1024*1024) << "\n"
                    << setw(40)<<left<< " On insufficient disk space"<<": "  << POLICY_FAILACTION_STRING[cdpsettings.OnSpaceShortage()] << "\n\n"
                    << setw(40)<<left<< " CDP_POLICY" << "\n";
                int i = 0;
                for(CDPPOLICYITER piter = cdpsettings.cdp_policies.begin();
                    piter != cdpsettings.cdp_policies.end();++piter)
                {
                    i++;
                    cout << "     ------------------------------------------------------" << "\n";

                    if(i == 1)
                    {
                        cout << "     Retain all data for " << convertTimeToDiplayableString(piter->Duration())<<"\n";
                    }
                    else
                    {
                        std::string appName;
                        VacpUtil::TagTypeToAppName(piter->AppType(), appName);
                        cout << "     Data older than " << convertTimeToDiplayableString(piter->Start()) <<"\n"
                            <<"     Retain "<<piter->TagCount()<<" "<<appName<< " bookmark per " 
                            <<convertTimeToDiplayableString(piter->Granularity()) << "for " <<convertTimeToDiplayableString(piter->Duration())<<"\n";
                    }

                    for(std::vector<std::string>::const_iterator it = piter->userbookmarks.begin();
                        it != piter->userbookmarks.end();++it)
                    { 
                        //if(!((*it).empty()))
                        {
                            cout << "      user event:" << (*it) << endl;
                        }
                    }
                }
                cout << "     ------------------------------------------------------" << "\n";
            }
        }


        cout << "\n\n" << "TRANSPORT CONNECTION SETTINGS:" << "\n"
            <<"------------------------------" << "\n"
            << setw(40)<<left<< " Ip Address"<<": " << vg.transportSettings.ipAddress<< "\n"             
            << setw(40)<<left<< " Port"<<": " << vg.transportSettings.port<< '\n'
            << setw(40)<<left<< " SSL port"<<": " << vg.transportSettings.sslPort<< '\n'
            << setw(40)<<left<< " FTP port"<<": " << vg.transportSettings.ftpPort<< '\n'
            << setw(40)<<left<< " User"<<": " << vg.transportSettings.user << "\n"
            << setw(40)<<left<< " Password"<<": " << vg.transportSettings.password << "\n"
            << setw(40)<<left<< " Active Mode"<<": " << vg.transportSettings.activeMode << "\n"
            << setw(40)<<left<< " ConnectTimeout"<<": " << vg.transportSettings.connectTimeout << "\n"
            << setw(40)<<left<< " Response Timeout"<<": " << vg.transportSettings.responseTimeout << "\n"             
            << endl;
        
        if (!vs.options.empty()) {
            cout << std::endl << std::endl << "options:" << std::endl;

            for (std::map<std::string, std::string>::const_iterator it = vs.options.begin(); it != vs.options.end(); it++)
            {
                cout << setw(40) << left << it->first << ": " << it->second << std::endl;
            }
        }

    }

}


//Utility method which converts time in hundred nanosecs to displayable string
// (y) years (w) weeks (d) days (h) hours (m) minutes (s) secs
std::string CDPUtil::format_time_for_display(const unsigned long long &time_hns)
{
    stringstream lifetime;
    int years = time_hns/(HUNDREDS_OF_NANOSEC_IN_YEAR);
    int months = (time_hns%(HUNDREDS_OF_NANOSEC_IN_YEAR))/(HUNDREDS_OF_NANOSEC_IN_MONTH);
    int week = ((time_hns%(HUNDREDS_OF_NANOSEC_IN_YEAR))%(HUNDREDS_OF_NANOSEC_IN_MONTH))/(HUNDREDS_OF_NANOSEC_IN_WEEK);
    int days = (((time_hns%(HUNDREDS_OF_NANOSEC_IN_YEAR))%(HUNDREDS_OF_NANOSEC_IN_MONTH))%(HUNDREDS_OF_NANOSEC_IN_WEEK))/(HUNDREDS_OF_NANOSEC_IN_DAY);
    int hrs =  ((((time_hns%(HUNDREDS_OF_NANOSEC_IN_YEAR))%(HUNDREDS_OF_NANOSEC_IN_MONTH))%(HUNDREDS_OF_NANOSEC_IN_WEEK))%(HUNDREDS_OF_NANOSEC_IN_DAY))/(HUNDREDS_OF_NANOSEC_IN_HOUR);
    int mins = (((((time_hns%(HUNDREDS_OF_NANOSEC_IN_YEAR))%(HUNDREDS_OF_NANOSEC_IN_MONTH))%(HUNDREDS_OF_NANOSEC_IN_WEEK))%(HUNDREDS_OF_NANOSEC_IN_DAY))%(HUNDREDS_OF_NANOSEC_IN_HOUR))/(HUNDREDS_OF_NANOSEC_IN_MINUTE);

    if(years)
    {
        if(years >1)
        {
            lifetime << years <<" years ";
        }
        else
            lifetime << years <<" year ";

    }
    if(months)
    {
        if(months > 1)
        {
            lifetime << months <<" months ";
        }
        else
            lifetime << months <<" month ";

    }
    if(week)
    {
        if(week > 1)
        {
            lifetime << week <<" weeks ";
        }
        else
            lifetime << week <<" week ";
    }
    if(days)
    {
        if(days > 1)
        {
            lifetime << days <<" days ";
        }
        else
            lifetime << days <<" day ";
    }
    if(hrs)
    {
        if(hrs > 1)
        {
            lifetime << hrs <<" hours ";
        }
        else
            lifetime << hrs <<" hour ";
    }

    if(mins)
    {
        if(mins > 1)
        {
            lifetime << mins <<" minutes ";
        }
        else
            lifetime << mins << " minute ";
    }


    return lifetime.str();
}

//
// FUNCTION NAME :  parse_and_get_specifiedcolumn
//
// DESCRIPTION : Fetches the  'column' data from file.column field start at 0 (i.e. first column should call with '0' and so on
//        
// INPUT PARAMETERS : filename : Name of file 
//                      delim : delimter (',' , ':' or group of delimiters can be passed which will be used to tokenize)
//                      column : coulmn number to fetch.It starts from 0.
// OUTPUT PARAMETERS : vector of tokens
//
//
//
bool CDPUtil::parse_and_get_specifiedcolumn(const std::string& filename,const std::string& delim,const int& column,std::vector<std::string>& tokenlist)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    std::string line;
    try
    {

        ifstream inputfile(filename.c_str());
        //std::string delimter = std::string(delim);
        if (!inputfile.is_open()) 
        {
            DebugPrintf(SV_LOG_ERROR," Failed to open file  :%s for reading @LINE %d in FILE %s\n",filename.c_str(),LINE_NO,FILE_NAME);
            rv = false;
            return rv;
        }
        svector_t tokens;
        while (getline(inputfile,line))
        {
            tokens.clear();
            //CDPUtil::split will split the input into "column" number. If we specify field as 2,the line will be divided into 2 parts.
            tokens = CDPUtil::split(line,delim,column+1);

            if(tokens.size() != column+1)
            {
                DebugPrintf(SV_LOG_ERROR," Invalid no of tokens found in input :%s. Expected tokens count:%d @LINE %d in FILE %s\n",line.c_str(),column+1,LINE_NO,FILE_NAME);
                rv = false;
                break;
            }
            tokenlist.push_back(tokens[column]);
        }
        inputfile.close();
    }
    catch( std::exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), filename.c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, filename.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
    return rv;
}

//
// FUNCTION NAME :  get_filenames_withpattern
//
// DESCRIPTION : Fetches the list of specific files which matches given pattern from given directory
//        
// INPUT PARAMETERS : dirname : Directory Name 
//                      pattern : suffix name (e.g. all files ending with '.log' suffix or 'txt'.
//                    
// OUTPUT PARAMETERS : vector of desired files 
//
//
//
bool CDPUtil::get_filenames_withpattern(const std::string & dirname,const std::string& pattern,std::vector<std::string>& filelist, unsigned int limit)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s %s\n", FUNCTION_NAME, dirname.c_str(), pattern.c_str());
    ACE_stat statBuff = {0};
    unsigned int fileMatchCount = 0;

    do
    {
        if( (sv_stat(getLongPathName(dirname.c_str()).c_str(), &statBuff) != 0) )
        {
            DebugPrintf(SV_LOG_ERROR, "  Failed to stat file %s, error no: %d @LINE %d in FILE %s\n",
                dirname.c_str(),ACE_OS::last_error(),LINE_NO,FILE_NAME);
            rv = false;
            break;
        }
        if( (statBuff.st_mode & S_IFDIR) != S_IFDIR )
        {           
            DebugPrintf(SV_LOG_ERROR, "Given file %s is not a directory,@LINE %d in FILE %s\n",
                dirname.c_str(),LINE_NO,FILE_NAME);
            rv = false;
            break;
        }
        ACE_DIR *pSrcDir = sv_opendir(dirname.c_str());
        if(pSrcDir == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to open directory %s, error no: %d @LINE %d in FILE %s\n",
                FUNCTION_NAME, dirname.c_str(),ACE_OS::last_error(),LINE_NO,FILE_NAME);
            rv = false;
            break;
        }

        struct ACE_DIRENT *dEnt = NULL;
        while ( ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL) && 
                ((NoLimit == limit) || (fileMatchCount < limit) )
            )
        {
            std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
            if ( dName == "." || dName == ".." )
                continue;

            std::string srcPath = dirname + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
            DebugPrintf(SV_LOG_DEBUG, "checking %s for match\n", dName.c_str());
            ACE_stat statbuf = {0};
            if( (sv_stat(getLongPathName(srcPath.c_str()).c_str(), &statbuf) != 0))
            {
                //DebugPrintf(SV_LOG_ERROR, "Given file %s does not exist.@LINE %d in FILE %s\n", srcPath.c_str(),LINE_NO,FILE_NAME);
                //rv = false;
                //break;
                continue;
            }
            if((statbuf.st_mode & S_IFREG) == S_IFREG)
            {
                if (RegExpMatch(pattern.c_str(), dName.c_str()))
                {
                    filelist.push_back(dName);
                    DebugPrintf(SV_LOG_DEBUG, "found %s\n", dName.c_str());
                    ++fileMatchCount;
                }

            }
        }
        ACE_OS::closedir(pSrcDir);
    } while(false);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s %s\n", FUNCTION_NAME, dirname.c_str(), pattern.c_str());
    return rv;
}

bool CDPUtil::validate_cdp_settings(std::string volume_name,CDP_SETTINGS & settings)
{
    // verify that the cdp_status is valid
    bool valid = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME,  volume_name.c_str());

    do
    {
        if(settings.Status() != CDP_DISABLED &&
            settings.Status() != CDP_ENABLED &&
            settings.Status() != CDP_PAUSED)
        {
            valid = false;
            DebugPrintf(SV_LOG_ERROR,"Invalid cdp state recieved from CS for volume %s : %u\n",volume_name.c_str(),settings.Status());
            break;
        }
        //Version validation.
        if(settings.cdp_version != 0 && //Disabled
            settings.cdp_version != 1 && //Non-sparse
            settings.cdp_version != 3) //Sparse
        {
            valid = false;
            DebugPrintf(SV_LOG_ERROR,
                "Invalid cdp version recieved from CS for volume %s : %u\n",
                volume_name.c_str(),settings.cdp_version);
            break;
        }

        if(settings.Status() == CDP_DISABLED || settings.Status() == CDP_PAUSED)
        {
            // if cdp is disabled or in paused, no further checks
            valid = true;
            break;
        }

        //Alert threshold validation.
        if(settings.cdp_alert_threshold > 100)
        {
            valid = false;
            DebugPrintf(SV_LOG_ERROR,"Invalid alert threshold recieved from CS for volume %s : %u\n",volume_name.c_str(),settings.cdp_alert_threshold);
        }

        if(settings.cdp_onspace_shortage != CDP_PRUNE && 
            settings.cdp_onspace_shortage != CDP_STOP)
        {
            valid = false;
            DebugPrintf(SV_LOG_ERROR,"Invalid cdp oninsufficient-space policy recieved from CS for volume %s : %u\n",volume_name.c_str(),settings.Status());
            break;
        }

        //To validate policies verify 'start' for each policy.
        //The start time should be equal to previous start and duration.
        //Verify that the apptype is valid.
        SV_ULONGLONG    prev_start = 0;
        SV_ULONGLONG    prev_duration = 0;
        SV_UINT apptype = 0;
        std::string appName;
        bool all_changes = true; //first policy will be all changes.
        std::vector<cdp_policy>::const_iterator piter;


        for(piter = settings.cdp_policies.begin();
            piter != settings.cdp_policies.end();++piter)
        {

            if(!all_changes && (0 == piter -> Granularity()))
            {
                valid = false;
                DebugPrintf(SV_LOG_ERROR,
                    "Invalid cdp policies (unexpected all-writes policy) recieved from CS for volume %s\n",
                    volume_name.c_str());
            }

            if(0 != piter ->Granularity())
            {
                all_changes = false;
            }

            if(!all_changes)
            {

                if(0 == apptype)
                {
                    apptype = piter -> AppType();

                } else if(apptype != piter ->AppType())
                {
                    valid = false;
                    DebugPrintf(SV_LOG_ERROR,
                        "Invalid cdp policies recieved from CS (application name are not same for all levels) for volume %s : previous level: %u current level:%u\n",
                        volume_name.c_str(), apptype, piter->AppType());
                    break;
                }

                if(STREAM_REC_TYPE_USERDEFINED_EVENT != piter->AppType())
                {
                    if(piter ->TagCount() == 0)
                    {
                        valid = false;
                        DebugPrintf(SV_LOG_ERROR,
                            "Invalid cdp policies recieved from CS (bookmarks to retain is zero) for volume %s\n",
                            volume_name.c_str());
                        break;                
                    }
                }

                if( !VacpUtil::TagTypeToAppName(piter->AppType(), appName))
                {
                    valid = false;
                    DebugPrintf(SV_LOG_ERROR,
                        "Invalid cdp policies (invalid application name) recieved from CS for volume %s : %u\n",
                        volume_name.c_str(),piter->AppType());
                    break;
                }
            }

            // we can have only one 'all-writes' window
            all_changes = false;

            if(piter->Start() != (prev_start+prev_duration))
            {
                valid = false;
                DebugPrintf(SV_LOG_ERROR,
                    "Inconsistent policy durations for volume %s, previous policy end " ULLSPEC" current policy start " ULLSPEC"\n",
                    volume_name.c_str(),(prev_start+prev_duration),piter->Start());
                break;
            }

            prev_start = piter->Start();
            prev_duration = piter->Duration();
        }

        if(valid && (prev_start+prev_duration) !=  settings.cdp_timepolicy)
        {
            valid = false;
            DebugPrintf(SV_LOG_ERROR,
                "Incosistent time policy for volume %s, total time " ULLSPEC" last policy end " ULLSPEC"\n",
                volume_name.c_str(),settings.cdp_timepolicy,(prev_start+prev_duration));
            break;
        }


    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME,  volume_name.c_str());
    return valid;
}
/*
* The free space trigger should be factored based on the size of retention volume bound by a lower threshold and an upper thresold.
* The default free space trigger is 5% of the retention volume capacity.
* The lower limit is 1GB, upper limit is 4gb.
*/
bool CDPUtil::get_lowspace_value(const std::string &retentiondir,SV_ULONGLONG &lowspace_trigger)
{
    bool rv = true;
    LocalConfigurator localconfigurator;
    SV_ULONGLONG lowspace_trigger_lower_threshold = localconfigurator.getCdpLowSpaceTriggerLowerThreshold();
    SV_ULONGLONG lowspace_trigger_upper_threshold = localconfigurator.getCdpLowSpaceTriggerUpperThreshold();
    SV_ULONGLONG lowspace_trigger_percentage = localconfigurator.getCdpLowSpaceTriggerPercentage();

    SV_ULONGLONG diskFreeSpace = 0;
    SV_ULONGLONG quota = 0;
    SV_ULONGLONG capacity = 0 ;

    if(!GetDiskFreeSpaceP(retentiondir,&quota,&capacity,&diskFreeSpace))
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to get disk free space for %s\n",retentiondir.c_str()); 
        rv = false;
        return rv;
    }

    lowspace_trigger = (capacity * lowspace_trigger_percentage)/100;

    if(lowspace_trigger < lowspace_trigger_lower_threshold)
    {
        lowspace_trigger = lowspace_trigger_lower_threshold;
    }
    else if(lowspace_trigger > lowspace_trigger_upper_threshold)
    {
        lowspace_trigger = lowspace_trigger_upper_threshold;
    }

    DebugPrintf(SV_LOG_DEBUG, "get_lowspace_value: lowspace_trigger_lower_threshold " ULLSPEC" lowspace_trigger_upper_threshold " ULLSPEC"\n", lowspace_trigger_lower_threshold, lowspace_trigger_upper_threshold);

    DebugPrintf(SV_LOG_DEBUG, "get_lowspace_value: Retention dir %s, lowspace_trigger value is " ULLSPEC"\n", retentiondir.c_str(), lowspace_trigger);

    return rv;
}

std::string CDPUtil::get_resync_required_path(const std::string & volume_name)
{
    LocalConfigurator localConfig;
    std::string resync_required_path = localConfig.getInstallPath();
    std::string cxvolume_name = volume_name;
    FormatVolumeNameForCxReporting(cxvolume_name);
    resync_required_path += ACE_DIRECTORY_SEPARATOR_STR_A;
    resync_required_path += "AppliedInfo";
    resync_required_path += ACE_DIRECTORY_SEPARATOR_STR_A;
    resync_required_path += localConfig.getHostId();
    resync_required_path += ACE_DIRECTORY_SEPARATOR_STR_A;
    resync_required_path += GetVolumeDirName(cxvolume_name);
    resync_required_path += ACE_DIRECTORY_SEPARATOR_STR_A;
    resync_required_path += "resyncRequired";
    return resync_required_path;
}


bool CDPUtil::store_and_sendcs_resync_required(const std::string & volume_name,  const std::string& msg,
    const ResyncReasonStamp &resyncReasonStamp, bool skip_cs_reporting, long errorcode)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s volume:%s\n",FUNCTION_NAME, volume_name.c_str());

    try    {

        DebugPrintf(SV_LOG_ERROR, "Resync is required for '%s' with reason: '%s' and error code: %ld.\n", volume_name.c_str(), msg.c_str(), errorcode);

        std::string resync_required_path = CDPUtil::get_resync_required_path(volume_name);
        std::string resync_history = resync_required_path + "history.log";

        persist_resync_required(resync_required_path);
        store_resync_required_history(volume_name, resync_history, msg);

        if(skip_cs_reporting){
            rv = true;
        } else {
            while(!rv && !CDPUtil::QuitRequested(60)) {
                rv = send_resyncrequired_to_cs(volume_name, msg, resyncReasonStamp, errorcode);
				DebugPrintf(SV_LOG_ALWAYS,
					"\nsetTargetResyncRequired call to CS for device %s succeeded.\n",
					volume_name.c_str());
            }
        }
    }
    catch ( ContextualException& ce ){
        DebugPrintf(SV_LOG_ERROR, "\n%s volume:%s encountered exception %s.\n", FUNCTION_NAME, volume_name.c_str(), ce.what());
    }
    catch( exception const& e ) {
        DebugPrintf(SV_LOG_ERROR, "\n%s volume:%s encountered exception %s.\n", FUNCTION_NAME, volume_name.c_str(), e.what());
    }
    catch ( ... ) {
        DebugPrintf(SV_LOG_ERROR, "\n%s volume:%s encountered unknown exception.\n", FUNCTION_NAME, volume_name.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s volume: %s\n",FUNCTION_NAME, volume_name.c_str());
    return rv;
}

void CDPUtil::persist_resync_required(const std::string & resync_required_path)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s path:%s\n", FUNCTION_NAME, resync_required_path.c_str());

    std::string folder_name = dirname_r(resync_required_path.c_str());
    folder_name += ACE_DIRECTORY_SEPARATOR_STR_A;
    if (SVMakeSureDirectoryPathExists(folder_name.c_str()).failed()) {
        throw INMAGE_EX("unable to create folder")(folder_name) ("error:")(ACE_OS::last_error());
    }

    // PR#10815: Long Path support
    ACE_HANDLE hFile =  ACE_OS::open(getLongPathName(resync_required_path.c_str()).c_str(), O_RDWR | O_CREAT, FILE_SHARE_READ);

    if(ACE_INVALID_HANDLE != hFile) {
        ACE_OS::close(hFile);
    } else {
        throw INMAGE_EX("unable to open")(resync_required_path) ("error:")(ACE_OS::last_error());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s path:\n", FUNCTION_NAME, resync_required_path.c_str());
}



void CDPUtil::store_resync_required_history(const std::string &volume_name, const std::string & resync_history, const std::string& msg)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s path:%s\n", FUNCTION_NAME, resync_history.c_str());

    std::string lockpath = resync_history + ".lck";
    std::string folder_name = dirname_r(resync_history.c_str());
    folder_name += ACE_DIRECTORY_SEPARATOR_STR_A;
    if (SVMakeSureDirectoryPathExists(folder_name.c_str()).failed()) {
        throw INMAGE_EX("unable to create folder")(folder_name) ("error:")(ACE_OS::last_error());
    }


    // PR#10815: Long Path support
    ACE_File_Lock lock(getLongPathName(lockpath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    if(lock.acquire() < 0)    {
        throw INMAGE_EX("unable to acquire lock")(lockpath) ("error:")(ACE_OS::last_error());
    }

    // PR#10815: Long Path support
    std::ofstream out(getLongPathName(resync_history.c_str()).c_str(), std::ios::app);
    if (!out) {
        throw INMAGE_EX("unable to open")(resync_history)("error :")(ACE_OS::last_error());
    }
    else {
        out.seekp(0, std::ios::end);
        
        out << boost::posix_time::to_simple_string(boost::posix_time::second_clock::universal_time())
            << " Resync Required for volume " << volume_name << ".\n"
            << "Reason for Resync: " << msg << "\n\n";
        out.close();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s path:\n", FUNCTION_NAME, resync_history.c_str());
}


bool CDPUtil::send_resyncrequired_to_cs(const std::string & volume_name, const std::string &msg,
    const std::map<std::string, std::string> &reason, long errorcode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s volume_name:%s\n", FUNCTION_NAME, volume_name.c_str());

    bool status = false;
    Configurator* TheConfigurator = NULL;
    if(!GetConfigurator(&TheConfigurator)) {
        throw INMAGE_EX("unable to obtain configurator");
    }

    return (setTargetResyncRequired(*TheConfigurator, volume_name, status, msg, reason, errorcode) && status);
}



