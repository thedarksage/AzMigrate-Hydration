
///
///  \file  genericlogger.h
///
///  \brief a generic logger that do logging in a dedicated single thread irrespective of number of log files
///
/// \example simplelogger/genericloggerexample.cpp

#ifndef GENERICLOGGER_H
#define GENERICLOGGER_H

#include <iostream>
#include <string>
#include <vector>
#include <list>

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>

#include "simplelogger.h"
#include "sigslot.h"
#include "svtypes.h"
#include "errorexception.h"
#include "cdputil.h"

/// \brief simple logger namesapce
namespace SIMPLE_LOGGER {

    #define MAX_MSG_BLOCKS 40
    #define MSG_MAX_INDX 500
    #define LOG_SIZE 1024*1024*15
    #define ROTATION_COUNT 3

    typedef sigslot::signal0<> SigslotSignal0;

    typedef std::vector<SigslotSignal0*> WriteLogSignals;
    
    /// \brief forward declaration
    template<typename MSG_TYPE>
    class GenericLogger;

    /// \brief SimpleMsgBlk holds an array of generic messages of type <MSG_TYPE>
    ///
    /// \note a message of type <MSG_TYPE> must implement a public method <std::string str()>
    template <typename MSG_TYPE>
    struct SimpleMsgBlk
    {
    public:
        SimpleMsgBlk() : m_msgIndx(0), m_msgMaxIndx(MSG_MAX_INDX-1), m_filled(false), m_booked(false) {}

    public:
        MSG_TYPE m_msg[MSG_MAX_INDX];
        int m_msgIndx; ///< current index
        const int m_msgMaxIndx; ///< array boundry

        /// \brief GenericLogger is declared friend to have fine control over state indicators
        friend class GenericLogger<MSG_TYPE>;

    private:
        bool m_filled; ///< <0=empty, 1=filled> state indicator indicates slot filled state
        bool m_booked; ///< <0=available, 1=booked> state indicator indicates slot is booked for fill or drain
        /*
         *  /// \brief sudo code for using state flags
         *  // producer: if(filled==0 && booked==0) { book slot(booked=1) -> fill  -> indicate filled(filled=1) -> release slot(booked=0) } \note use with lock as multiple user threads can request for SimpleMsgBlk slot
         *  // consumer: if(filled==1 && booked==0) { book slot(booked=1) -> drain -> indicate empty(filled=0)  -> release slot(booked=0) } \note processed by only one thread so no lock required
         *
         *  stage     filled    booked    state indicator         actor                                           check
         *  ========================================================================================================================================================================
         *    I        0	     0        available for fill      requestor (getSimpleMsgBlk) check for slot	  00 : filler check
         *   II        0	    *1        booked for write        set by requestor (getSimpleMsgBlk)              \note use with mutex as more than one thread can request same time
         *  III       *1	    *0        available for drain     set by requestor (bookMsgBlk) after filled
         *   IV        1	     0        available for drain     writer (writeToFile) check for drain   	      10 : drainer check, \note only one reader so no lock required
         *    V        1	    *1        booked for drain        set by writer (writeToFile) for drain
         *   VI       *0	    *0        available for fill      set by writer (writeToFile) for fill
         *
         *  \note (*) indicates state change in stage
         *
         */
    };

    class LogWriterCreator;

    /// \brief helper thread for GenericLogger.
    ///
    /// LogWriter runs in a wakable sleep mode and calls each registered method registered by each GenericLogger instance that do actual file logging
    /// LogWriter is a singleton class so all instances of GenericLogger will share same LogWriter
    class LogWriter
    {
    public:
        /// \brief registers function of GenericLogger with an associated signal in WriteLogSignals
        template <typename MSG_TYPE>
        bool registerLoggerSignal(GenericLogger<MSG_TYPE>* genericLogger, void (GenericLogger<MSG_TYPE>::*signal)())
        {
            boost::unique_lock<boost::mutex> lock(m_writerLock);
            SigslotSignal0 * writeLogSignal = new SigslotSignal0();
            writeLogSignal->connect(genericLogger, signal);
            m_writeLogSignals.push_back(writeLogSignal);
            return true;
        }

        /// \brief an interface class to create and return and manage singleton instance of LogWriter
        friend class LogWriterCreator;

        /// \brief GenericLogger is declared friend to have fine control over lock and condition variale of wrter thread
        template <typename MSG_TYPE>
        friend class GenericLogger;

    protected:
        /// \brief indicates writer thread to stop and exit
        void stop()
        {
            if (!m_stop)
            {
                boost::unique_lock<boost::mutex> lock(m_writerLock);
                m_stop = true;
                lock.unlock();
            }
            m_writerThread->join();
        }

        /// \brief releases allocated resource memory
        ///
        /// \exception throws ERROR_EXCEPTION on error
        void release()
        {
            if (!m_stop)
            {
                throw ERROR_EXCEPTION << "LogWriter thread still runing. Stop LogWriter thread and try again." << '\n';
            }
            boost::unique_lock<boost::mutex> lock(m_writerLock);
            WriteLogSignals::iterator itr = m_writeLogSignals.begin();
            for (itr; itr != m_writeLogSignals.end(); ++itr)
            {
                delete *itr;
            }
            m_writeLogSignals.empty();

            delete m_writerCondVar;
            delete m_writerThread;
        }

        /// \brief Verifies quit event is set
        bool quitRequested(int seconds = 0)
        {
            if (m_stop || CDPUtil::Quit()) {
                return true;
            }

            if (CDPUtil::QuitRequested(seconds)) {
                m_stop = true;
            }

            return (m_stop || CDPUtil::Quit());
        }

    private:
        /// \brief private default constructor for singleton LogWriter
        LogWriter() : m_stop(false)
        {
            m_writerCondVar = new boost::condition_variable();
            m_writerThread = new boost::thread(boost::bind(&LogWriter::write, this));
        }

        /// \brief private copy constructor for singleton LogWriter
        LogWriter(const LogWriter&){}

        /// \brief overloaded private assignment operator for singleton LogWriter
        LogWriter& operator=(const LogWriter&);

    private:
        /// \brief actual thread function that iterate through m_writeLogSignals and call each method registered  by GenericLogger instances
        void write()
        {
            while (true)
            {
                boost::unique_lock<boost::mutex> lock(m_writerLock);
                WriteLogSignals::iterator itr = m_writeLogSignals.begin();
                for (itr; itr != m_writeLogSignals.end(); ++itr)
                {
                    // calls registered function of GenericLogger
                    (*(*itr))();
                }
                if (quitRequested())
                {
                    break;
                }
                // interruptable sleep for 15 seconds before next iteration
                boost::system_time const timeout = boost::get_system_time() + boost::posix_time::milliseconds(15000);
                m_writerCondVar->timed_wait(lock, timeout);
            }
        }

        /// \brief Interrupts sleeping  writer thread.
        ///
        /// Private to have fine control over condition variable so that only friend classes should access.
        void wake()
        {
            m_writerCondVar->notify_one();
        }

        /// \brief returns mutex associated with condition varialble.
        ///
        /// Private to have fine control over lock so that only friend classes should access.
        boost::mutex& getWriteLock()
        {
            return m_writerLock;
        }

    private:
        bool m_stop; ///< indicates writer thread to exit

        boost::thread *m_writerThread; ///< a dedicated singleton thread to invoke registered methods of each GenericLogger

        WriteLogSignals m_writeLogSignals; ///< a vector of registered methods of each GenericLogger

        boost::condition_variable *m_writerCondVar; ///< condition variable to make writer thread sleep after completion of one iteration in WriteLogSignals
                                                    ///< used by each registered GenericLogger instance to wake writer thread if sleeping

        boost::mutex m_writerLock; ///< associated with condition variable to wake writer thread. Used by each registered GenericLogger instance to wake writer thread if sleeping
    };

    /// \brief helper class to create and manage singleton LogWriter
    class LogWriterCreator
    {
    public:
        /// \brief returns singleton LogWriter instance
        static LogWriter* getLogWriter()
        {
            if (!m_isInit)
            {
                boost::unique_lock<boost::mutex> lock(m_lock);
                if (!m_isInit)
                {
                    m_logWriter = new LogWriter();
                    m_isInit = true;
                }
            }
            return m_logWriter;
        }

        /// \brief indicates LogWriter thread to stop and exit
        static void stopLogWriterThread()
        {
            if (m_isInit)
            {
                m_logWriter->stop();
            }
        }

        /// \brief releases LogWriter singleton instance
        static void releaseLogWriter()
        {
            if (m_isInit)
            {
                m_logWriter->release();

                delete m_logWriter;

                m_isInit = false;
            }
        }

    private:
        static bool m_isInit; ///< indicates LogWriter singleton instance initialization state

        static LogWriter* m_logWriter; ///< LogWriter singleton instance

        static boost::mutex m_lock; ///< lock for first time creating LogWriter singleton instance
    };



    /// \brief a generic class to log generic message type <MSG_TYPE> to file
    ///
    /// GenericLogger holds an array of SimpleMsgBlk and process each SimpleMsgBlk to actual file log all messages hold by SimpleMsgBlk
    ///
    /// \note a generic type MSG_TYPE should implement a public method <std::string str()> in order to use this class
    template<typename MSG_TYPE>
    class GenericLogger : public SimpleLogger, public sigslot::has_slots<>
    {
    public:

        /// \brief default constructor
        explicit GenericLogger(boost::filesystem::path & fileName,                          ///< name of log file
                                LogWriter* logWriter,                                       ///< LogWriter reference
                                int maxLogSizeBytes = LOG_SIZE,                             ///< maximum size of the log file before rotating
                                int rotateCount = ROTATION_COUNT,                           ///< number of days to retain rotated log files
                                int retainSizeBytes = 0,                                    ///< number of bytes to keep in log file when rotating if rotateCount is 0 (use 0 to keep default)
                                bool enabled = true,                                        ///< is logging enabled true: yes, false: noe
                                LOG_LEVEL logLevel = LEVEL_ERROR) :                           ///< user defined value for the current log level
                                SimpleLogger(fileName, maxLogSizeBytes, rotateCount, retainSizeBytes, enabled, logLevel),
                                m_logWriter(logWriter),
                                m_msgAddCount(0),
                                m_msgWriteCount(0)
        {
            // allocate memory slots for m_msgBlocks
            m_msgBlocks = new SimpleMsgBlk<MSG_TYPE>* [MAX_MSG_BLOCKS];
            for (int indx = 0; indx < MAX_MSG_BLOCKS; ++indx)
            {
                m_msgBlocks[indx] = new SimpleMsgBlk<MSG_TYPE>;
                // TBD: init m_msgBlocks[indx] with 0
                m_freeSlotIndxList.push_back(indx); /// Initial availability of slots
            }

            m_logWriter->registerLoggerSignal(this, &GenericLogger::writeToFile);
        }

        /// \brief returns available SimpleMsgBlk slot to fill
        ///
        /// \note use lock as multiple user threads can request for SimpleMsgBlk slot
        SimpleMsgBlk<MSG_TYPE> ** getSimpleMsgBlk()
        {
            if (!m_freeSlotIndxList.empty())
            {
                boost::unique_lock<boost::mutex> lock(m_freeSlotIndxListLock);
                if (!m_freeSlotIndxList.empty())
                {
                    int sindx = m_freeSlotIndxList.front();
                    m_msgBlocks[sindx]->m_booked = true;
                    m_freeSlotIndxList.pop_front();
                    return &(m_msgBlocks[sindx]);
                }
            }
            return NULL; // all slots occupied try again
        }

        /// \brief enqueues SimpleMsgBlk to be processed later in writeToFile to actual file log all messages it holds
        void bookMsgBlk(SimpleMsgBlk<MSG_TYPE> **simpleMsgBlk)
        {
            m_msgAddCount += ((*simpleMsgBlk)->m_msgIndx + 1); // update stats to verify processed messages count at end. \note add 1 as index starts from 0

            (*simpleMsgBlk)->m_filled = true; // \note refer SimpleMsgBlk definition comments
            (*simpleMsgBlk)->m_booked = false; // \note refer SimpleMsgBlk definition comments

            // use mutex associated with condition variable of LogWriter
            // then try to aquire this mutex
            //      if success : LogWriter thread is in sleeping mode, signal to resume
            //      else : LogWriter thread is already busy so do nothing
            boost::unique_lock<boost::mutex> lock(m_logWriter->getWriteLock(), boost::try_to_lock); // Use try_lock as LogWriter may be already processing msg
            if (lock.owns_lock())
            {
                m_logWriter->wake();
            }
        }

        /// \brief logs stats of processed messages
        void printStats()
        {
            std::stringstream len;
            len << "File=";
            len << SimpleLogger::getLogName();
            len << ",";
            len << "Add Count=";
            len << m_msgAddCount.value();
            len << ",";
            len << "Write Count=";
            len << m_msgWriteCount.value();
            len << std::endl;
            SimpleLogger::doLog(len.str().c_str(), false);
        }

        /// \brief a thread safe version of writeToFile
        void writeToFileSafe()
        {
            boost::unique_lock<boost::mutex> lock(m_writeSafeLock);
            writeToFile();
        }

        ~GenericLogger()
        {
            // release memory slots for m_msgBlocks
            for (int indx = 0; indx < MAX_MSG_BLOCKS; ++indx)
            {
                delete m_msgBlocks[indx];
            }
            delete m_msgBlocks;
        }

    protected:

        void updateSlotIndxList(const int &sno)
        {
            // scoped so the lock is released after adding free slot index to list
            boost::unique_lock<boost::mutex> lock(m_freeSlotIndxListLock);
            m_freeSlotIndxList.push_back(sno); /// add to available free slots list
        }

        /// \brief iterates through array of SimpleMsgBlk to processes all messages hold by each SimpleMsgBlk
        ///
        /// \note writeToFile is registered to LogWriter and is called in LogWriter thread
        ///
        /// \note geberic message type <MSG_TYPE> should implement public method "std::string str()"
        ///
        /// declared protected as not thread safe so should be called only by friend class LogWriter
        void writeToFile()
        {
            try
            {
                for (int sno = 0; sno < MAX_MSG_BLOCKS; ++sno)
                {
                    // \note check if(filled==1 && booked==0) { book slot(booked=1) -> drain -> indicate empty(filled=0)  -> release slot(booked=0) }
                    // \note only one reader so no lock required
                    if (m_msgBlocks[sno]->m_filled && !m_msgBlocks[sno]->m_booked) // \note refer SimpleMsgBlk definition comments
                    {
                        m_msgBlocks[sno]->m_booked = true; // \note refer SimpleMsgBlk definition comments

                        int indx = 0;
                        while ((indx <= m_msgBlocks[sno]->m_msgIndx) && (indx <= m_msgBlocks[sno]->m_msgMaxIndx))  // array boundry check
                        {
                            std::stringstream ss;
                            int count = 0;
                            for (indx, count; (indx <= m_msgBlocks[sno]->m_msgIndx) && (indx <= m_msgBlocks[sno]->m_msgMaxIndx) && count < 500; ++indx, ++count) // array boundry check
                            {
                                ss << m_msgBlocks[sno]->m_msg[indx].str() << std::endl; // \note geberic message type <MSG_TYPE> should implement public method "std::string str()"
                            }
                            SimpleLogger::openLogIfNeeded();
                            SimpleLogger::rotateLogIfNeeded(false); // Monitor thread OFF
                            SimpleLogger::doLog(ss.str().c_str(), false);
                        }
                        m_msgWriteCount += (m_msgBlocks[sno]->m_msgIndx + 1); // \note add 1 as index starts from 0

                        m_msgBlocks[sno]->m_msgIndx = 0; // reset index to 0
                        m_msgBlocks[sno]->m_filled = false; // \note refer SimpleMsgBlk definition comments
                        m_msgBlocks[sno]->m_booked = false; // \note refer SimpleMsgBlk definition comments

                        updateSlotIndxList(sno);
                    }
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

    private:

        LogWriter *m_logWriter; ///< reference to singleton LogWriter instance

        SimpleMsgBlk<MSG_TYPE> **m_msgBlocks; ///< predefined array of 20 SimpleMsgBlk for reuse of slots

        std::list<int> m_freeSlotIndxList; ///< indicates available slots of m_msgBlocks

        boost::mutex m_freeSlotIndxListLock; ///< lock to push and pop free index to m_freeSlotIndxListList

        boost::mutex m_writeSafeLock; ///< lock for thread safe implementation of writeToFileSafe

        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONG> m_msgAddCount; ///< a stats counter to count incoming messages. Atomic as can be updated by multiple threads

        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONG> m_msgWriteCount; ///< a stats counter to count processed messages. Atomic as can be updated by multiple threads
    };


} // namespace SIMPLE_LOGGER


#endif // GENERICLOGGER_H
