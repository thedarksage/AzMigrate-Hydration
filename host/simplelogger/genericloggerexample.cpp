
///
/// \file genericloggerTest.cpp
///
/// \brief shows how to use genericlogger
///

#include <iostream>

#include "genericlogger.h"

#include <boost/thread/thread.hpp>;

void test4_thread(SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *gl)
{
    int n = 2;
    for (int i = 1; i <= n; ++i)
    {
        SIMPLE_LOGGER::SimpleMsgBlk<SIMPLE_LOGGER::SimpleMsg> **msg = gl->getSimpleMsgBlk(); // never delete this slot of SimpleMsgBlk

        (*msg)->m_msg->operator<<(ACE_Thread::self()); // current thread ID
        (*msg)->m_msg->operator<<(":message:");
        (*msg)->m_msg->operator<<(i);
        (*msg)->m_msg->operator<<("\n");
        gl->bookMsgBlk(msg);
    }
}

void test5_thread(SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *gl)
{
    int n = 10;
    for (int i = 1; i <= n; ++i)
    {
        SIMPLE_LOGGER::SimpleMsgBlk<SIMPLE_LOGGER::SimpleMsg> **msg = gl->getSimpleMsgBlk(); // never delete this slot of SimpleMsgBlk

        (*msg)->m_msg->operator<<(ACE_Thread::self()); // current thread ID
        (*msg)->m_msg->operator<<(":message:");
        (*msg)->m_msg->operator<<(i);
        (*msg)->m_msg->operator<<("\n");
        gl->bookMsgBlk(msg);
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    }
}

int main(int argc, char ** argv)
{
    /// init ACE
    ACE::init();

    /// init CDPUtil quit event
    CDPUtil::InitQuitEvent(false);

    /// initalize LogWriter
    SIMPLE_LOGGER::LogWriter* logWriter = SIMPLE_LOGGER::LogWriterCreator::getLogWriter();

    /// ****************************************** TEST #1 ******************************************
    /// \test verify GenericLogger creates appropriate log file and verify log messages in log file
    /// define file name
    boost::filesystem::path logName("genericlogger-example_1.log");

    /// initalize GenericLogger
    SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *glTest1 = \
        new SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg>(logName, logWriter, 100, 3, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);

    /// get available slot of SimpleMsgBlk
    SIMPLE_LOGGER::SimpleMsgBlk<SIMPLE_LOGGER::SimpleMsg> **mb1 = glTest1->getSimpleMsgBlk(); // never delete this slot of SimpleMsgBlk

    /// define and set message type which has public method "std::string str()" for example <SIMPLE_LOGGER::SimpleMsg>
    /// add 5 messages to message block
    int n = 5;
    for (int i = 1; i <= n; ++i)
    {
        (*mb1)->m_msg->operator<<(ACE_Thread::self()); // current thread ID
        (*mb1)->m_msg->operator<<(":message:");
        (*mb1)->m_msg->operator<<(i);
        (*mb1)->m_msg->operator<<("\n");
        /// \note as index start at 0 for n messages m_msgIndx is (n-1). So index increment check is <Indx < (n-1) && Indx < MaxIndx)> where MaxIndx is array boundry
        if ((*mb1)->m_msgIndx < (n - 1) && (*mb1)->m_msgIndx < (*mb1)->m_msgMaxIndx) // array boundry check
        {
            (*mb1)->m_msgIndx++; // increment index to point to next message
        }
    }

    /// book message block for logging
    glTest1->bookMsgBlk(mb1);

    /// ****************************************** TEST #2 ******************************************
    /// \test verify log file truncate works for preconfigured log file size greater than 10 bytes
    logName = "genericlogger-example_2.log";
    SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *glTest2 = \
        new SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg>(logName, logWriter, 10, 3, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);
    n = 2;
    for (int i = 1; i <= n; ++i)
    {
        SIMPLE_LOGGER::SimpleMsgBlk<SIMPLE_LOGGER::SimpleMsg> **mb2 = glTest2->getSimpleMsgBlk(); // never delete this slot of SimpleMsgBlk

        (*mb2)->m_msg->operator<<(ACE_Thread::self()); // current thread ID
        (*mb2)->m_msg->operator<<(":message:");
        (*mb2)->m_msg->operator<<(i);
        (*mb2)->m_msg->operator<<("\n");
        glTest2->bookMsgBlk(mb2);
    }

    /// ****************************************** TEST #3 ******************************************
    /// \test verify log file rotation logic works for preconfigured rotation count
    logName = "genericlogger-example_3.log";
    SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *glTest3 = \
        new SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg>(logName, logWriter, 10, 3, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);
    n = 4;
    for (int i = 1; i <= n; ++i)
    {
        SIMPLE_LOGGER::SimpleMsgBlk<SIMPLE_LOGGER::SimpleMsg> **mb3 = glTest3->getSimpleMsgBlk(); // never delete this slot of SimpleMsgBlk

        (*mb3)->m_msg->operator<<(ACE_Thread::self()); // current thread ID
        (*mb3)->m_msg->operator<<(":message:");
        (*mb3)->m_msg->operator<<(i);
        (*mb3)->m_msg->operator<<("\n");
        glTest3->bookMsgBlk(mb3);
    }

    /// ****************************************** TEST #4 ******************************************
    /// \test verify different threads sharing same logger loger successfully to same file
    logName = "genericlogger-example_4.log";
    SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *glTest4 = new SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg>(logName, logWriter, 1024, 3, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);

    boost::thread t4_1 = boost::thread(&test4_thread, glTest4);
    boost::thread t4_2 = boost::thread(&test4_thread, glTest4);
    t4_1.join();
    t4_2.join();

    /// ****************************************** TEST #5 ******************************************
    /// \test verify writer thread exit on quit event and messages enqued after quit event not logged
    /// to verify this, pause 1 sec after each message enque in thread, in main thread raise quit signal after 3 sec
    /// So writer thread should exit after logging only 3-4 messages approximately
    logName = "genericlogger-example_6.log";
    SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg> *glTest6 = new SIMPLE_LOGGER::GenericLogger<SIMPLE_LOGGER::SimpleMsg>(logName, logWriter, 1024, 3, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);

    boost::thread t6 = boost::thread(&test5_thread, glTest6);
    boost::this_thread::sleep(boost::posix_time::milliseconds(3000));
    CDPUtil::SignalQuit();
    t6.join();

    /// cleaning and exit
    ///
    /// stop LogWriter
    SIMPLE_LOGGER::LogWriterCreator::stopLogWriterThread();

    glTest1->printStats();
    glTest2->printStats();
    glTest3->printStats();
    glTest4->printStats();
    glTest6->printStats();

    /// release LogWriter
    SIMPLE_LOGGER::LogWriterCreator::releaseLogWriter();

    std::cout << "Goodby!" << std::endl;

    return 1;
}


/*

TEST #1 expected o/p:
- 2 files created as,
./genericlogger-example_2.<rotation timestamp>.log.gz
./genericlogger-example_2.log

- Contents in genericlogger-example_2.<rotation timestamp>.log.gz:
14400:message:1

- Contents in genericlogger-example_2.log:
14400:message:2
File=./genericlogger-example_2.log,Add Count=2,Write Count=2

TEST #3 expected o/p:
- 5 log files created: 4 in zip form, 1 zip file deleted, 1 last log file as,
genericlogger-example_3.20160307T130308.635466.log.gz
genericlogger-example_3.20160307T130316.071746.log.gz
genericlogger-example_3.20160307T130337.679875.log.gz
genericlogger-example_3.log

- Contents in genericlogger-example_3.20160307T130308.635466.log
11912:message:1

- Contents in genericlogger-example_3.20160307T130316.071746.log
11912:message:2

- Contents in genericlogger-example_3.20160307T130337.679875.log
11912:message:3

- Contents in genericlogger-example_3.log:
11912:message:4
File=./genericlogger-example_3.log,Add Count=4,Write Count=4


TEST #4 expected o/p:
- Both threads should log 2 messages in genericlogger-example_4.log

- Contents in genericlogger-example_4.log
15792:message:1
22248:message:1
22248:message:2
15792:message:2
File=./genericlogger-example_4.log,Add Count=4,Write Count=4

TEST #5 expected o/p:
- add count should be 10, write count should be 3 or 4

- Contents in genericlogger-example_5.log
10008:message:1
10008:message:2
10008:message:3
10008:message:4
File=./genericlogger-example_5.log,Add Count=10,Write Count=4

*/


