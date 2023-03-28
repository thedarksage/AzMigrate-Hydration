
///
/// \file genericprofilerexample.cpp
///
/// \brief shows how to use genericprofiler
///

#include <iostream>

#include "genericprofiler.h"

#include <boost/thread/thread.hpp>;

void test7_thread(Profiler &p)
{
    for (int i = 1; i <= 2; ++i)
    {
        p->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        p->stop(1); // dummy value 1
    }
}

void test8_thread(std::string action)
{
    Profiler p7_1 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, action, NULL, true, "genericprofiler-example_8.log", true);
}

void test9_thread(Profiler &p)
{
    for (int i = 0; i < 2000; ++i)
    {
        p->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        p->stop(i);
    }
}

void test11_thread(Profiler &p)
{
    for (int i = 0; i < 20000; ++i)
    {
        p->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        p->stop(i);
    }
}

void test14_thread(Profiler &p)
{
    for (int i = 0; i < 2000; ++i)
    {
        p->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        p->stop(i);
    }
}

int main(int argc, char ** argv)
{
    /// init ACE
    ACE::init();

    /// init CDPUtil quit event
    CDPUtil::InitQuitEvent(false);

    try
    {
        /// ****************************************** TEST #1 ******************************************
        /// \test verify profiling for GENERIC_PROFILER is done with format as expected for 1 sec sleep action
        /// initalize Profiler
        Profiler p1 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_1.csv");

        /// start Profiler
        p1->start();

        /// pause for 1 sec
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

        /// stop Profiler
        p1->stop(1); // dummy value 1


        /// ****************************************** TEST #2 ******************************************
        /// \test verify profiling skipped for PASSIVE_PROFILER
        /// initalize Profiler
        Profiler p2 = ProfilerFactory::getProfiler(PROFILER_TYPE::PASSIVE_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_2.csv");
        p2->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        p2->stop(1); // dummy value 1


        /// ****************************************** TEST #3 ******************************************
        /// \test verify if extention not suplied log file created with extention ".csv"
        Profiler p3 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_3");

        /// ****************************************** TEST #4 ******************************************
        /// \test verify if any extention supplied other than ".csv" that extention is replaced to ".csv"
        Profiler p4 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_4.log");

        /// ****************************************** TEST #5 ******************************************
        /// \test verify delayed write works for profiler created with delayedWrite=true - verify if buffer has < 500 messages no message is missed from file write
        /// \brief Each profiler maintains buffer of 500 messages and once hit this buffer is booked for logging
        /// \note changed profiler library to set buffer threshold to 5 for testing
        /// \note use of commit() for delayed write is mandetory

        Profiler p5 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_5.log", true);
        p5->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        p5->stop(1); // dummy value 1
        /// commit for delayed write - forces message booking to logger
        p5->commit();

        /// ****************************************** TEST #6 ******************************************
        /// \test verify instant write works for profiler created with delayedWrite=false even when commit() is not done
        /// \brief Few operations are very infrequent so we can bypass message buffering and book every single message to logger

        Profiler p6 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_6.log", false);
        p6->start();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        p6->stop(1); // dummy value 1

        /// ****************************************** TEST #7 ******************************************
        /// \test verify profilers in different threads with same log file does profiling in same file and no message is missed in log

        Profiler p7_1 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_7.log", true);
        Profiler p7_2 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_7.log", true);

        boost::thread t7_1 = boost::thread(&test7_thread, p7_1);
        boost::thread t7_2 = boost::thread(&test7_thread, p7_2);
        t7_1.join();
        t7_2.join();

        p7_1->commit();
        p7_2->commit();

        /// ****************************************** TEST #8 ******************************************
        /// \note TEST #8 is dev unit testing purpose also an example
        /// \test verify if multiple threads for profiler for same log file name all profilers created associated with same logger and log messages in same file
        /// \note logger is created only once in this case and for all further requests ProfilerFactory will associate existing logger to new profilers
        /// \note code for ProfilerFactory::getProfiler modified for debug o/p for testing purpose, see sample o/p for TEST #8

        boost::thread t8_1 = boost::thread(&test8_thread, "action1");
        boost::thread t8_2 = boost::thread(&test8_thread, "action2");
        boost::thread t8_3 = boost::thread(&test8_thread, "action3");
        boost::thread t8_4 = boost::thread(&test8_thread, "action4");
        boost::thread t8_5 = boost::thread(&test8_thread, "action5");
        t8_1.join();
        t8_2.join();
        t8_3.join();
        t8_4.join();
        t8_5.join();

        /// ****************************************** TEST #9 ******************************************
        /// \test verify 9 profilers in different threads log for every 1 milli seconds, 2000 message per profiler and logger drains each message without any drop

        Profiler p9[9];
        boost::thread t9[9];
        for (int i = 0; i < 9; ++i)
        {
            p9[i] = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_9.log", true);
            t9[i] = boost::thread(&test9_thread, p9[i]);
        }
        for (int i = 0; i < 9; ++i) { t9[i].join(); }
        for (int i = 0; i < 9; ++i) { p9[i]->commit(); }


        /// ****************************************** TEST #10 ******************************************
        /// \test verify exception is thrown when creating log file at path where write permission is not present
        try
        {
            Profiler p10 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "g:\\genericprofiler-example_10.log", true);
            p10->start();
            boost::this_thread::sleep(boost::posix_time::milliseconds(1));
            p10->stop();
            p10->commit();
        }
        catch (std::exception const& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        }

        /// ****************************************** TEST #11 ******************************************
        /// \test verify if log file volume becomes full, test program never crashes
        Profiler p11 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "g:\\genericprofiler-example_11.log", true);
        boost::thread t11 = boost::thread(&test11_thread, p11);
        t11.join();
        p11->commit();

        /// ****************************************** TEST #12 ******************************************
        /// \test verify if no space on volume where log file path is set in profiler and no program crash happen
        try
        {
            Profiler p12 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "g:\\genericprofiler-example_12.log", true);
        }
        catch (std::exception const& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        }

        /// ****************************************** TEST #13 ******************************************
        /// \test if stop() called before start() of profiler verify program never crashes
        Profiler p13 = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "g:\\genericprofiler-example_11.log", true);
        p13->stop();

        /// ****************************************** TEST #14 ******************************************
        /// \test verify if data pump is high speed than writer drain speed no program crash happen.
        /// Example: 20 profilers in different threads log for every 1 milli seconds, 2000 message per profiler and logger drains each message

        Profiler p14[20];
        boost::thread t14[20];
        for (int i = 0; i < 20; ++i)
        {
            p14[i] = ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction", NULL, true, "genericprofiler-example_14.log", true);
            t14[i] = boost::thread(&test14_thread, p14[i]);
        }
        for (int i = 0; i < 20; ++i) { t14[i].join(); }
        for (int i = 0; i < 20; ++i) { p14[i]->commit(); }

        /// ****************************************** TEST #15 ******************************************
        /// \test verify if no file name provided a file created with name as profiling action supplied with .csv as extention
        ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "sleepAction");

    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
    }
    std::cout << "Goodby!" << std::endl;

    /// graceful exit
    ProfilerFactory::exitProfiler();

    std::cout << "Goodby!" << std::endl;

    return 1;
}


    /*

    TEST #1 expected o/p:
    - File created genericprofiler-example_1.csv with contents
    <sleepAction>,<provides value for action size>,<start timestamp>,<stop timestamp>,<1 sec equivalent in microseconds>

    - Contents in ggenericprofiler-example_1.csv:
    sleepAction,1,2016-03-07T17:23:59.156364,2016-03-07T17:24:00.156964,999716
    File=./genericprofiler-example_1.csv,Add Count=1,Write Count=1

    TEST #2 expected o/p:
    - File genericprofiler-example_2.csv not created

    TEST #3 expected o/p:
    - File created genericprofiler-example_3.csv

    TEST #4 expected o/p:
    - File created genericprofiler-example_4.csv

    TEST #5 expected o/p:
    - File created genericprofiler-example_5.csv with appropriate contents

    - Contents in genericprofiler-example_5.csv:
    sleepAction,1,2016-03-07T18:24:24.591898,2016-03-07T18:24:25.592302,1000026
    File=./genericprofiler-example_5.csv,Add Count=1,Write Count=1

    TEST #6 expected o/p:
    - File created genericprofiler-example_6.csv with appropriate contents

    - Contents in genericprofiler-example_6.csv:
    sleepAction,1,2016-03-07T18:24:25.622232,2016-03-07T18:24:26.624095,1000891
    File=./genericprofiler-example_6.csv,Add Count=1,Write Count=1

    TEST #7 expected o/p:
    - File created genericprofiler-example_7.csv with 4 messages

    - Contents in genericprofiler-example_7.csv:
    sleepAction,1,2016-03-07T18:46:53.468565,2016-03-07T18:46:54.468636,999534
    sleepAction,1,2016-03-07T18:46:54.468636,2016-03-07T18:46:55.469534,1000716
    sleepAction,1,2016-03-07T18:46:53.468565,2016-03-07T18:46:54.468636,999528
    sleepAction,1,2016-03-07T18:46:54.468636,2016-03-07T18:46:55.469534,1000710
    File=./genericprofiler-example_7.csv,Add Count=4,Write Count=4


    TEST #8 expected o/p:
    - Only one file created genericprofiler-example_8.csv

    - Modified code for this test:
    if (itr == m_genericLoggerMap.end())
    {
    DebugPrintf(SV_LOG_ERROR, "aquiring lock...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    boost::unique_lock<boost::mutex> lock(m_lock);
    itr = m_genericLoggerMap.find(filePath.string()); // double lock check as two threads may request for same logger same time
    if (itr == m_genericLoggerMap.end())
    {
    DebugPrintf(SV_LOG_ERROR, "got lock...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    SIMPLE_LOGGER::LogWriter* profileLogWriter = SIMPLE_LOGGER::LogWriterCreator::getLogWriter();

    DebugPrintf(SV_LOG_ERROR, "Creating GenericLogger...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    logger = new SIMPLE_LOGGER::GenericLogger<ProfilingMsg>(filePath,
    profileLogWriter,
    1024*1024*15,
    3,
    0,
    true,
    SIMPLE_LOGGER::LEVEL_ALWAYS,
    SIMPLE_LOGGER::DEL_OLDER_EXCEEDING_RCOUNT);
    m_genericLoggerMap[filePath.string()] = logger;
    itr = m_genericLoggerMap.find(filePath.string()); /// TBD: temp code for testing
    if (itr != m_genericLoggerMap.end()) /// TBD: temp code for testing
    {
    DebugPrintf(SV_LOG_ERROR, "Created GenericLogger...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    }
    }
    else
    {
    /// case: two threads requested for same logger same time, first created GenericLogger second thrad found logger in GenericLoggerMap
    logger = itr->second;
    DebugPrintf(SV_LOG_ERROR, "got GenericLogger...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    }
    }
    else
    {
    logger = itr->second;
    DebugPrintf(SV_LOG_ERROR, "got GenericLogger...%s: %s\n", profilingAction.c_str(), filePath.string().c_str()); /// TBD: temp code for testing
    }

    - Console o/p for this test:
    aquiring lock...action1: genericprofiler-example_8.csv
    aquiring lock...action4: genericprofiler-example_8.csv
    aquiring lock...action3: genericprofiler-example_8.csv
    aquiring lock...action2: genericprofiler-example_8.csv
    aquiring lock...action5: genericprofiler-example_8.csv
    got lock...action1: genericprofiler-example_8.csv
    Creating GenericLogger...action1: genericprofiler-example_8.csv
    Created GenericLogger...action1: genericprofiler-example_8.csv
    got GenericLogger...action4: genericprofiler-example_8.csv
    got GenericLogger...action3: genericprofiler-example_8.csv
    got GenericLogger...action2: genericprofiler-example_8.csv
    got GenericLogger...action5: genericprofiler-example_8.csv
    Goodby!
    Press any key to continue . . .

    - note that multiple threads requested to create genericprofiler-example_8 one thread belonging to action1 created file and all other threads got existing logger i.e. first created


    TEST #9 expected o/p:
    - File created genericprofiler-example_9.csv with correct count 18000.
    - Statistics log from file:
    File=./genericprofiler-example_9.csv,Add Count=18000,Write Count=18000

    TEST #10 expected o/p:
    - No program crash should happen and an exception message should be seen on console
    - Console o/p:
    ProfilerFactory::getProfiler, Exception caught : [at e : \workarea\trunk\host_profiling\simplelogger\simplelogger.h:SIMPLE_LOGGER::SimpleLogger::openLogIfNeeded : 448]   error opening log file : g : / genericprofiler - example_10.csv.Error : Permission denied
    main, Exception caught : [at e : \workarea\trunk\host_profiling\simplelogger\simplelogger.h:SIMPLE_LOGGER::SimpleLogger::openLogIfNeeded : 448]   error opening log file : g : / genericprofiler - example_10.csv.Error : Permission denied
    Goodby!

    TEST #11 expected o/p:
    - No program crash should happen and an exception message should be seen on console
    - Console o/p:
    - File created genericprofiler-example_9.csv with partially filled messages and count statistics as,
    File=g:/genericprofiler-example_10.csv,Add Count=20000,Write Count=14000
    SIMPLE_LOGGER::GenericLogger<class ProfilingMsg>::writeToFile, Exception caught: [at e:\workarea\trunk\host_profiling\simplelogger\simplelogger.h:SIMPLE_LOGGER::SimpleLogger::doLog:475]   error writing data to file: g:/genericprofiler-example_10.csv. Error: No space left on device

    TEST #12 expected o/p:
    - No program crash should happen and an exception message should be seen on console
    - Console o/p:
    ProfilerFactory::getProfiler, Exception caught: [at e:\workarea\trunk\host_profiling\simplelogger\simplelogger.h:SIMPLE_LOGGER::SimpleLogger::openLogIfNeeded:448]   error opening log file: g:/genericprofiler-example_11.csv. Error: Permission denied
    main, Exception caught: [at e:\workarea\trunk\host_profiling\simplelogger\simplelogger.h:SIMPLE_LOGGER::SimpleLogger::openLogIfNeeded:448]   error opening log file: g:/genericprofiler-example_11.csv. Error: Permission denied

    TEST #13 expected o/p:
    - No program crash should happen and an exception message should be seen on console
    - Console o/p:
    genericprofiler::stop, Exception caught: [at e:\workarea\trunk\host_profiling\genericprofiler\genericprofiler.h:genericprofiler::stop:156]   PerformanceFrequency not initialized. Use start before stop

    TEST #14 expected o/p:
    - No program crash should happen
    - File genericprofiler-example_14.csv got created with statistics line as,
    File=./genericprofiler-example_14.csv,Add Count=36610,Write Count=36610
    This means 3390 messages are dropped out of 40000 pumped.

    - Console o/p: Seen number of messages of "genericprofiler::start, not an error, dropping profiling message as all slots are occupied..." as,
    genericprofiler::start, not an error, dropping profiling message as all slots are occupied...
    genericprofiler::start, not an error, dropping profiling message as all slots are occupied...
    .
    .
    .
    genericprofiler::start, not an error, dropping profiling message as all slots are occupied...
    Goodby!

    TEST #15 o/p:
    - File sleepAction.csv created in current folder.
    */


