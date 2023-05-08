///
///  \file  genericprofiler.h
///
///  \brief a profiler implementation for profiling any operation to analyse operation latency
///
/// \example genericprofiler/genericprofilerexample.cpp

#ifndef GENERICPROFILER_H
#define GENERICPROFILER_H
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <math.h>
#include <iomanip>

#ifdef SV_WINDOWS
#include <Windows.h>
#else
#include <unistd.h>    /* POSIX flags */
#include <time.h>
#endif

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>

#include "genericlogger.h"
#include "errorexception.h"
#include "logger.h"

#include "json_writer.h"

#define DELAY_WRITE_COUNT 500

#define BUCKETSSIZE 13

#define PERCENTAGE(total, val) (roundf((double)val * 100 / total))

typedef std::map<std::string, std::string> BucketsMap;

class PrBuckets
{
public:
    std::string Op;
    std::string Dev;
    SV_UINT Sz_B;
    SV_UINT NSz_B;
    std::string TSz_B;
    std::string TTime_us;
    SV_UINT TC;
    std::string FTS;
    std::string LTS;
    BucketsMap Bkts_ms;

public:
    PrBuckets() :Sz_B(0), NSz_B(0), TC(0){}
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "PrBuckets", false);
        JSON_E(adapter, Op);
        JSON_E(adapter, Dev);
        JSON_E(adapter, Sz_B);
        JSON_E(adapter, NSz_B);
        JSON_E(adapter, TSz_B);
        JSON_E(adapter, TTime_us);
        JSON_E(adapter, TC);
        JSON_E(adapter, FTS);
        JSON_E(adapter, LTS);
        JSON_T(adapter, Bkts_ms);
    }
};

// ProfilingBucketsMsg interface
struct ProfilingBuckets
{
    explicit ProfilingBuckets(const std::string &operationName, const bool useTS = false, std::string deviceName = "", const int bucketsSize = BUCKETSSIZE) :
                    m_operationName(operationName),
                    m_useTS(useTS),
                    m_deviceName(deviceName),
                    m_bucketsSize(bucketsSize),
                    m_totalSize(0),
                    m_totalTime(0),
                    m_profilingSize(0),
                    m_init(false)
    {
        m_buckets.resize(m_bucketsSize, 0);
    }

    ProfilingBuckets(const ProfilingBuckets &other) :
        m_operationName(other.m_operationName),
        m_buckets(other.m_buckets),
        m_bucketsSize(other.m_bucketsSize),
        m_totalTime(other.m_totalTime),
        m_totalSize(other.m_totalSize),
        m_init(other.m_init),
        m_useTS(other.m_useTS),
        m_profilingSize(other.m_profilingSize),
        m_firstTS(other.m_firstTS),
        m_lastUpdateTS(other.m_lastUpdateTS),
        m_deviceName(other.m_deviceName) { }

    virtual ProfilingBuckets * clone() const
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        return new ProfilingBuckets(static_cast<ProfilingBuckets const&>(*this));
    }

    virtual bool updateBucket(const SV_ULONGLONG &timeDiffMicroSec, const SV_UINT &size = 0)
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        int ret = 1;
        
        if (!m_init)
        {
            m_profilingSize = size;
            if (m_useTS)
            {
                m_firstTS = boost::posix_time::microsec_clock::universal_time();
            }
            m_init = true;
        }

        updateTotalTimeAndSize(timeDiffMicroSec, size);

        const SV_ULONGLONG timeDiffMiliSec = timeDiffMicroSec / 1000;

        int index = 0;
        if (timeDiffMiliSec > pow((double)2, m_bucketsSize))
        {
            index = m_bucketsSize - 1;
        }
        else
        {
            int time = timeDiffMiliSec;
            while (time) {
                ++index;
                time >>= 1;
                if (!time)
                    break;
            }
        }
        m_buckets[index >= m_bucketsSize ? m_bucketsSize - 1 : index]++;

        if (m_useTS)
        {
            m_lastUpdateTS = boost::posix_time::microsec_clock::universal_time();
        }

        return ret;
    }

    virtual void operator+=(const ProfilingBuckets &other)
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        boost::unique_lock<boost::recursive_mutex> otherGuard(other.m_profilingBucketsLock);
        if (!other.m_init)
        {
            return;
        }

        for (int i = 0; i < m_bucketsSize; i++)
        {
            m_buckets[i] += other.m_buckets[i];
        }

        m_totalSize += other.m_totalSize;
        m_totalTime += other.m_totalTime;

        if (m_useTS)
        {
            if (m_firstTS > other.m_firstTS)
            {
                m_firstTS = other.m_firstTS;
            }

            if (!other.m_lastUpdateTS.is_not_a_date_time())
            {
                if (m_lastUpdateTS.is_not_a_date_time() || (!m_lastUpdateTS.is_not_a_date_time() && (m_lastUpdateTS < other.m_lastUpdateTS)))
                {
                    m_lastUpdateTS = other.m_lastUpdateTS;
                }
            }
        }
    }

    virtual PrBuckets getSummary() const
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        PrBuckets jpb;
        SV_UINT total = 0;
        int indx = 0;
        for (indx; indx < m_bucketsSize; ++indx)
        {
            total += m_buckets[indx];
        }
        if (total)
        {
            jpb.Op = m_operationName;
            jpb.Dev = m_deviceName;
            jpb.Sz_B = m_profilingSize;
            jpb.TSz_B = boost::lexical_cast<std::string>(m_totalSize);
            jpb.TTime_us = boost::lexical_cast<std::string>(m_totalTime);
            jpb.TC = total;
            if (m_useTS)
            {
                jpb.FTS = boost::posix_time::to_simple_string(m_firstTS);
                jpb.LTS = boost::posix_time::to_simple_string(m_lastUpdateTS);
            }
            jpb.Bkts_ms = getBuckets();
        }
        return jpb;
    }

    virtual ~ProfilingBuckets() {}

    boost::posix_time::ptime GetLastUpdateTS() const
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        return m_lastUpdateTS;
    }

    SV_LONGLONG GetTotalSize() const { return m_totalSize; }

protected:
    virtual BucketsMap getBuckets() const
    {
        BucketsMap buckets;
        int indx = 0;
        bool isnext = false;
        for (indx; indx < m_bucketsSize-1; ++indx)
        {
            if (m_buckets[indx])
            {
                int firstRange = !indx ? 0 : pow((double)2, indx - 1);
                buckets[boost::lexical_cast<std::string>(firstRange)+"_" + boost::lexical_cast<std::string>(pow((double)2, indx))] = boost::lexical_cast<std::string>(m_buckets[indx]);
            }
        }
        if (m_buckets[m_bucketsSize - 1])
        {
            buckets[">" + boost::lexical_cast<std::string>(pow((double)2, m_bucketsSize - 2))] = boost::lexical_cast<std::string>(m_buckets[m_bucketsSize - 1]);
        }
        return buckets;
    }

    virtual void updateTotalTimeAndSize(const SV_ULONGLONG &timeDiffMicroSec, const SV_UINT &size)
    {
        m_totalTime += timeDiffMicroSec;
        m_totalSize += size;
    }

private:
    virtual void operator=(const ProfilingBuckets &other) { }

public:
    const std::string m_operationName; ///< name of profiling operation

protected:
    std::vector<SV_ULONGLONG> m_buckets;
    const int m_bucketsSize;
    SV_LONGLONG m_totalTime;
    SV_LONGLONG m_totalSize;

    bool m_init;
    const bool m_useTS;
    SV_UINT m_profilingSize;
    boost::posix_time::ptime m_firstTS;
    boost::posix_time::ptime m_lastUpdateTS;

    std::string m_deviceName;
    mutable boost::recursive_mutex m_profilingBucketsLock;
};

struct NormProfilingBuckets : public ProfilingBuckets
{
    explicit NormProfilingBuckets(const SV_UINT normalizationSize, const std::string operationName, const bool useTS = false, std::string deviceName = "") :
                                    ProfilingBuckets(operationName, useTS, deviceName),
                                    m_normalizationSize(normalizationSize),
                                    m_time(0),
                                    m_size(0) {}

    NormProfilingBuckets(const NormProfilingBuckets &other) :
        ProfilingBuckets(other),
        m_normalizationSize(other.m_normalizationSize),
        m_time(other.m_time),
        m_size(other.m_size) { }

    virtual PrBuckets getSummary() const
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        PrBuckets jpb;
        SV_ULONG total = 0;
        int indx = 0;
        for (indx; indx < m_bucketsSize; ++indx)
        {
            total += m_buckets[indx];
        }
        if (total)
        {
            jpb.Op = m_operationName;
            jpb.Dev = m_deviceName;
            jpb.NSz_B = m_normalizationSize;
            jpb.TSz_B = boost::lexical_cast<std::string>(m_totalSize);
            jpb.TTime_us = boost::lexical_cast<std::string>(m_totalTime);
            jpb.TC = total;
            if (m_useTS)
            {
                jpb.FTS = boost::posix_time::to_iso_extended_string(m_firstTS);
                jpb.LTS = boost::posix_time::to_iso_extended_string(m_lastUpdateTS);
            }
            jpb.Bkts_ms = getBuckets();
        }
        return jpb;
    }

    virtual ProfilingBuckets * clone() const
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_profilingBucketsLock);
        return new NormProfilingBuckets(static_cast<NormProfilingBuckets const&>(*this));
    }

protected:
    virtual bool updateBucket(const SV_ULONGLONG &timeDiffMicroSec, const SV_ULONG &size = 0)
    {
        m_time = timeDiffMicroSec; // save to update TotalTime
        m_size = size; // save to update TotalSize
        return ProfilingBuckets::updateBucket(timeDiffMicroSec*m_normalizationSize / size, m_normalizationSize);
    }

    virtual void updateTotalTimeAndSize(const SV_ULONGLONG &dummytime, const SV_ULONG &dummysize)
    {
        ProfilingBuckets::updateTotalTimeAndSize(m_time, m_size); // ignore dummy values and update actual zise
    }

    virtual ~NormProfilingBuckets() {}

private:
    virtual void operator=(const NormProfilingBuckets &other) { }

private:
    const SV_UINT m_normalizationSize;
    SV_LONGLONG m_time;
    SV_LONGLONG m_size;
};

// collects cumulative summary of all ProfilingBuckets
class ProfilingSummary
{
public:
    ProfilingSummary() {}

    void addProfilingBucketsType(ProfilingBuckets **profBktType)
    {
        m_profilingBucketsList.push_back(*profBktType);
    }

    virtual void getSummaryAsData(std::vector<boost::shared_ptr<ProfilingBuckets> > &vProfilingBuckets) const
    {
        if (!m_profilingBucketsList.empty())
        {
            std::list < ProfilingBuckets * >::const_iterator it = m_profilingBucketsList.begin();
            boost::shared_ptr<ProfilingBuckets> ps((*it)->clone());

            for (++it; it != m_profilingBucketsList.end(); ++it)
            {
                *ps += *(*it);
            }
            vProfilingBuckets.push_back(ps);
        }
    }

    virtual void getSummaryAsJson(std::vector<PrBuckets> & vJPrBkts) const
    {
        std::vector<boost::shared_ptr<ProfilingBuckets> > vProfilingBuckets;
        getSummaryAsData(vProfilingBuckets);
        std::vector<boost::shared_ptr<ProfilingBuckets> >::const_iterator it(vProfilingBuckets.begin());
        for (it; it != vProfilingBuckets.end(); it++)
        {
            PrBuckets jb = (*it)->getSummary();
            if (!jb.Bkts_ms.empty())
            {
                vJPrBkts.push_back(jb);
            }
        }
    }

protected:
    const std::list<ProfilingBuckets*>& GetProfilingBucketsList()  const
    {
        return 	 m_profilingBucketsList;
    }

private:
    ProfilingSummary(const ProfilingSummary&) {}
    void operator=(const ProfilingSummary&) {}

private:
    std::list < ProfilingBuckets * > m_profilingBucketsList;
};

// collects  summary for each ProfilingBuckets
class IndividualProfilingSummary : public ProfilingSummary
{
public:
    IndividualProfilingSummary() {}

    virtual void getSummaryAsJson(std::vector<PrBuckets> &vJPrBkts) const
    {
        std::list < ProfilingBuckets * >::const_iterator it = GetProfilingBucketsList().begin();
        for (it; it != GetProfilingBucketsList().end(); ++it)
        {
            PrBuckets jb = (*it)->getSummary();
            if (!jb.Bkts_ms.empty())
            {
                vJPrBkts.push_back(jb);
            }
        }
    }

    virtual void getSummaryAsData(std::vector<boost::shared_ptr<ProfilingBuckets> > &vProfilingBuckets) const
    {
        std::list < ProfilingBuckets * >::const_iterator it = GetProfilingBucketsList().begin();
        for (it; it != GetProfilingBucketsList().end(); ++it)
        {
            vProfilingBuckets.push_back(boost::shared_ptr<ProfilingBuckets>((*it)->clone()));
        }
    }

private:
    IndividualProfilingSummary(const IndividualProfilingSummary&) {}
    void operator=(const IndividualProfilingSummary&) {}
};

typedef std::map < std::string, ProfilingSummary * > ProfilingSummaryMap;

class ProfilingSummaryFactory
{
public:
    static ProfilingSummary * getProfilingSummary(ProfilingBuckets *profBktsType, bool bCumulative = true);

    static void summaryAsJson(std::vector<PrBuckets> &vJPrBktsAll)
    {
        ProfilingSummaryMap::iterator itr = m_ProfilingSummaryMap.begin();
        for (itr; itr != m_ProfilingSummaryMap.end(); ++itr)
        {
            std::vector<PrBuckets> vJPrBkts;
            itr->second->getSummaryAsJson(vJPrBkts);
            for (std::vector<PrBuckets>::const_iterator it = vJPrBkts.begin(); it != vJPrBkts.end(); ++it)
            {
                vJPrBktsAll.push_back(*it);
            }
        }
    }

    static void summaryAsData(const std::string & operationName, std::vector<boost::shared_ptr<ProfilingBuckets> > &vProfilingBuckets)
    {
        ProfilingSummaryMap::iterator it = m_ProfilingSummaryMap.find(operationName);
        if (it != m_ProfilingSummaryMap.end())
        {
            it->second->getSummaryAsData(vProfilingBuckets);
        }
    }

private:
    static ProfilingSummaryMap m_ProfilingSummaryMap; ///< a map of GenericLogger associated with file name

    static boost::mutex m_lock; ///< lock to create GenericLogger 
};

class ProfilingMsg;

typedef std::map < std::string, SIMPLE_LOGGER::GenericLogger<ProfilingMsg> * > GenericLoggerMap;

class _Profiler;
typedef boost::shared_ptr<_Profiler> Profiler;

enum PROFILER_TYPE {
    PASSIVE_PROFILER,
    GENERIC_PROFILER
};

struct TimeStamp
{
    bool set()
    {
        m_ts = boost::posix_time::microsec_clock::universal_time();
        return true;
    }

    const std::string str() const
    {
        return boost::posix_time::to_iso_extended_string(m_ts);
    }
    
    boost::posix_time::ptime m_ts;
};

/// \brief a profiling message class to store profiling data
///
/// \note implementation of "std::string str()" is a must to use in conjunction with SIMPLE_LOGGER::GenericLogger
class ProfilingMsg
{
public:
    /// \brief default constructor
    ProfilingMsg() : m_size(0), m_timeDiff(0), m_offset(0){}

    /// \brief returns csv formatted string of profiling data
    ///
    /// outputs the data in the following format\n
    ///
    /// <profiling operation>,<size in bytes>,<startTimeStamp>,<stopTimeStamp>,<time diff in micro sec>
    std::string str() const
    {
        std::stringstream ssProfilingData;
        ssProfilingData << m_operationName << "," << m_size << "," << m_startTimeStamp.str() << "," << m_stopTimeStamp.str() << "," << m_timeDiff << "," << m_offset;
        return ssProfilingData.str();
    }

    std::string m_operationName; ///< name of profiling operation

    SV_ULONGLONG m_size; ///< size in bytes of processed profiling operation

    SV_ULONG m_timeDiff; ///< time difference in micro seconds of processed profiling operation

    TimeStamp m_startTimeStamp; ///< clock time before profiling operation

    TimeStamp m_stopTimeStamp; ///< clock time after profiling operation

    SV_ULONGLONG m_offset;
};

/// \brief an interface class that defines basic profiling functions
class _Profiler
{
public:
    /// \brief starts profiling
    virtual void start() = 0;

    /// \brief stops profiling
    virtual SV_ULONG stop(SV_ULONGLONG size = 0, SV_ULONGLONG offset = 0) = 0;

    /// \brief books profiling data for logging
    ///
    /// \note should be called at last when we are done profiling
    virtual void commit() = 0;

    virtual void profilingSunmmaryAsJson(std::vector<PrBuckets> &vpb) const = 0;

    virtual void profilingSunmmaryAsData(std::vector<boost::shared_ptr<ProfilingBuckets> > &vpb) const = 0;

    /// \brief a vrtual destructor
    virtual ~_Profiler() {}
};

/// \brief implements _Profiler for profiling a code block latency using message type ProfilingMsg
// TBD: change name GenericProfiler
class GenericProfiler : public _Profiler
{
public:
    /// \brief default constuctor
    explicit GenericProfiler(std::string operationName, ProfilingBuckets * profilingBuckets = NULL, ProfilingSummary * profilingSummary = NULL, SIMPLE_LOGGER::GenericLogger<ProfilingMsg> * genericLogger = NULL, bool delayedWrite = true) :
        m_operationName(operationName),
        m_profilingBuckets(profilingBuckets),
        m_profilingSummary(profilingSummary),
        m_genericLogger(genericLogger),
        m_delayedWrite(delayedWrite),
        m_simpleMsgBlk(NULL){}

    /// \brief starts profiling
    ///
    /// \note do not let exception be thrown, just catch all and ignore
	virtual void start()
	{
        try
        {
            // request msg block only when required
            if (m_genericLogger && !m_simpleMsgBlk)
            {
                m_simpleMsgBlk = m_genericLogger->getSimpleMsgBlk();
                if (!m_simpleMsgBlk)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s, not an error, dropping profiling message as all slots are occupied...\n", FUNCTION_NAME);
                    return;
                }
            }

#ifdef SV_WINDOWS
            !(m_perfFrequency.QuadPart) && init();
#endif
            m_simpleMsgBlk && (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_startTimeStamp.set();
#ifdef SV_WINDOWS
            QueryPerformanceCounter(&m_startCounter); // make sure no code after this call
#else
            clock_gettime(CLOCK_MONOTONIC, &m_startCounter); // make sure no code after this call
#endif
        }
        catch (std::exception const& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Unknown exception caught\n", FUNCTION_NAME);
        }
	}
	
    /// \brief stops profiling and returns profiling latency time in microseconds
    ///
    /// \note If logging enabled, calls commit for every 500 messages if delayed write is opted
    /// \note do not let exception be thrown, just catch all and ignore
    /// TBD: offset used for disk read only
    virtual SV_ULONG stop(SV_ULONGLONG size = 0, SV_ULONGLONG offset = 0)
	{
        try
        {
#ifdef SV_WINDOWS
            QueryPerformanceCounter(&m_endCounter); // make sure no code before this call
#else
            clock_gettime(CLOCK_MONOTONIC, &m_endCounter); // make sure no code before this call
#endif

#ifdef SV_WINDOWS
            // init check
            if (!m_perfFrequency.QuadPart)
            {
                throw ERROR_EXCEPTION << "PerformanceFrequency not initialized. Use start before stop\n";
            }
#endif
            SV_ULONG timeDiff = latencyInMicroSec();
            if (m_genericLogger)
            {
                if (m_simpleMsgBlk)
                {
                    // update ProfilingMsg
                    (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_stopTimeStamp.set();
                    (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_timeDiff = timeDiff;
                    (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_size = size;
                    (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_operationName = m_operationName;
                    (*m_simpleMsgBlk)->m_msg[(*m_simpleMsgBlk)->m_msgIndx].m_offset = offset;

                    // bypass commit for delayed write up to DELAY_WRITE_COUNT messages (0 to DELAY_WRITE_COUNT-1)
                    /// \note as index start at 0 for n messages m_msgIndx is (n-1). So m_msgIndx increment check is <Indx < (n-1) && Indx < MaxIndx)> where MaxIndx is array boundry
                    if (m_delayedWrite && ((*m_simpleMsgBlk)->m_msgIndx < (DELAY_WRITE_COUNT - 1)) && ((*m_simpleMsgBlk)->m_msgIndx < (*m_simpleMsgBlk)->m_msgMaxIndx)) // array boundry check
                    {
                        (*m_simpleMsgBlk)->m_msgIndx++; // increment index to point to next message, as index start at 0 for n messages m_msgIndx is (n-1)
                    }
                    else
                    {
                        // pass message block to GenericLogger for logging
                        commit();
                    }
                }
                else
                {
                    // no message block slot available with logger - ignore message
                }
            }
            m_profilingBuckets && m_profilingBuckets->updateBucket(timeDiff, size);
            return timeDiff;
        }
        catch (std::exception const& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, ex.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Unknown exception caught\n", FUNCTION_NAME);
        }
        return 0;
	}
	
    /// \brief books profiling message block to GenericLogger for file writing
    ///
    /// \note should be called at last when we are done profiling
    /// \note do not let exception be thrown, just catch all and ignore
    virtual void commit()
    {
        try
        {
            if (m_simpleMsgBlk)
            {
                /// \note decrement a pre-incremented and unused index in case messages filled < DELAY_WRITE_COUNT
                if (m_delayedWrite && (((*m_simpleMsgBlk)->m_msgIndx < (DELAY_WRITE_COUNT-2)) || ((*m_simpleMsgBlk)->m_msgIndx < ((*m_simpleMsgBlk)->m_msgMaxIndx - 1))))
                {
                    (*m_simpleMsgBlk)->m_msgIndx--;
                }

                m_genericLogger->bookMsgBlk(m_simpleMsgBlk);
                m_simpleMsgBlk = NULL; // set to NULL as done with profiling
            }
        }
        catch (std::exception const& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Unknown exception caught\n", FUNCTION_NAME);
        }
	}
	
#ifdef SV_WINDOWS
    /// \brief one time initialization of QueryPerformanceFrequency
    static bool init();
#endif

protected:
    /// \brief returns profiling latency time in microseconds
	long latencyInMicroSec() const
	{
        try
        {
#ifdef SV_WINDOWS
            if (!m_perfFrequency.QuadPart)
            {
                throw ERROR_EXCEPTION << "PerformanceFrequency not initialized. Use start before stop\n";
            }
			LARGE_INTEGER ElapsedMicroseconds;
			ElapsedMicroseconds.QuadPart = m_endCounter.QuadPart - m_startCounter.QuadPart;
			ElapsedMicroseconds.QuadPart *= 1000000;
			return (ElapsedMicroseconds.QuadPart / m_perfFrequency.QuadPart); // \note can throw "devide by zero exception" if m_perfFrequency not initialized using QueryPerformanceFrequency

#elif(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
		    unsigned long long diff = (m_endCounter.tv_sec*1000000000 + m_endCounter.tv_nsec) - (m_startCounter.tv_sec*1000000000 + m_startCounter.tv_nsec);
		    return diff /= 1000;
#else
		    return -1;
#endif
        }
        catch (std::exception const& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Exception caught: %s\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s, Unknown exception caught\n", FUNCTION_NAME);
        }
	}

    void profilingSunmmaryAsJson(std::vector<PrBuckets> &vpb) const
    {
        if (m_profilingSummary)
        {
            m_profilingSummary->getSummaryAsJson(vpb);
        }
    }

    void profilingSunmmaryAsData(std::vector<boost::shared_ptr<ProfilingBuckets> > &vpb) const
    {
        if (m_profilingSummary)
        {
            m_profilingSummary->getSummaryAsData(vpb);
        }
    }

private:
    std::string m_operationName; ///< name of profiling operation

    const bool m_delayedWrite; ///< indicate instant or delayed booking of profiling message to GenericLogger

    SIMPLE_LOGGER::GenericLogger<ProfilingMsg> *m_genericLogger; ///< reference to associated GenericLogger object

    SIMPLE_LOGGER::SimpleMsgBlk<ProfilingMsg> **m_simpleMsgBlk; ///< reference to a message block that holds a vector of ProfilingMsg type messages

#ifdef SV_WINDOWS
    LARGE_INTEGER m_startCounter, m_endCounter; ///< holds current value of the performance counter retrieved by QueryPerformanceCounter

    static LARGE_INTEGER m_perfFrequency; ///< holds frequency of the performance counter retrieved by QueryPerformanceFrequency

    static boost::mutex m_queryPerfFreqLock; ///< mutex for one time initialization of m_perfFrequency
#else
    struct timespec m_startCounter, m_endCounter; ///< holds resolution of CLOCK_MONOTONIC retrieved by clock_gettime
#endif

    ProfilingBuckets *m_profilingBuckets;

    ProfilingSummary *m_profilingSummary;
};

/// \brief a dummy profiler class implementing _Profiler with dummy methods to turn off profiling when not required
// TBD: use singelton passive profiler
class PassiveProfiler : public _Profiler
{
public:
    PassiveProfiler() {}

    virtual void start() {}

    virtual SV_ULONG stop(SV_ULONGLONG size = 0, SV_ULONGLONG offset = 0) { return 0; }

    virtual void commit() {}

    virtual void profilingSunmmaryAsJson(std::vector<PrBuckets> &vpb) const { }

    virtual void profilingSunmmaryAsData(std::vector<boost::shared_ptr<ProfilingBuckets> > &vpb) const { }

    virtual ~PassiveProfiler() {}
};

/// \brief a factroy interface to procure Profiler
// TBD: update comments
class GenericLoggerFactory
{
public:
    /// \brief creates and returns GenericLogger for profilingOperation
    ///
    /// \param profilingOperation name of profiling operation E.G. <diskRead>
    /// \param fileName log file where profiling is logged
    static SIMPLE_LOGGER::GenericLogger<ProfilingMsg>* getGenericLogger(const std::string profilingOperation, const std::string fileName = "");

    /// \brief turns off all GenericLogger by graceful exit of writer threads and releasing associated GenericLoggers
    ///
    /// \note should be called once at end of user application
    static void exitGenericLogger();// Should be called once at end of application

private:
    static GenericLoggerMap m_genericLoggerMap; ///< a map of GenericLogger associated with file name

    static boost::mutex m_lock; ///< lock to create GenericLogger 
};

/// \brief a factroy interface to procure Profiler
// TBD: update comments
class ProfilerFactory
{
public:
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
    static Profiler getProfiler(PROFILER_TYPE pt, const std::string operationName = "", ProfilingBuckets *profBktsType = NULL, bool verbose = false, const std::string fileName = "", const bool delayedWrite = false, bool bCumulative = true);
    
    /// \brief turns off all profilers by graceful exit of writer threads and releasing associated GenericLoggers
    ///
    /// \note should be called once at end of user application
    static void exitProfiler();// Should be called once at end of application
};
#endif //GENERICPROFILER_H
