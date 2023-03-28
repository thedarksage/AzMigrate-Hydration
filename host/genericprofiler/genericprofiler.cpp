
///
///  \file  genericprofiler.cpp
///
///  \brief definitions for genericprofiler functions
///

#include "genericprofiler.h"
#include <typeinfo>

#ifdef SV_WINDOWS
LARGE_INTEGER GenericProfiler::m_perfFrequency;
boost::mutex GenericProfiler::m_queryPerfFreqLock;
#endif

#ifdef SV_WINDOWS
/// \brief one time initialization
bool GenericProfiler::init()
{
    if (!(m_perfFrequency.QuadPart))
    {
        boost::unique_lock<boost::mutex> lock(m_queryPerfFreqLock);
        if (!(m_perfFrequency.QuadPart))
        {
            QueryPerformanceFrequency(&m_perfFrequency);
        }
    }
    return true;
}
#endif

ProfilingSummaryMap ProfilingSummaryFactory::m_ProfilingSummaryMap;

boost::mutex ProfilingSummaryFactory::m_lock;

ProfilingSummary * ProfilingSummaryFactory::getProfilingSummary(ProfilingBuckets *profBktsType, bool bCumulative)
{
    try
    {
        // check type of profBktMsg in m_ProfilingSummaryMap
            // if not present insert
        // Add profBktsType in list
        ProfilingSummary * profilingSummary;
        ProfilingSummaryMap::iterator itr = m_ProfilingSummaryMap.find(profBktsType->m_operationName);
        if (itr == m_ProfilingSummaryMap.end())
        {
            boost::unique_lock<boost::mutex> lock(m_lock);
            itr = m_ProfilingSummaryMap.find(profBktsType->m_operationName); // double lock check as two threads may request for same logger same time
            if (itr == m_ProfilingSummaryMap.end())
            {
                profilingSummary = bCumulative ? new ProfilingSummary() : new IndividualProfilingSummary();
                m_ProfilingSummaryMap[profBktsType->m_operationName] = profilingSummary;
            }
            else
            {
                /// case: two threads requested for same ProfilingSummary same time: first thread created ProfilingSummary second thread found ProfilingSummary in ProfilingSummaryMap
                profilingSummary = itr->second;
            }
        }
        else
        {
            profilingSummary = itr->second;
        }
        profilingSummary->addProfilingBucketsType(&profBktsType);
        return profilingSummary;
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        throw; // rethrow exception
    }
}

GenericLoggerMap GenericLoggerFactory::m_genericLoggerMap;
boost::mutex GenericLoggerFactory::m_lock;

/// \brief creates and returns GenericLogger for profilingOperation
///
/// \param profilingOperation name of profiling operation E.G. <diskRead>
/// \param fileName log file where profiling is logged
///
/// \note throws an exception on error conditions like Permission denied for log file creation or no free space to create logger file
SIMPLE_LOGGER::GenericLogger < ProfilingMsg > * GenericLoggerFactory::getGenericLogger(const std::string profilingOperation, const std::string fileName)
{
    try
    {
        // fileName can be
        //   <file Name>
        //   <file Name>.<txt>
        //   <dir Name>\<file Name>
        //   <dir Name>\<file Name>.<csv>
        // If extention is not ".csv" or is missing replace or add with ".csv"
        boost::filesystem::path filePath = fileName.length() > 0 ? fileName : profilingOperation;;
        if (filePath.extension().compare(std::string(".csv")) != 0)
        {
            filePath.replace_extension(".csv");
        }

        // check fileName in GenericLoggerMap
        // if not present insert
        SIMPLE_LOGGER::GenericLogger < ProfilingMsg > * logger;
        GenericLoggerMap::iterator itr = m_genericLoggerMap.find(filePath.string());
        if (itr == m_genericLoggerMap.end())
        {
            boost::unique_lock<boost::mutex> lock(m_lock);
            itr = m_genericLoggerMap.find(filePath.string()); // double lock check as two threads may request for same logger same time
            if (itr == m_genericLoggerMap.end())
            {
                SIMPLE_LOGGER::LogWriter* profileLogWriter = SIMPLE_LOGGER::LogWriterCreator::getLogWriter();

                logger = new SIMPLE_LOGGER::GenericLogger<ProfilingMsg>(filePath,
                    profileLogWriter,
                    1024 * 1024 * 15,
                    3,
                    0,
                    true,
                    SIMPLE_LOGGER::LEVEL_ALWAYS);
                m_genericLoggerMap[filePath.string()] = logger;
            }
            else
            {
                /// case: two threads requested for same logger same time, first created GenericLogger second thrad found logger in GenericLoggerMap
                logger = itr->second;
            }
        }
        else
        {
            logger = itr->second;
        }
        return logger;
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        throw; // rethrow exception
    }
}

/// \brief turns off all GenericLogger by graceful exit of writer threads and releasing associated GenericLoggers
///
/// \note should be called once at end of user application
///
/// \note do not let exception be thrown, just catch all and ignore
void GenericLoggerFactory::exitGenericLogger()
{
    try
    {
        // LogWriter has reference to registered signals of GenericLogger
        // Also GenericLogger has reference to LogWriter
        // So Always first stop LogWriter thread. This will no harm for GenericLogger as LogWriter object is still there
        // Then release all GenericLogger instances in GenericLoggerMap
        // At last release LogWriter
        SIMPLE_LOGGER::LogWriterCreator::stopLogWriterThread();

        GenericLoggerMap::iterator loggerItr = m_genericLoggerMap.begin();
        for (loggerItr; loggerItr != m_genericLoggerMap.end(); ++loggerItr)
        {
            loggerItr->second->writeToFileSafe(); // purge if anything pending after writer thread exited
            loggerItr->second->printStats();
            delete loggerItr->second; // release GenericLogger
        }
        m_genericLoggerMap.empty();

        SIMPLE_LOGGER::LogWriterCreator::releaseLogWriter(); // release LogWriter
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        throw; // rethrow exception
    }
}

/// \brief creates and returns Profiler on demand
///
/// \param pt profiler type
/// \param operationName name of profiling operation E.G. <diskRead>
/// \param profBktsType object of type ProfilingBuckets
/// \param verbose flag indicating detailed profiling
/// \param fileName log file where profiling is logged
/// \param delayedWrite indicates profiling is done for each message or for a batch of 500 messages
///
/// E.G. ProfilerFactory::getProfiler(PROFILER_TYPE::GENERIC_PROFILER, "checksumList", "<install path>\\checksumList.csv", true);
///
/// \note throws an exception on error conditions like Permission denied for log file creation or no free space to create logger file
Profiler ProfilerFactory::getProfiler(PROFILER_TYPE pt, const std::string operationName, ProfilingBuckets *profBktsType, bool verbose, const std::string fileName, const bool delayedWrite, bool bCumulative)
{
    try
    {
        if (GENERIC_PROFILER == pt)
        {
            SIMPLE_LOGGER::GenericLogger<ProfilingMsg>* logger = NULL;
            if (verbose)
            {
                logger = GenericLoggerFactory::getGenericLogger(operationName, fileName);
            }
            
            ProfilingSummary * ps = NULL;
            if (profBktsType)
            {
                ps = ProfilingSummaryFactory::getProfilingSummary(profBktsType, bCumulative);
            }
            return Profiler(new GenericProfiler(operationName, profBktsType, ps, logger, delayedWrite));
        }
        return Profiler(new PassiveProfiler());
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        throw; // rethrow exception
    }
}

/// \brief turns off all profilers by graceful exit of writer threads and releasing associated GenericLoggers
///
/// \note should be called once at end of user application
///
/// \note do not let exception be thrown, just catch all and ignore
void ProfilerFactory::exitProfiler()
{
    try
    {
        GenericLoggerFactory::exitGenericLogger();
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        /// catch all unknown exceptions and ignore
    }
}