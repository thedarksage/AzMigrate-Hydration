#ifndef __COMMUNICATOR_H
#define __COMMUNICATOR_H

#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#include <boost/chrono.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <ace/OS.h>
#include <ace/Mutex.h>
#include <ace/Event.h>
#include <ace/Synch.h>
#include <ace/Thread_Manager.h>
#include <ace/Atomic_Op.h>
#include <ace/Message_Queue_T.h>

#include "PerfCounter.h"
#include "SecureTransport.h"
#include "VacpCommon.h"
#include "VacpErrorCodes.h"

using namespace boost::chrono;

typedef boost::shared_ptr<ACE_Time_Value> SP_ACE_TIME_VALUE;

#ifndef WIN32
typedef unsigned char   BYTE;
typedef unsigned int    UINT;
typedef unsigned int    UINT32;
typedef unsigned long   DWORD;
typedef unsigned short  USHORT;
typedef unsigned short  UINT16;

#define DebugPrintf inm_printf

#ifdef DEBUG
#define XDebugPrintf inm_printf
#else
#define XDebugPrintf
#endif

#endif

extern std::string g_strRecvdTagGuid;

extern const char* DVACP_PACKET_STARTER_STRING;
extern const char* DVACP_PORT;
extern const int DVACP_SOCK_SERVER_PORT;
extern const int ERROR_NULL_MESSAGE_RECEIVED;
extern const int ERROR_INVALID_PACKET_REFERENCE;
extern const int ERROR_INVALID_TARGET_NODE_NAME_LENGTH;
extern const int ERROR_INVALID_DATA_LENGTH;
extern const int ERROR_CLIENT_SOCKET_CREATION_FAILED;
extern const int ERROR_FAILED_TO_SEND_CMD_TO_COORDINATOR_NODES;
extern const int ERROR_FAILED_TO_SEND_MSG_TO_MASTER_NODE;
extern const int ERROR_INVALID_SOCKET;


extern std::string g_strLocalIpAddress;
extern std::string g_strLocalHostName;
extern std::string g_strMasterHostName;

enum ENUM_MSG_TYPE
{
    ENUM_MSG_SPAWN_CMD =65,
    ENUM_MSG_SPAWN_ACK =66,
    ENUM_MSG_PREPARE_CMD =67,
    ENUM_MSG_PREPARE_ACK =68,
    ENUM_MSG_QUIESCE_CMD =69,
    ENUM_MSG_QUIESCE_ACK =70,
    ENUM_MSG_TAG_COMMIT_CMD =71,
    ENUM_MSG_TAG_COMMIT_ACK =72,
    ENUM_MSG_STATUS_CMD =73,
    ENUM_MSG_STATUS_ACK =74,
    ENUM_MSG_SEND_TAG_GUID =75,
    ENUM_MSG_STATUS_INFO =76,
    ENUM_MSG_RESUME_ACK =77,
    ENUM_MSG_RESUME_CMD =78,
    //ENUM_MSG_STATUS_ACK,
    ENUM_MSG_SCHEDULE_CMD = 79,
    ENUM_MSG_SCHEDULE_ACK = 80,
    ENUM_MSG_OTHER
};

enum ENUM_DATA_TYPE
{
    ENUM_UNSIGNED_CHAR = 0,
    ENUM_UNSIGNED_SHORT = 1,
    ENUM_UNSIGNED_INT = 2,
    ENUM_UNSIGNED_LONG = 3,
    ENUM_UNSIGNED_DOUBLE = 4,
    ENUM_CHAR = 5,
    ENUM_SHORT = 6,
    ENUM_INT = 7,
    ENUM_LONG = 8,
    ENUM_DOUBLE = 9
};

/*
//
Protocol Definition:
------------------------------
<CommunicationId=unsigned int;
RequestId=unsigned int;
Direction=unsigned char M2C=0/C2M=1;
srcNodeLen=unsignd char;
SrcNode=char*;
destNodeLen=unsigned char;
DestNode=char*;
FunctionId=unsigned char;
DataLen=unsigned short;
Data=unsigned char*;>

Function Id	Function Description		Data
0		Invoke Vacp.exe process	Command line
1		SpawnedVacpAck		0
2		SendPrepareCmd		0
3		SendPrepareAck		0
4		SendQuiesceCmd		0
5		SendQuiesceAck		0
6		CommitTagCmd		0
7		CommitTagAck		0
8		SendStatusCmd		0
9		SendStatusAck		0= success, any non-zero is error code
10		SendTagGuid		Tag GUID

*/

typedef struct _stPacket
{
    //The first 19 bytes are standard and is treated as Header Packet
    std::string strPacketStarter;//VacpPacket (10 Bytes)
    UINT totalPacketLen;//4 bytes
    BYTE packetType;//1 byte;65 (or A means a new packet). 91 (or Z means it is a response or Ack packet);
    UINT requestId;//4 bytes;if packetType is A then it is a new request ID or if packetType is Z then it is response(ACK) packet for original requestId

    UINT socketId;
    UINT commSequenceId;
    BYTE direction;//A=Master to Co-ordinator Node; Z=Co-rodinator Node to Master Node
    BYTE FunctionId;
    BYTE srcNodeLen;

    std::string  strSrcNode;
    BYTE destNodeLen;
    std::string strDestNode;  
    USHORT dataLen;
    std::string strData;

    unsigned char* ReadBytes(	unsigned char* pBuffer,
    unsigned short nStartPos,
    unsigned short numberOfBytes);
  
    unsigned int ConvertToNative(unsigned char* recvdBytes, ENUM_DATA_TYPE type);

    unsigned char* ConvertToNetworkByteOrder(unsigned int intNum, ENUM_DATA_TYPE type, unsigned char* sendBytes);

    std::string ConvertToString(unsigned char* pRecvdBytes,unsigned int nLen);

    void Clear();
} stPacket;

struct MsgBucket
{
    bool bConnected;
    bool bSpawnVacpAck;
    bool bSendTagGuid;
    bool bPrepareCmd;

    bool bPrepareAck;
    bool bQuiesceCmd;
    bool bQuiesceAck;
    bool bCommitTagCmd;

    bool bCommitTagAck;
    bool bResumeCmd;
    bool bResumeAck;
    bool bStatusCmd;

    bool bStatusAck;
    bool bStatusInfo;

    std::string strDesc;
    bool bGotExcluded;
    int sock;

    MsgBucket()
    {
        bConnected = false;
        bSpawnVacpAck = false;
        bSendTagGuid = false;
        bPrepareCmd = false;

        bPrepareAck = false;
        bQuiesceCmd = false;
        bQuiesceAck =false;
        bCommitTagCmd = false;

        bCommitTagAck = false;
        bResumeCmd = false;
        bResumeAck = false;
        bStatusCmd = false;

        bStatusAck = false;
        std::string strDesc ="";
        bGotExcluded = false;
        sock = 0;
    }
};

class CommunicatorTimeouts
{
public:
    SP_ACE_TIME_VALUE m_serverCreateSockMaxTime;
    SP_ACE_TIME_VALUE m_serverCreateSockAttemptFrequency;
    SP_ACE_TIME_VALUE m_serverSpawnAckWaitTime;
    SP_ACE_TIME_VALUE m_serverPrepareAckWaitTime;
    SP_ACE_TIME_VALUE m_serverQuiesceAckWaitTime;
    SP_ACE_TIME_VALUE m_serverInsertTagAckWaitTime;
    SP_ACE_TIME_VALUE m_serverResumeAckWaitTime;

    SP_ACE_TIME_VALUE m_clientConnectSockMaxTime;
    SP_ACE_TIME_VALUE m_clientConnectSockAttemptFrequency;
    SP_ACE_TIME_VALUE m_clientTagGuidRecvWaitTime;
    SP_ACE_TIME_VALUE m_clientPrepareCmdWaitTime;
    SP_ACE_TIME_VALUE m_clientQuiesceCmdWaitTime;
    SP_ACE_TIME_VALUE m_clientInsertTagCmdWaitTime;
    SP_ACE_TIME_VALUE m_clientResumeCmdWaitTime;

    std::map<string, SP_ACE_TIME_VALUE> m_mapTimeouts;

private:

    void InitTimeoutMap()
    {
        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerCreateSockMaxTime", m_serverCreateSockMaxTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerCreateSockAttemptFrequency", m_serverCreateSockAttemptFrequency));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerSpawnAckWaitTime", m_serverSpawnAckWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerPrepareAckWaitTime", m_serverPrepareAckWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerQuiesceAckWaitTime", m_serverQuiesceAckWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerInsertTagAckWaitTime", m_serverInsertTagAckWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ServerResumeAckWaitTime", m_serverResumeAckWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientConnectSockMaxTime", m_clientConnectSockMaxTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientConnectSockAttemptFrequency", m_clientConnectSockAttemptFrequency));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientTagGuidRecvWaitTime", m_clientTagGuidRecvWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientPrepareCmdWaitTime", m_clientPrepareCmdWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientQuiesceCmdWaitTime", m_clientQuiesceCmdWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientInsertTagCmdWaitTime", m_clientInsertTagCmdWaitTime));

        m_mapTimeouts.insert(
            std::pair<string, SP_ACE_TIME_VALUE>
            ("ClientResumeCmdWaitTime", m_clientResumeCmdWaitTime));

        return;
    }

    void SetServerTimeouts()
    {
        m_serverCreateSockMaxTime->msec(30000);
        m_serverCreateSockAttemptFrequency->msec(5000);
        m_serverSpawnAckWaitTime->msec(90000);

        m_serverPrepareAckWaitTime->msec(300);
        m_serverQuiesceAckWaitTime->msec(100);
        m_serverInsertTagAckWaitTime->msec(100);
        m_serverResumeAckWaitTime->msec(100);

    }

    void SetClientTimeouts()
    {
        m_clientConnectSockMaxTime->msec(90000);
        m_clientConnectSockAttemptFrequency->msec(5000);
        m_clientTagGuidRecvWaitTime->msec(90010);

        m_clientPrepareCmdWaitTime->msec(310);
        m_clientQuiesceCmdWaitTime->msec(310);
        m_clientInsertTagCmdWaitTime->msec(110);
        m_clientResumeCmdWaitTime->msec(110);
    }

public:
    CommunicatorTimeouts() :
        m_serverCreateSockMaxTime(new ACE_Time_Value),
        m_serverCreateSockAttemptFrequency(new ACE_Time_Value),
        m_serverSpawnAckWaitTime(new ACE_Time_Value),
        m_serverPrepareAckWaitTime(new ACE_Time_Value),
        m_serverQuiesceAckWaitTime(new ACE_Time_Value),
        m_serverInsertTagAckWaitTime(new ACE_Time_Value),
        m_serverResumeAckWaitTime(new ACE_Time_Value),
        m_clientConnectSockMaxTime(new ACE_Time_Value),
        m_clientConnectSockAttemptFrequency(new ACE_Time_Value),
        m_clientTagGuidRecvWaitTime(new ACE_Time_Value),
        m_clientPrepareCmdWaitTime(new ACE_Time_Value),
        m_clientQuiesceCmdWaitTime(new ACE_Time_Value),
        m_clientInsertTagCmdWaitTime(new ACE_Time_Value),
        m_clientResumeCmdWaitTime(new ACE_Time_Value)
    {
        InitTimeoutMap();
        SetServerTimeouts();
        SetClientTimeouts();
    }

    void SetTimeoutsForCrashConsistency()
    {
        m_serverPrepareAckWaitTime->msec(300);
        m_serverQuiesceAckWaitTime->msec(100);
        m_serverResumeAckWaitTime->msec(100);

        m_clientQuiesceCmdWaitTime->msec(310);
        m_clientInsertTagCmdWaitTime->msec(110);
    }

    void SetTimeoutsForAppConsistency()
    {
        m_serverPrepareAckWaitTime->msec(90000);
        m_serverQuiesceAckWaitTime->msec(9000);
        m_serverResumeAckWaitTime->msec(40000);

        m_clientQuiesceCmdWaitTime->msec(90010);
        m_clientInsertTagCmdWaitTime->msec(9010);
    }

    void SetCustomTimeoutsForCrashConsistency(std::map<std::string, std::string> &timeouts)
    {
        const std::string consistencyType = "CrashConsistency";
        SetCustomTimeouts(consistencyType, timeouts);
    }

    void SetCustomTimeoutsForAppConsistency(std::map<std::string, std::string> &timeouts)
    {
        const std::string consistencyType = "AppConsistency";
        SetCustomTimeouts(consistencyType, timeouts);
    }

    void SetCustomTimeouts(const std::string &consistencyType, std::map<std::string, std::string> &timeouts)
    {
        std::map<std::string, SP_ACE_TIME_VALUE>::iterator itertv = m_mapTimeouts.begin();
        while (itertv != m_mapTimeouts.end())
        {
            std::string key = consistencyType + itertv->first;
            std::map<std::string, std::string>::iterator iterto = timeouts.find(key);
            if (iterto != timeouts.end())
                itertv->second->msec(atoi(iterto->second.c_str()));

            itertv++;
        }
    }
};

class Communicator;

class Communicator
{
public:
    Communicator(const std::string& comm_type);

    ~Communicator();

    //Socket created by the Coordinator to communicate with Master Vacp Node
    int CreateClientConnection(bool bCompatMode = false);

    int CreateServerConnection();

    void HandleRecvdData(const char *data, size_t length);

    void ProcessRecvdQ();
    
    void InsertToMsgBucket(std::string strCoordIp);

    int SendMessageToMasterNode(ENUM_MSG_TYPE msgType,std::string strData);

    int SendMessageToEachCoordinatorNode(ENUM_MSG_TYPE msgType, std::string strData);

    void SetCoordNodeCount(int nCount);
    
    std::string GetLocalSystemTime();

    int GetCoordNodeCount();

    std::string CheckForMissingSpawnVacpAck();
    void CheckForMissingPrepareAck();
    void CheckForMissingQuiesceAck();
    void CheckForMissingCommitTagAck();
    void CheckForMissingResumeAck();
    std::string ShowDistributedVacpReport();

    int WaitForMsg(ENUM_MSG_TYPE msgType);

    void StopAcceptingConnections();

    void CloseCommunicator();

    std::string GetReceivedData()
    {
        return m_data;
    }

    boost::function<size_t(const std::string&, const char *, size_t, const std::string&)> send;

    boost::function<bool(const std::string&, const std::string&)> is_client_loggedin;

    const TagTelemetryInfo & GetTagTelemetryInfo()
    {
        m_perfCounter.PrintAllElapsedTimes();
        m_tagTelInfo.Add(m_perfCounter.GetTagTelemetryInfo().GetMap());
        return m_tagTelInfo;
    }

    VacpLastError m_lastError;

private:

    void StartInboundMessageProcessing();

    int SendMessageToCoordinatorNode(const std::string &node,ENUM_MSG_TYPE msgType, std::string strData);

    int PrepareMessageToBeSent(bool bIsNewPacket,
        UINT nRequestId,
        BYTE bFuncId,
        BYTE bDirection,
        std::string strSrc,
        std::string strDest,
        std::string strData,
        stPacket& packet);

    void ShowFunctionMsg(std::string strPreMsg,
        std::string strPreposition,
        int nFunctionId,
        std::string strSourceMachine,
        std::string strTime);

    int InterpretReceivedMessage(unsigned char* pszRecvBuf, stPacket* pRecvdPkt);
    
    int ProcessPacket(stPacket* pRecvdPkt, stPacket* pSendPkt);
    
    int SendPacket(stPacket sendPkt);

    int ProcessQueueElement();


    SecureClient *m_ptrSecureClient;

    std::map<std::string, MsgBucket>m_mapIpToMsg;

    unsigned long m_commSequenceId;

    ACE_Atomic_Op<ACE_Thread_Mutex, int> nSpawnedAckCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpPrepareAckCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpQuiesceAckCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpCommitTagAckCounter;

    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpStatusAckCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpTagGuidRecvdCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpResumeAckCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> nVacpStatusInfoCounter;
    ACE_Atomic_Op<ACE_Thread_Mutex, int> m_nCoordinatorCount;

    PerfCounter m_perfCounter;

    std::string m_type;

    ///< "server" or "client"
    std::string m_mode;

    boost::mutex m_mutex;

    boost::condition_variable m_cvRecvData;

    boost::thread_group m_thread_group;

    ACE_Atomic_Op<ACE_Thread_Mutex, bool> m_bActive;

    ACE_Atomic_Op<ACE_Thread_Mutex, bool> m_bQuit;

    std::string m_recvDataToProcess;

    boost::shared_ptr<ACE_Message_Queue<ACE_MT_SYNCH> > m_recvQ;

    std::string m_data;

    ACE_Event m_spawnedAck;
    ACE_Event m_prepareAck;
    ACE_Event m_quiesceAck;

    ACE_Event m_tagCommitAck;
    ACE_Event m_resumeAck;
    ACE_Event m_statusAck;

    ACE_Event m_tagGuidCmd;
    ACE_Event m_prepareCmd;
    ACE_Event m_quiesceCmd;

    ACE_Event m_tagCommitCmd;
    ACE_Event m_resumeCmd;
    ACE_Event m_statusCmd;

    ACE_Event m_schedAck;

    CommunicatorTimeouts m_timeouts;

    TagTelemetryInfo m_tagTelInfo;
};

// shared pointer type for holding Communicator object
typedef boost::shared_ptr<Communicator> CommunicatorPtr;

class ServerCommunicator;

typedef boost::shared_ptr<ServerCommunicator> ServerCommunicatorPtr;

class ServerCommunicator
{
public:
    static ServerCommunicator* GetServerCommunicator();

    ~ServerCommunicator();

    void RequestHandler(const std::string& comm_type,
        const char *data,
        const size_t length);

    bool Initialize(const std::set<std::string>& allowed_clients,
        const std::set<std::string>& comm_types);

    bool StartCommunicator(const std::string& comm_type);

    bool StopCommunicator(const std::string& comm_type);

    CommunicatorPtr GetCommunicator(const std::string& comm_type);

    void StopAcceptingClients(const std::string& comm_type);

    VacpLastError m_lastError;

private:
    ServerCommunicator();

    int CreateServerSocket();

    void SetAllowedClients(const std::set<std::string>& allowed_clients);

    int StartServer();

    static boost::mutex  m_mutex;

    static ServerCommunicatorPtr m_pCommunicator;

    bool m_initialized;

    SecureServer *m_ptrSecureServer;

    std::set<std::string> m_allowedClients;

    boost::thread_group m_thread_group;

    boost::function<size_t(const std::string&, const char *, const size_t, const std::string&)> send;

    boost::mutex  m_commMutex;
 
    std::map<std::string, CommunicatorPtr> m_communicators;

};

class CommunicatorScheduler
{
public:
    static CommunicatorScheduler* GetCommunicatorScheduler()
    {
        if (NULL == m_pCommScheduler)
        {
            boost::mutex::scoped_lock gaurd(m_mutex);
            if (NULL == m_pCommScheduler)
                m_pCommScheduler = new (std::nothrow) CommunicatorScheduler();
        }

        return m_pCommScheduler;
    }

    ~CommunicatorScheduler();

    void SetSchedule(const std::string& commType, steady_clock::time_point time)
    {
        boost::unique_lock<boost::mutex> lock(m_schedMutex);

        m_schedules[commType] = time;
    }

    steady_clock::time_point GetSchedule(const std::string& commType)
    {
        boost::unique_lock<boost::mutex> lock(m_schedMutex);
        return m_schedules[commType];
    }

    std::string GetSchedules()
    {
        boost::unique_lock<boost::mutex> lock(m_schedMutex);
        std::stringstream ssScheduleData;
        std::map<std::string, steady_clock::time_point>::iterator schedIter = m_schedules.begin();
        while (schedIter != m_schedules.end())
        {
            uint64_t nextSchedule = 0;
            steady_clock::time_point currTime = steady_clock::now();
            if (currTime < schedIter->second)
            {
                duration<double> dur = schedIter->second - currTime;
                seconds elapsedTime = duration_cast<seconds> (dur);
                nextSchedule = elapsedTime.count();
            }
            ssScheduleData << schedIter->first << ":" << nextSchedule;
            schedIter++;
            if (schedIter != m_schedules.end())
                ssScheduleData << ";";
        }

        return ssScheduleData.str();
    }

    int SetSchedules(std::string& schedules)
    {
        boost::unique_lock<boost::mutex> lock(m_schedMutex);
        std::vector<std::string> tokens;
        boost::split(tokens, schedules, boost::is_any_of(";"), boost::token_compress_on);
        std::vector<std::string>::iterator tokenIter = tokens.begin();
        for (/*empty*/; tokenIter != tokens.end(); tokenIter++)
        {
            size_t pos = tokenIter->find_last_of(':');
            std::string type = tokenIter->substr(0, pos);
            std::string interval = tokenIter->substr(pos + 1);

            uint64_t time = 0;
            try {
                time = boost::lexical_cast<uint64_t>(interval.c_str());
                steady_clock::time_point nextSchedule = steady_clock::now() + seconds(time);
                m_schedules[type] = nextSchedule;
            }
            catch (std::exception& e)
            {
                //inm_printf("Failed to parse the interval %s for %s.\n", interval.c_str(), type.c_str());
                continue;
            }
        }

        return 0;
    }

private:
    CommunicatorScheduler(){ }

    static CommunicatorScheduler * m_pCommScheduler;

    static boost::mutex m_mutex;

    std::map<std::string, steady_clock::time_point> m_schedules;

    boost::mutex m_schedMutex;
};

#endif

