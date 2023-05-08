#include <stdafx.h>

#include <boost/lexical_cast.hpp>
#include <boost/chrono/duration.hpp>
#include "Communicator.h"
#include "inmsafecapis.h"
#include "securityutils.h"
#include "defaultdirs.h"
#include "VacpConf.h"

using namespace std;

#define NUM_CLIENT_THREADS 1
#define NUM_SERVER_THREADS 2

static const std::string CommunicatorTypeApp = "AppConsistency";
static const std::string CommunicatorTypeCrash = "CrashConsistency";

const char* DVACP_PACKET_STARTER_STRING = "VacpPacket";
const char* DVACP_PORT = "20004";
const int DVACP_SOCK_SERVER_PORT = 20004;
const int ERROR_NULL_MESSAGE_RECEIVED = 10100;
const int ERROR_INVALID_PACKET_REFERENCE = 10101;
const int ERROR_INVALID_TARGET_NODE_NAME_LENGTH = 10102;
const int ERROR_INVALID_DATA_LENGTH = 10103;
const int ERROR_CLIENT_SOCKET_CREATION_FAILED = 10104;
const int ERROR_FAILED_TO_SEND_CMD_TO_COORDINATOR_NODES = 10105;
const int ERROR_FAILED_TO_SEND_MSG_TO_MASTER_NODE = 10106;
const int ERROR_INVALID_SOCKET = 10107;
const int ERROR_SERVER_SOCKET_CREATION_FAILED = 10108;
const int ERROR_SERVER_HAS_NO_CLIENTS_TO_SERVE = 10109;

const BYTE SPAWN_VACP_CMD			  = 65;//equivalent to ASCII character 'A'
//const BYTE SPAWN_VACP_ACK			  = 66;
const BYTE PREPARE_CMD		  = 67;
const BYTE PREPARE_ACK		  = 68;
const BYTE QUIESCE_CMD		  = 69;
const BYTE QUIESCE_ACK		  = 70;
const BYTE INSERT_TAG_CMD			  = 71;
const BYTE INSERT_TAG_ACK			  = 72;
const BYTE STATUS_CMD		  = 73;
const BYTE STATUS_ACK		  = 74;
const BYTE SEND_TAG_GUID			  = 75;
const BYTE SEND_STATUS_INFO		  = 76;
const BYTE RESUME_ACK		  = 77;
const BYTE RESUME_CMD		  = 78;

const BYTE DIR_MASTER_TO_COORD = 65;//equivalent to ASCII character 'A'
const BYTE DIR_COORD_TO_MASTER = 91;//equivalent to ASCII character 'Z'

const BYTE INVALID_FUNCTION_ID = 0xFF;
const BYTE DVACP_NEW_PACKET = 65;//A
const BYTE DVACP_RESPONSE_PACKET = 91;//Z
const BYTE DVACP_PACKET_HEADER_LEN = 19;
const UINT DVACP_PACKET_MIDLEN = 14;
const UINT DVACP_PACKET_MINLEN = DVACP_PACKET_HEADER_LEN + DVACP_PACKET_MIDLEN;
const UINT DVACP_PACKET_MAXLEN = 255;
const UINT DVACP_NEW_PACKET_REQUEST_ID = 0xFFFFFFFF;

//Global data members
unsigned short g_DistributedPort = 20004;

extern void inm_printf(const char * format, ...);
extern std::string g_strMasterNodeFingerprint;

ServerCommunicatorPtr ServerCommunicator::m_pCommunicator;
boost::mutex ServerCommunicator::m_mutex;

CommunicatorScheduler* CommunicatorScheduler::m_pCommScheduler = NULL;
boost::mutex CommunicatorScheduler::m_mutex;

ServerCommunicator* ServerCommunicator::GetServerCommunicator()
{
    if (NULL == m_pCommunicator.get())
    {
        boost::mutex::scoped_lock gaurd(m_mutex);
        if (NULL == m_pCommunicator.get())
            m_pCommunicator.reset(new (std::nothrow) ServerCommunicator());
    }

    return m_pCommunicator.get();
}

ServerCommunicator::ServerCommunicator()
    :m_initialized(false),
    m_ptrSecureServer(NULL)
{
}

ServerCommunicator::~ServerCommunicator()
{
    if (m_ptrSecureServer != NULL)
    {
        m_ptrSecureServer->stop_accepting_connections();

        m_ptrSecureServer->close();

    }
    std::map<std::string, CommunicatorPtr>::iterator commIter = m_communicators.begin();
    for (/*empty*/; commIter != m_communicators.end(); commIter++)
    {
        commIter->second->CloseCommunicator();
    }

    m_thread_group.join_all();

    if (m_ptrSecureServer != NULL)
        delete m_ptrSecureServer;
    m_ptrSecureServer = NULL;
}

bool ServerCommunicator::Initialize(const std::set<std::string> &clients,
    const std::set<std::string>& comm_types)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    try {
        std::stringstream ssExpMsg;

        SetAllowedClients(clients);

        if (!CreateServerSocket())
        {
            CommunicatorPtr commPtr;
            std::set<std::string>::iterator ctIter = comm_types.begin();
            while (ctIter != comm_types.end())
            {
                if (!StartCommunicator(*ctIter))
                {
                    ssExpMsg << "Failed to start " << *ctIter << " communicator.";
                    throw std::runtime_error(ssExpMsg.str().c_str());
                }
                ctIter++;
            }

            if (StartServer())
            {
                ssExpMsg << "Failed to start server.";
                throw std::runtime_error(ssExpMsg.str().c_str());
            }

            m_initialized = true;
        }
    }
    catch (const std::exception &ex)
    {
        std::stringstream ss;
        ss << "Failed to initialize server communicator with exception: " << ex.what();
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = VACP_MULTIVM_SERVER_CREATE_FAILED;
        inm_printf("%s.\n", ss.str().c_str());
    }
    catch (...)
    {
        std::stringstream ss;
        ss << "Failed to initialize server communicator with unknown exception";
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = VACP_MULTIVM_SERVER_CREATE_FAILED;
        inm_printf("%s.\n", ss.str().c_str());
    }

    return m_initialized;
}

bool ServerCommunicator::StartCommunicator(const std::string& comm_type)
{
    try {

        boost::unique_lock<boost::mutex > lock(m_commMutex);

        std::map<std::string, CommunicatorPtr>::iterator commIter = m_communicators.find(comm_type);
        if (commIter != m_communicators.end())
        {
            inm_printf("Communicator for connection type %s already started.\n", comm_type.c_str());
            return true;
        }

        CommunicatorPtr commPtr;
        commPtr.reset(new Communicator(comm_type));

        std::set<std::string>::iterator clientsIter = m_allowedClients.begin();
        for (/*empty*/; clientsIter != m_allowedClients.end(); clientsIter++)
            commPtr->InsertToMsgBucket(*clientsIter);

        commPtr->SetCoordNodeCount(m_allowedClients.size());

        commPtr->CreateServerConnection();

        commPtr->send = boost::bind(&SecureServer::send, m_ptrSecureServer, _1, _2, _3, _4);

        commPtr->is_client_loggedin = boost::bind(&SecureServer::is_client_loggedin, m_ptrSecureServer, _1, _2);

        m_communicators.insert(std::make_pair(comm_type, commPtr));

        m_ptrSecureServer->open_connections(comm_type);

        return true;
    }
    catch (const std::exception &ex)
    {
        std::stringstream ss;
        ss << "Failed to start " << comm_type.c_str() << " communicator with exception: " << ex.what();
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = VACP_MULTIVM_SERVER_CREATE_FAILED;
        inm_printf("%s.\n", ss.str().c_str());
    }

    return false;
}

bool ServerCommunicator::StopCommunicator(const std::string& comm_type)
{
    try {
        boost::unique_lock<boost::mutex> lock(m_commMutex);
        std::map<std::string, CommunicatorPtr>::iterator commIter = m_communicators.find(comm_type);
        if (commIter == m_communicators.end())
        {
            inm_printf("Communicator for connection type %s not found.\n", comm_type.c_str());
            return true;
        }

        m_ptrSecureServer->close_connections(comm_type);

        CommunicatorPtr comPtr = commIter->second;
        m_communicators.erase(comm_type);

        comPtr->CloseCommunicator();
        comPtr.reset();

        return true;
    }
    catch (const std::exception &ex)
    {
        inm_printf("Failed to stop %s communicator with error %s.\n",
            comm_type.c_str(),
            ex.what());
    }
    catch (...)
    {
        inm_printf("Failed to stop %s communicator with unknown exception.\n",
            comm_type.c_str());
    }

    return false;
}

void ServerCommunicator::SetAllowedClients(const std::set<std::string> &clients)
{
    m_allowedClients = clients;
}

void ServerCommunicator::StopAcceptingClients(const std::string& comm_type)
{
    m_ptrSecureServer->stop_accepting_connections(comm_type);
}

void ServerCommunicator::RequestHandler(const std::string& comm_type,
    const char *data,
    const size_t length)
{
    if (!m_initialized)
        return;

    std::map<std::string, CommunicatorPtr>::iterator commIter = m_communicators.find(comm_type);
    if (commIter == m_communicators.end())
    {
        std::string errMsg = "Failed to find server communicator for connection type ";
        errMsg += comm_type;
        throw std::runtime_error(errMsg.c_str());
    }

    commIter->second->HandleRecvdData(data, length);

    return;
}

int ServerCommunicator::CreateServerSocket()
{

    if (m_allowedClients.size() == 0)
    {
        return ERROR_SERVER_HAS_NO_CLIENTS_TO_SERVE;
    }

    boost::filesystem::path certpath(securitylib::getCertDir());
    boost::filesystem::path privatekeypath(securitylib::getPrivateDir());
    boost::filesystem::path dhpath(securitylib::getPrivateDir());

    if (g_strMasterNodeFingerprint.empty())
    {
        certpath /= "ps.crt";
        privatekeypath /= "ps.key";
        dhpath /= "ps.dh";
    }
    else
    {
        certpath /= "vacp.crt";
        privatekeypath /= "vacp.key";
        dhpath /= "vacp.dh";
    }

    try
    {
        m_ptrSecureServer = new SecureServer(g_DistributedPort,
            certpath.string(),
            privatekeypath.string(),
            dhpath.string(),
            boost::bind(&ServerCommunicator::RequestHandler, this, _1, _2, _3));

        m_ptrSecureServer->set_allowed_clients(m_allowedClients);

        send = boost::bind(&SecureServer::send, m_ptrSecureServer, _1, _2, _3, _4);
    }
    catch (std::exception& e)
    {
        std::stringstream ss;
        ss << "CreateServerSocket failed with exception: " << e.what();
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = ERROR_INVALID_SOCKET;
        inm_printf("%s.\n", ss.str().c_str());

        if (m_ptrSecureServer != NULL)
            m_ptrSecureServer->close();

        if (m_ptrSecureServer != NULL)
            delete m_ptrSecureServer;

        m_ptrSecureServer = NULL;

        return ERROR_INVALID_SOCKET;
    }

    return 0;
}

int ServerCommunicator::StartServer()
{
    try {
        boost::thread *t = new boost::thread(boost::bind(&SecureServer::run, m_ptrSecureServer, NUM_SERVER_THREADS));

        m_thread_group.add_thread(t);
    }
    catch (std::exception& e)
    {
        std::stringstream ss;
        ss << "StartServer caught exception: " << e.what();
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = ERROR_INVALID_SOCKET;
        inm_printf("%s.\n", ss.str().c_str());

        if (m_ptrSecureServer != NULL)
            m_ptrSecureServer->close();

        m_thread_group.join_all();

        if (m_ptrSecureServer != NULL)
            delete m_ptrSecureServer;

        m_ptrSecureServer = NULL;

        return ERROR_INVALID_SOCKET;
    }

    return 0;
}

CommunicatorPtr ServerCommunicator::GetCommunicator(const std::string& comm_type)
{
    CommunicatorPtr commPtr;
    boost::unique_lock<boost::mutex> lock(m_commMutex);
    std::map<std::string, CommunicatorPtr>::iterator commIter = m_communicators.find(comm_type);
    if (commIter == m_communicators.end())
    {
        inm_printf("Communicator for connection type %s not found.\n", comm_type.c_str());
    }
    else
    {
        commPtr =  commIter->second;
    }

    return commPtr;
}

Communicator::Communicator(const std::string& comm_type)
    :m_type(comm_type),
    m_bActive(false),
    m_bQuit(false)
{
    m_commSequenceId = 0;
    nSpawnedAckCounter = 0;

    nVacpPrepareAckCounter = 0;

    nVacpQuiesceAckCounter = 0;
    nVacpCommitTagAckCounter = 0;
    nVacpStatusAckCounter = 0;
    nVacpTagGuidRecvdCounter = 0;

    nVacpResumeAckCounter = 0;
    nVacpStatusInfoCounter = 0;
    m_nCoordinatorCount = 0;

    m_ptrSecureClient = NULL;

    m_recvQ.reset(new ACE_Message_Queue<ACE_MT_SYNCH>);

    if (boost::iequals(m_type, CommunicatorTypeCrash))
        m_timeouts.SetTimeoutsForCrashConsistency();
    else
        m_timeouts.SetTimeoutsForAppConsistency();

    VacpConfigPtr pVacpConf = VacpConfig::getInstance();
    if (pVacpConf.get() != NULL)
    {
        if (boost::iequals(m_type, CommunicatorTypeCrash))
            m_timeouts.SetCustomTimeoutsForCrashConsistency(pVacpConf->getParams());
        else
            m_timeouts.SetCustomTimeoutsForAppConsistency(pVacpConf->getParams());
    }

}

Communicator::~Communicator()
{
    inm_printf("%s: Entered\n", __FUNCTION__);
    m_recvQ.reset();
}

int Communicator::ProcessQueueElement()
{
    std::string strTemp;
    int nLen = 0;
    int nIndex = 0;
    int nResult = 0;
    DWORD dwResult = 0;
    BYTE* pBytes = NULL;
    stPacket recvdPkt;
    stPacket sendPkt;
    std::string strElement;
    strElement.clear();
    strTemp.clear();

    const BYTE DVACP_PACKET_STARTER_STRING_LEN = (BYTE) strlen(DVACP_PACKET_STARTER_STRING);

    if (!m_recvDataToProcess.size())
    {
        inm_printf("There is no data in the received Queue.\n");
        return nResult;
    }

    strElement = m_recvDataToProcess;

    if ((strElement.size() >= (DVACP_PACKET_STARTER_STRING_LEN + sizeof(UINT32))) &&
        ((nIndex = strElement.find(DVACP_PACKET_STARTER_STRING)) != strElement.npos))
    {
        m_recvDataToProcess.clear();

        nIndex += DVACP_PACKET_STARTER_STRING_LEN;

        strTemp = strElement.substr(nIndex, sizeof(UINT32));//Read the total length of the packet
        pBytes = recvdPkt.ReadBytes((BYTE*)strTemp.c_str(), 0, sizeof(UINT32));
        recvdPkt.totalPacketLen = (UINT32)(recvdPkt.ConvertToNative(pBytes, ENUM_UNSIGNED_INT));
        delete [] pBytes;
        pBytes = NULL;

        if ((recvdPkt.totalPacketLen < DVACP_PACKET_MINLEN) ||
        (recvdPkt.totalPacketLen > DVACP_PACKET_MAXLEN))
        {
            DebugPrintf("\nIgnoring received data that doesn't match typical vacp packet size. received size = %d.\n", recvdPkt.totalPacketLen);
            return nResult;
        }

        nIndex += sizeof(UINT32);
        nLen = recvdPkt.totalPacketLen - nIndex;//because VacpHeader and total length fields are already read
        strTemp.clear();

        //the Q element contains data equals to or more than one exact recv packet from vacp coordinator
        if (strElement.length() >= recvdPkt.totalPacketLen)
        {
            strTemp = strElement.substr(nIndex, nLen);
            if(strTemp.length() == nLen)
            {
                nResult = InterpretReceivedMessage((unsigned char*)strTemp.c_str(),&recvdPkt);
                if(0 == nResult)
                {
                    nResult = ProcessPacket(&recvdPkt,&sendPkt);
                    if(0 != nResult)
                    {
                        DebugPrintf("\nFailed to process the packet.\n");
                    }
                }
            }

            if (strElement.length() > recvdPkt.totalPacketLen)
            {
                strTemp.clear();
                strTemp = strElement.substr(nIndex + nLen, string::npos);
                strElement.clear();
                strElement = strTemp;
                m_recvDataToProcess = strElement;
                ProcessQueueElement();
            }
        }
        else if (strElement.length() < recvdPkt.totalPacketLen)
        {
            //the Q element does not even contain one exact recv packet from vacp coordinator
            strTemp.clear();
            strTemp += strElement;
            do 
            {
                strElement.clear();
                ACE_Message_Block *mb = NULL;
                ACE_Time_Value wait = ACE_OS::gettimeofday();
                wait += ACE_Time_Value(0, 10 * 1000);
                m_recvQ->dequeue_head(mb, &wait);
                if (mb != NULL)
                {
                    char *msgData = mb->base();
                    size_t size = mb->size();
                    strElement = string(msgData, size);
                    strTemp += strElement;
                    delete[] msgData;
                    delete mb;
                }

                if (m_bQuit.value())
                    return nResult;

            } while (strTemp.length() < recvdPkt.totalPacketLen);

            m_recvDataToProcess = strTemp;
            nResult = ProcessQueueElement();
        }
    }
    else
    {
        inm_printf("Invalid messsage in the receive queue.\n");
    }

    return nResult;
}

void Communicator::ProcessRecvdQ()
{
    int dwResult = 0;
    std::string strElement;
    strElement.clear();
    while (!m_bQuit.value())
    {
        ACE_Message_Block *mb = NULL;
        ACE_Time_Value wait = ACE_OS::gettimeofday();
        wait += ACE_Time_Value(0, 10 * 1000);

        m_recvQ->dequeue_head(mb, &wait);
        if (mb != NULL)
        {
            char *msgData = mb->base();
            size_t size = mb->size();
            strElement = string(msgData, size);
            delete[] msgData;
            delete mb;
            m_recvDataToProcess += strElement;
            dwResult = ProcessQueueElement();
        }
    }

    while (1)
    {
        ACE_Message_Block *mb = NULL;
        ACE_Time_Value wait = ACE_OS::gettimeofday();
        wait += ACE_Time_Value(0, 10 * 1000);

        m_recvQ->dequeue_head(mb, &wait);
        if (mb != NULL)
        {
            char *msgData = mb->base();
            size_t size = mb->size();
            strElement = string(msgData, size);
            delete[] msgData;
            delete mb;
            m_recvDataToProcess += strElement;
            dwResult = ProcessQueueElement();
        }
        else
        {
            break;
        }
    }

    inm_printf("\nExited: ProcessRecvdQ()\n");

    return;

}


void Communicator::SetCoordNodeCount(int nCount)
{
  m_nCoordinatorCount = nCount;
}

/// \brief callback function that the transport layer calls when data is received
void Communicator::HandleRecvdData(const char *data, size_t length)
{
    if (!m_bActive.value())
    {
        inm_printf("\nHandleRecvdData: %s %s communicator is not active.\n", m_type.c_str(), m_mode.c_str());
        return;
    }

    char * msgData = new (std::nothrow) char[length];

    if (msgData == NULL)
    {
        inm_printf("\nError: HandleRecvdData() - failed to allocate %d bytes.\n", length);
        return;
    }

    inm_memcpy_s((void *)msgData, length, data, length);

    ACE_Message_Block *mb = NULL;
    mb = new (std::nothrow) ACE_Message_Block(msgData, length);
    if (mb == NULL)
    {
        inm_printf("\nError: HandleRecvdData() - failed to allocate message block of length %d.\n", length);
        return;
    }

    m_recvQ->enqueue_prio(mb);

    XDebugPrintf("\nrecv size = %d\n", length);

    return;

}

int Communicator::CreateClientConnection(bool bCompatMode)
{
    try
    {
        m_bQuit = false;

        std::string port = boost::lexical_cast<std::string>(g_DistributedPort);

        StartInboundMessageProcessing();

        inm_printf("CreateClientConnection: g_strLocalIpAddress=%s, g_strMasterHostName=%s.\n",
            g_strLocalIpAddress.c_str(), g_strMasterHostName.c_str());

        m_ptrSecureClient = new SecureClient(g_strLocalIpAddress,
                            boost::bind(&Communicator::HandleRecvdData, this, _1, _2));


        boost::system::error_code ec = m_ptrSecureClient->connect(g_strMasterHostName, port);
        if (ec)
        {
            inm_printf("connection to master node failed :%d %s\n", ec.value(), ec.message().c_str());
            throw std::exception();
        }
        else
        {
            send = boost::bind(&SecureClient::send, m_ptrSecureClient, _1, _2, _3, _4);
            boost::thread *t = new boost::thread(boost::bind(&SecureClient::run, m_ptrSecureClient, NUM_CLIENT_THREADS));
            m_thread_group.add_thread(t);
            m_ptrSecureClient->login(bCompatMode? std::string() : m_type);
            m_mode = "Client";
            m_bActive = true;
        }
    }
    catch (std::exception& e)
    {
        std::stringstream ss;
        ss << "CreateClientConnection failed with exception: " << e.what();
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = ERROR_INVALID_SOCKET;
        inm_printf("%s.\n", ss.str().c_str());

        CloseCommunicator();

        return ERROR_INVALID_SOCKET;
    }
    catch (...)
    {
        std::stringstream ss;
        ss << "CreateClientConnection failed with unknown exception";
        m_lastError.m_errMsg = ss.str();
        m_lastError.m_errorCode = ERROR_INVALID_SOCKET;
        inm_printf("%s.\n", ss.str().c_str());

        CloseCommunicator();

        return ERROR_INVALID_SOCKET;
    }

    return 0;
}

int Communicator::CreateServerConnection()
{
    StartInboundMessageProcessing();
    m_mode = "Server";
    m_bActive = true;

    return 0;
}

void Communicator::StartInboundMessageProcessing()
{
    boost::thread *t = new boost::thread(boost::bind(&Communicator::ProcessRecvdQ, this));
    m_thread_group.add_thread(t);
}

void Communicator::StopAcceptingConnections()
{
}

int Communicator::PrepareMessageToBeSent(bool bIsNewPacket,
										 UINT nRequestId,
										 BYTE bFuncId,
										 BYTE bDirection,
										 std::string strSrc,
										 std::string strDest,
										 std::string strData,
										 stPacket& packet )
{
  int nRet = 0;
  packet.strPacketStarter = std::string("VacpPacket");
  if(bIsNewPacket)
  {
	packet.packetType = DVACP_NEW_PACKET;//A or 65
	packet.requestId = DVACP_NEW_PACKET_REQUEST_ID;
  }
  else
  {
	packet.packetType = DVACP_RESPONSE_PACKET;//Z or 91
	packet.requestId = nRequestId;
  }
  
  packet.socketId = 0;//4bytes
  packet.commSequenceId = m_commSequenceId++;//4bytes
  packet.direction = bDirection;//1byte
  packet.FunctionId = bFuncId;//1byte
  packet.srcNodeLen = (BYTE)strSrc.length();//1byte
  packet.strSrcNode = strSrc;//string
  packet.destNodeLen = (BYTE)strDest.length();//1byte
  packet.strDestNode = strDest;//string
  packet.dataLen = (USHORT)(strData.length());//2bytes
  packet.strData = strData;//string

  packet.totalPacketLen = DVACP_PACKET_HEADER_LEN + 
						  DVACP_PACKET_MIDLEN + 
						  packet.strSrcNode.length() + 
						  packet.strDestNode.length() + 
						  packet.strData.length();
  return nRet;
}
int Communicator::InterpretReceivedMessage(unsigned char* pszRecvBuf,stPacket* pRecvdPkt)
{
  XDebugPrintf("\nEnter InterpretReceivedMessage() at %s\n", GetLocalSystemTime().c_str());
  int nRet = 0;
  int nCurrentPos = 0;
  do
  {
	if(NULL == pszRecvBuf)
	{
	  nRet = ERROR_NULL_MESSAGE_RECEIVED;
	  break;
	}
	else if(!pRecvdPkt)
	{
	  nRet = ERROR_INVALID_PACKET_REFERENCE;
	}
	else
	{
	  //Read Packet Type 1 byte
	  BYTE* pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(BYTE));
	  pRecvdPkt->packetType = (BYTE)*pBytes;
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(BYTE);

	  //Read Request Id 4 bytes
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(UINT32));
	  pRecvdPkt->requestId = (UINT)(pRecvdPkt->ConvertToNative(pBytes,ENUM_UNSIGNED_INT));
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(UINT32);

	  //Read SocketID
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(UINT32));
	  pRecvdPkt->socketId = (UINT32)(pRecvdPkt->ConvertToNative(pBytes,ENUM_UNSIGNED_INT));
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(UINT32);

	  //Read Communication Sequence ID
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(UINT32));
	  pRecvdPkt->commSequenceId = (UINT)(pRecvdPkt->ConvertToNative((pBytes),ENUM_UNSIGNED_INT));
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(UINT32);

	  //Read Direction of communication; 0=Master to Co-ordinatro Node; 1= Co-ordinator to Master Node
	  pBytes =pRecvdPkt->ReadBytes((pszRecvBuf),nCurrentPos,sizeof(BYTE));
	  pRecvdPkt->direction = (BYTE)*pBytes;
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(BYTE);

	  //Read Function Id
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(BYTE));
	  pRecvdPkt->FunctionId = (BYTE)*pBytes;
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(BYTE);

	  //Read source node length
	  pBytes = pRecvdPkt->ReadBytes((pszRecvBuf),nCurrentPos,sizeof(BYTE));
	  pRecvdPkt->srcNodeLen = (BYTE)*pBytes;
	  delete [] pBytes;
	  pBytes = NULL;
	  nCurrentPos+=sizeof(BYTE);

	  //Read Source Node Name
	  pBytes =(pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,pRecvdPkt->srcNodeLen));
	  pRecvdPkt->strSrcNode = pRecvdPkt->ConvertToString(pBytes,pRecvdPkt->srcNodeLen);
	  ShowFunctionMsg(std::string("RECEIVED"),std::string("FROM"),pRecvdPkt->FunctionId,pRecvdPkt->strSrcNode,GetLocalSystemTime());
	  nCurrentPos+=pRecvdPkt->srcNodeLen;
	  delete [] pBytes;
	  pBytes = NULL;

	  //Read Destination Node Name Len
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(BYTE));
	  pRecvdPkt->destNodeLen = (BYTE)*pBytes;
	  if(0xFFFE == pRecvdPkt->destNodeLen)
	  {
		nRet = ERROR_INVALID_TARGET_NODE_NAME_LENGTH;
		delete [] pBytes;
		break;
	  }
	  nCurrentPos+=sizeof(BYTE);
	  delete [] pBytes;
	  pBytes = NULL;

	  //Read Target Node Name
	  pBytes =pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,pRecvdPkt->destNodeLen);
	  pRecvdPkt->strDestNode = pRecvdPkt->ConvertToString(pBytes,pRecvdPkt->destNodeLen);
	  nCurrentPos+=pRecvdPkt->destNodeLen;
	  delete [] pBytes;
	  pBytes = NULL;

	  //Read Data Length
	  pBytes = pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,sizeof(UINT16));
	  pRecvdPkt->dataLen = (USHORT)(pRecvdPkt->ConvertToNative((pBytes),ENUM_UNSIGNED_SHORT));;
	  if(0xFFFE == pRecvdPkt->dataLen)
	  {
		nRet = ERROR_INVALID_DATA_LENGTH;
		delete [] pBytes;
		break;
	  }
	  nCurrentPos+=sizeof(UINT16);
	  delete [] pBytes;
	  pBytes = NULL;

	  //Read Data
	  pBytes =pRecvdPkt->ReadBytes(pszRecvBuf,nCurrentPos,pRecvdPkt->dataLen);
	  pRecvdPkt->strData = pRecvdPkt->ConvertToString(pBytes,pRecvdPkt->dataLen);
	  delete [] pBytes;
	  pBytes = NULL;
	}
  }while(false);

  XDebugPrintf("\nExit InterpretReceivedMessage() at %s\n", GetLocalSystemTime().c_str());
  return nRet;
}

int Communicator::GetCoordNodeCount()
{
  return m_nCoordinatorCount.value();
}

int Communicator::WaitForMsg(ENUM_MSG_TYPE msgType)
{
    switch (msgType)
    {
        case ENUM_MSG_SPAWN_ACK:
            return m_spawnedAck.wait(m_timeouts.m_serverSpawnAckWaitTime.get(), 0);
        case ENUM_MSG_PREPARE_ACK:
            return m_prepareAck.wait(m_timeouts.m_serverPrepareAckWaitTime.get(), 0);
        case ENUM_MSG_QUIESCE_ACK:
            return m_quiesceAck.wait(m_timeouts.m_serverQuiesceAckWaitTime.get(), 0);
        case ENUM_MSG_TAG_COMMIT_ACK:
            return m_tagCommitAck.wait(m_timeouts.m_serverInsertTagAckWaitTime.get(), 0);
        case ENUM_MSG_RESUME_ACK:
            return m_resumeAck.wait(m_timeouts.m_serverResumeAckWaitTime.get(), 0);
        case ENUM_MSG_SEND_TAG_GUID:
            return m_tagGuidCmd.wait(m_timeouts.m_clientTagGuidRecvWaitTime.get(), 0);
        case ENUM_MSG_PREPARE_CMD:
            return m_prepareCmd.wait(m_timeouts.m_clientPrepareCmdWaitTime.get(), 0);
        case ENUM_MSG_QUIESCE_CMD:
            return m_quiesceCmd.wait(m_timeouts.m_clientQuiesceCmdWaitTime.get(), 0);
        case ENUM_MSG_TAG_COMMIT_CMD:
            return m_tagCommitCmd.wait(m_timeouts.m_clientInsertTagCmdWaitTime.get(), 0);
        case ENUM_MSG_RESUME_CMD:
            return m_resumeCmd.wait(m_timeouts.m_clientResumeCmdWaitTime.get(), 0);
        case ENUM_MSG_SCHEDULE_ACK:
            return m_schedAck.wait(m_timeouts.m_clientTagGuidRecvWaitTime.get(), 0);
        default:
            return -1;
    }

    return -1;
}

int Communicator::ProcessPacket(stPacket* pRecvdPkt, stPacket* pSendPkt)
{
  int nRet = 0;
  
  XDebugPrintf("\nEnter ProcessPacket() at %s\n", GetLocalSystemTime().c_str());
  do
  {
	if(!pRecvdPkt)
	{
	  nRet = ERROR_INVALID_PACKET_REFERENCE;
	  break;
	}
	switch(pRecvdPkt->FunctionId)
	{
	  case ENUM_MSG_SPAWN_ACK:
	  {
		//Received acknowledgement that Vacp is spawned from 
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{

			if ((*iter).second.bConnected == true)
			{
				DebugPrintf("\nClient node %s already connected.\n", pRecvdPkt->strSrcNode.c_str());
				break;
			}

			(*iter).second.bConnected = true;
		    (*iter).second.bSpawnVacpAck = true;
		  
            nSpawnedAckCounter++;

		    if (nSpawnedAckCounter.value() >= m_nCoordinatorCount.value())
		    {
                m_spawnedAck.signal();
		    }
		}
		else
		{
			DebugPrintf("Received Spawn Vacp Ack from an unlisted Client IP: %s\n", pRecvdPkt->strSrcNode.c_str());
		}

		break;
	  }
	  case PREPARE_CMD:
	  {
        m_prepareCmd.signal();
		break;
	  }
	  case PREPARE_ACK:
	  {
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{
		  (*iter).second.bPrepareAck = true;

		  nVacpPrepareAckCounter++;

		  if (nVacpPrepareAckCounter.value() >= m_nCoordinatorCount.value())
		  {
              m_prepareAck.signal();
		  }

		}
		else
		{
			DebugPrintf("Error:Received PREPARE_ACK from an unlisted Client IP: %s\n", pRecvdPkt->strSrcNode.c_str());
		}
		break;
	  }
	  case QUIESCE_CMD://4		SendQuiesceCmd 		0
	  {
        m_quiesceCmd.signal();
		break;
	  }
	  case QUIESCE_ACK://5		SendQuiesceAck 		0
	  {
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{
		  (*iter).second.bQuiesceAck = true;

		  nVacpQuiesceAckCounter++;

		  if ((nVacpQuiesceAckCounter.value() + nVacpStatusInfoCounter.value()) >= m_nCoordinatorCount.value())
		  {
			  //reset the status info counter for next phase
			  nVacpStatusInfoCounter = 0;

              m_quiesceAck.signal();
		  }
		}
		else
		{
			DebugPrintf("Error:Received QUIESCE_ACK from an unlisted Client IP: %s\n", pRecvdPkt->strSrcNode.c_str());
		}
		break;
	  }
	  case INSERT_TAG_CMD:
	  {
        m_tagCommitCmd.signal();
		break;
	  }
	  case INSERT_TAG_ACK:
	  {
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{
		  (*iter).second.bCommitTagAck = true;

		  nVacpCommitTagAckCounter++;

		  if ((nVacpCommitTagAckCounter.value() + nVacpStatusInfoCounter.value()) >= m_nCoordinatorCount.value())
		  {
			  //reset the status info counter for next phase
			  nVacpStatusInfoCounter = 0;

              m_tagCommitAck.signal();
		  }

		}
		else
		{
			DebugPrintf("Error:Received INSERT_TAG_ACK from an unlisted Client IP: %s\n", pRecvdPkt->strSrcNode.c_str());
		}

		break;
	  }
	  case STATUS_CMD:
	  {
        m_statusCmd.signal();
		break;
	  }
	  case STATUS_ACK:
	  {
		nVacpStatusAckCounter++;

		if(nVacpStatusAckCounter.value() >= m_nCoordinatorCount.value())
		{
          m_statusAck.signal();
		}
		
		break;
	  }
	  case SEND_TAG_GUID:
	  {
		//Display the data received  along with setting the required event
		DebugPrintf("\nTagGuid received from %s is %s\n",pRecvdPkt->strSrcNode.c_str(),pRecvdPkt->strData.c_str());
		m_data = pRecvdPkt->strData;
        m_tagGuidCmd.signal();
		break;
	  }
	  case SEND_STATUS_INFO:
	  {
          // Add cn status info in telemetry info
          std::stringstream ssCNStatusKey;
          std::stringstream ssCNStatus;
          ssCNStatusKey << "STATUS_INFO from " << pRecvdPkt->strSrcNode.c_str();
          ssCNStatus << "SEND_STATUS_INFO received from " << pRecvdPkt->strSrcNode.c_str() << " is " << pRecvdPkt->strData.c_str() << " at time: " << GetLocalSystemTime().c_str() << "\n";
          if (!m_type.compare(CommunicatorTypeCrash))
          {
              m_tagTelInfo.Add(ssCNStatusKey.str(), ssCNStatus.str());
          }
          else
          {
              m_tagTelInfo.Add(ssCNStatusKey.str(), ssCNStatus.str());
          }
          DebugPrintf("\n%s\n", ssCNStatus.str().c_str());
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{
		  (*iter).second.bStatusInfo = true;

		  nVacpStatusInfoCounter++;

		}
		else
		{
			DebugPrintf("Error:Received SEND_STATUS_INFO from a strange Client IP: %s\n", pRecvdPkt->strSrcNode.c_str());
		}
		break;
	  }
	  case RESUME_CMD:
	  {
        m_resumeCmd.signal();
		break;
	  }
	  case RESUME_ACK:
	  {
		
		std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.find(pRecvdPkt->strSrcNode);
		if(m_mapIpToMsg.end() != iter)
		{
		  (*iter).second.bResumeAck = true;

		  nVacpResumeAckCounter++;

		  if (nVacpResumeAckCounter.value() >= m_nCoordinatorCount.value())
		  {
              m_resumeAck.signal();
		  }

		}
		else
		{
			DebugPrintf("Received Spawn Vacp Ack from an unlisted Client IP: %s\n" , pRecvdPkt->strSrcNode.c_str());
		}
		
	    break;
	  }
      case ENUM_MSG_SCHEDULE_CMD:
      {
          if (!boost::iequals(m_type, "Scheduler"))
          {
              inm_printf("Ignoring received scheduler request from %s with data %s on %s %s.\n",
                  pRecvdPkt->strSrcNode.c_str(),
                  pRecvdPkt->strData.c_str(),
                  m_type.c_str(),
                  m_mode.c_str());
              break;
          }

          //if it is client sending the request, ignore
          if (boost::iequals(m_mode, "Client"))
          {
              break;
          }

          CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();
          std::string scheduleData = pCommSched->GetSchedules();

          DebugPrintf("\nSched sent to %s is %s\n",
              pRecvdPkt->strSrcNode.c_str(), scheduleData.c_str());

          SendMessageToCoordinatorNode(pRecvdPkt->strSrcNode, ENUM_MSG_SCHEDULE_ACK, scheduleData);
          break;
      }
      case ENUM_MSG_SCHEDULE_ACK:
      {
          //if it is server sending the ack, ignore
          if (boost::iequals(m_mode, "Server"))
          {
              break;
          }

          if (!boost::iequals(m_type, "Scheduler"))
          {
              inm_printf("Ignoring received scheduler ack from %s with data %s on %s %s.\n",
                  pRecvdPkt->strSrcNode.c_str(),
                  pRecvdPkt->strData.c_str(),
                  m_type.c_str(),
                  m_mode.c_str());
              break;
          }

          DebugPrintf("\nSched received at %s is %s\n",
              pRecvdPkt->strSrcNode.c_str(), pRecvdPkt->strData.c_str());

          try {
              CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();
              pCommSched->SetSchedules(pRecvdPkt->strData);
              m_schedAck.signal();
          }
          catch (std::exception& ex)
          {
              inm_printf("Failed to process received schedules. Error %s\n", ex.what());
          }

          break;
      }
	  default:
	  {
		DebugPrintf("\nUnknown Function Id= %d received from %s with data = %s\n",pRecvdPkt->FunctionId,pRecvdPkt->strSrcNode.c_str(),pRecvdPkt->strData.c_str());
		break;
	  }
	}
  }while(false);

  XDebugPrintf("\nExit ProcessPacket() at %s\n", GetLocalSystemTime().c_str());
  return nRet;
}
void Communicator::ShowFunctionMsg(std::string strPreMsg,std::string strPreposition,int nFunctionId,std::string strSourceMachine,std::string strTime)
{
  switch(nFunctionId)
  {
	case SPAWN_VACP_CMD:
	{
	  DebugPrintf("\n%s - %s :SPAWN_VACP_CMD %s %s: at Time: %s\n",m_type.c_str(), strPreMsg.c_str(),strPreposition.c_str(),strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case ENUM_MSG_SPAWN_ACK:
	{
        DebugPrintf("\n%s %s SPAWN_VACP_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case PREPARE_CMD:
	{
        DebugPrintf("\n%s %s :PREPARE_CMD %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.StartCounter("PreparePhase");
	  break;
	}
	case PREPARE_ACK:
	{
        DebugPrintf("\n%s %s :PREPARE_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.EndCounter("PreparePhase");
	  break;
	}
	case QUIESCE_CMD:
	{
	  DebugPrintf("\n %s :QUIESCE_CMD %s %s:  at Time: %s\n",strPreMsg.c_str(),strPreposition.c_str(),strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.StartCounter("QuiescePhase");
	  break;
	}
	case QUIESCE_ACK:
	{
	  DebugPrintf("\n %s :QUIESCE_ACK %s %s: at Time: %s\n",strPreMsg.c_str(),strPreposition.c_str(),strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.EndCounter("QuiescePhase");
	  break;
	}
	case INSERT_TAG_CMD:
	{
        DebugPrintf("\n%s %s :INSERT_TAG_CMD %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.StartCounter("InsertTagPhase");
	  break;
	}
	case INSERT_TAG_ACK:
	{
        DebugPrintf("\n%s %s :INSERT_TAG_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.EndCounter("InsertTagPhase");
	  break;
	}
	case STATUS_CMD:
	{
        inm_printf("\n%s %s STATUS_CMD %s %s:  at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case STATUS_ACK:
	{
        inm_printf("\n%s %s :STATUS_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case SEND_TAG_GUID:
	{
        DebugPrintf("\n%s %s :SEND_TAG_GUID %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case SEND_STATUS_INFO:
	{
        DebugPrintf("\n%s %s :SEND_STATUS_INFO %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
	case RESUME_ACK:
	{
        DebugPrintf("\n%s %s :RESUME_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.EndCounter("ResumePhase");
	  break;
	}
	case RESUME_CMD:
	{
        DebugPrintf("\n%s %s :RESUME_CMD %s %s: at Time: %s\n", m_type.c_str(),strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  m_perfCounter.StartCounter("ResumePhase");
	  break;
	}
    case ENUM_MSG_SCHEDULE_CMD:
    {
        DebugPrintf("\n%s %s :SCHEDULE_CMD %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
        m_perfCounter.StartCounter("SchedulePhase");
        break;
    }
    case ENUM_MSG_SCHEDULE_ACK:
    {
        DebugPrintf("\n%s %s :SCHEDULE_ACK %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
        m_perfCounter.EndCounter("SchedulePhase");
        break;
    }
	default:
	{
        DebugPrintf("\n%s %s: UNKNOWN %s %s: at Time: %s\n", m_type.c_str(), strPreMsg.c_str(), strPreposition.c_str(), strSourceMachine.c_str(), strTime.c_str());
	  break;
	}
  }
}

int Communicator::SendPacket(stPacket sendPkt)
{
  int nRet = 0;
  std::string strBuffer,strTemp;
  strTemp.clear();
  strBuffer.clear();
  //Assign Packet Starter (Header)
  strBuffer += sendPkt.strPacketStarter;

  unsigned char ctemp[4];

  //Assign Total Packet Length
  sendPkt.ConvertToNetworkByteOrder(sendPkt.totalPacketLen, ENUM_UNSIGNED_INT, ctemp);
  strTemp.assign((const char*)ctemp, sizeof(UINT32));
  strBuffer += strTemp;
  strTemp.clear();
  
  //Assign Packet Type A means, new Packet, Z means a response packet.
  strTemp.assign((const char*)&(sendPkt.packetType),sizeof(char));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Request Id
  sendPkt.ConvertToNetworkByteOrder(sendPkt.requestId, ENUM_UNSIGNED_INT, ctemp);
  strTemp.assign((const char*)ctemp, sizeof(UINT32));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign SockId
  sendPkt.ConvertToNetworkByteOrder(sendPkt.socketId, ENUM_UNSIGNED_INT, ctemp);
  strTemp.assign((const char*)ctemp, sizeof(UINT32));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Communication Id
  sendPkt.ConvertToNetworkByteOrder(sendPkt.commSequenceId, ENUM_UNSIGNED_INT, ctemp);
  strTemp.assign((const char*)ctemp, sizeof(UINT32));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Direction Master to Communictor = 65 or A; Communicator to Master = 91 or Z
  strTemp.assign((const char*)&(sendPkt.direction),sizeof(char));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Function Id
  strTemp.assign((const char*)&(sendPkt.FunctionId),sizeof(char));
  strBuffer += strTemp;
  strTemp.clear();
  //DebugPrintf("\nFunctionId being sent = %d\n",sendPkt.FunctionId);
  //ShowFunctionMsg(sendPkt.FunctionId,std::string("SENT"));

  //Assign Source Node length
  strTemp.assign((const char*)&(sendPkt.srcNodeLen),sizeof(char));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Source Node Name
  strBuffer += sendPkt.strSrcNode;

  //Assign Destination Node length
  strTemp.assign((const char*)&(sendPkt.destNodeLen),sizeof(char));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Destination Node Name
  strBuffer += sendPkt.strDestNode;

  //Assign Data length
  sendPkt.ConvertToNetworkByteOrder(sendPkt.dataLen, ENUM_UNSIGNED_SHORT, ctemp);
  strTemp.assign((const char*)ctemp, sizeof(UINT16));
  strBuffer += strTemp;
  strTemp.clear();

  //Assign Data
  strBuffer += sendPkt.strData;
  //DebugPrintf("\nMessage being sent = %s\n",strBuffer.c_str());

  //Send the message over the socket
  //Get the socket id on which this message is received by mapping the source machine with the socket id
  //SOCKET s = (*(m_mapCoordinatorNodeAndSocket.find(sendPkt.strDestNode))).second;
  //if(INVALID_SOCKET != s)
  //{
  nRet = send(sendPkt.strDestNode, strBuffer.c_str(), strBuffer.length(), m_type);
	if(-1 == nRet)
	{
	  DebugPrintf("\nSend Failed at Time: %s.\n",GetLocalSystemTime().c_str());
	  //closesocket(m_clientSocket);//MCHECK
	  return nRet;
	}
	else//MCHECK:If this block is needed or not
	{
	  //DebugPrintf("\n %d Bytes Sent by %s\n",nRet,g_strLocalHostName.c_str());
	  ShowFunctionMsg(std::string("SENT"),std::string("TO"), sendPkt.FunctionId,sendPkt.strDestNode,GetLocalSystemTime());
	  nRet = 0;
	}
  return nRet;
}


void Communicator::InsertToMsgBucket(std::string strCoordIp)
{
    MsgBucket msgs;
    m_mapIpToMsg.insert(std::make_pair(strCoordIp,msgs));
}

void Communicator::CloseCommunicator()
{
    XDebugPrintf("\nEntered CloseCommunicator() at %s\n",GetLocalSystemTime().c_str());
    int nResult = 0;
    m_bQuit = true;

    if (m_ptrSecureClient != NULL)
    {
        m_ptrSecureClient->close();
    }

    m_thread_group.join_all();

    if (m_ptrSecureClient != NULL)
        delete m_ptrSecureClient;

    m_ptrSecureClient = NULL;

    if (m_bActive.value())
        m_perfCounter.PrintAllElapsedTimes();

    m_perfCounter.Clear();

    m_tagTelInfo.Clear();

    m_bActive = false;

    XDebugPrintf("\nExited CloseCommunicator() at %s\n",GetLocalSystemTime().c_str());
}

std::string Communicator::ShowDistributedVacpReport()
{
  bool bPrinted = false;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if((*iter).second.bResumeAck == true)
	{
	  if(!bPrinted)
	  {
		inm_printf("\nA Distributed tag is issued successfully on the following nodes...\n");
		bPrinted = true;
	  }
	  inm_printf("%s\n",(*iter).first.c_str());
	}
	iter++;
  }
  
  iter = m_mapIpToMsg.begin();
  bPrinted = false;
  while(iter != m_mapIpToMsg.end())
  {
	if(((*iter).second.bCommitTagAck == true) && 
	  ((*iter).second.bResumeAck == false))
	{
	  if(!bPrinted)
	  {
		inm_printf("\n The following nodes may have issued a Distributed tag or a Local tag...\n");
		bPrinted = true;
	  }
	  inm_printf("%s\n",(*iter).first.c_str());
	}
	 iter++;
  }
  iter = m_mapIpToMsg.begin();
  bPrinted = false;
  
  std::stringstream ssExcludedNodes;
  while(iter != m_mapIpToMsg.end())
  {
	if((*iter).second.bSpawnVacpAck == true &&
       (*iter).second.bGotExcluded == true &&
	   (*iter).second.bCommitTagAck == false)
	{
	  if(!bPrinted)
	  {
		inm_printf("\nThe following nodes were EXCLUDED from Distributed Consistency Tag operation...\n");
		bPrinted = true;
	  }
      inm_printf("%s\n", (*iter).first.c_str());
      ssExcludedNodes << (*iter).first + " ";
	}
	iter++;
  }
  return ssExcludedNodes.str();
}

std::string Communicator::CheckForMissingSpawnVacpAck()
{
  bool bMissed = false;
  std::stringstream ssErr;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if((*iter).second.bSpawnVacpAck == false)
	{
		if(!bMissed)
		{
            DebugPrintf("Did not get the SPAWN_VACP_ACK from the following clients : ");
		}
		
		bMissed = true;
		(*iter).second.bGotExcluded = true;
		m_nCoordinatorCount--;
        ssErr << (*iter).first + " ";
	}
	iter++;
  }
  if(bMissed)
  {
	if(nSpawnedAckCounter.value() >= m_nCoordinatorCount.value())
	{
        m_spawnedAck.signal();
	}
  }

  if (!ssErr.str().empty())
  {
      DebugPrintf("%s\n",ssErr.str().c_str());
  }
  return ssErr.str();
}

void Communicator::CheckForMissingPrepareAck()
{
  bool bMissed = false;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if(((*iter).second.bPrepareAck == false) &&
	  ((*iter).second.bGotExcluded == false))
	{
	  if(!bMissed)
	  {
		inm_printf("\nDid not get the PREPARE_ACK from the following clients...\n");
	  }
	  
	  bMissed = true;
	  (*iter).second.bGotExcluded = true;

      inm_printf("\n %s;", (*iter).first.c_str());
	}
	iter++;
  }
}

void Communicator::CheckForMissingQuiesceAck()
{
  bool bMissed = false;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if(((*iter).second.bQuiesceAck == false) &&
	  ((*iter).second.bGotExcluded == false))
	{
	  if(!bMissed)
	  {
		DebugPrintf("\nDid not get the QUIESCE_ACK from the following clients...\n");
	  }

	  bMissed = true;
	  (*iter).second.bGotExcluded = true;
	  DebugPrintf("%s ", (*iter).first.c_str());
	}
	iter++;
  }
  DebugPrintf("\n");

  //reset the status info counter for next phase
  nVacpStatusInfoCounter = 0;
}
void Communicator::CheckForMissingCommitTagAck()
{
  bool bMissed = false;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if(((*iter).second.bCommitTagAck == false) &&
	  ((*iter).second.bGotExcluded == false))
	{
	  if(!bMissed)
	  {
		DebugPrintf("\nDid not get the INSERT_TAG_ACK from the following clients...\n");
	  }
	  
	  bMissed = true;
	  (*iter).second.bGotExcluded = true;
	  DebugPrintf("%s ", (*iter).first.c_str());
	}
	iter++;
  }
  DebugPrintf("\n");

  //reset the status info counter for next phase
  nVacpStatusInfoCounter = 0;
}
void Communicator::CheckForMissingResumeAck()
{
  bool bMissed = false;
  std::map<std::string,MsgBucket>::iterator iter = m_mapIpToMsg.begin();
  while(iter != m_mapIpToMsg.end())
  {
	if(((*iter).second.bResumeAck == false) &&
	  ((*iter).second.bGotExcluded == false))
	{
	  if(!bMissed)
	  {
		DebugPrintf("\nDid not get the RESUME ACK from the following clients...\n");
	  }

	  bMissed = true;
	  (*iter).second.bGotExcluded = true;
	  DebugPrintf("%s ", (*iter).first.c_str());
	}
	iter++;
  }
  DebugPrintf("\n");
}

int Communicator::SendMessageToMasterNode(ENUM_MSG_TYPE msgType,std::string strData)
{
    if (!m_bActive.value())
    {
        inm_printf("\nSendMessageToMasterNode: %s %s communicator is not active.\n",
            m_type.c_str(), m_mode.c_str());
        return 1;
    }

  //Send Message to Master Vacp Node
  int nRet = 0;
 
  //1.Prepare the Message Packet
  stPacket preparePkt;
  preparePkt.Clear();
  PrepareMessageToBeSent( true,
						  DVACP_NEW_PACKET_REQUEST_ID,
						  msgType,
						  DIR_COORD_TO_MASTER,
						  g_strLocalIpAddress,
						  g_strMasterHostName,
						  strData,
						  preparePkt);
  //2.Send the Message Packet
  bool bSent = false;
  for(int nRetry = 0; nRetry < 3;nRetry++)
  {
	nRet = SendPacket(preparePkt);
	if(0 != nRet)
	{
	  if(nRetry > 0)
	  {
		DebugPrintf("\nRetrying to send the message to Master Node:%d\n",nRetry+1);
	  }
	  DebugPrintf("\nAttempt = %d, Failed to send the %d Command to %s Master node. Error = %d\n",nRetry+1,(UINT)msgType,g_strMasterHostName.c_str(),nRet);
	  //DebugPrintf("\nError = ERROR_SENDING_PREPARE_CMD\n");
	  ACE_OS::sleep(5);
	  continue;
	}
	else
	{
	  bSent = true;
	  break;
	}
  }
  if(!bSent)
  {
	DebugPrintf("\nFailed to send %d message to %s Master Node.\n",(UINT)msgType,g_strMasterHostName.c_str());
	nRet = ERROR_FAILED_TO_SEND_MSG_TO_MASTER_NODE;
  }
  return nRet;
}

int Communicator::SendMessageToEachCoordinatorNode(ENUM_MSG_TYPE msgType, std::string strData)
{
    if (!m_bActive.value())
    {
        inm_printf("\nSendMessageToEachCoordinatorNode: %s %s communicator is not active.\n",
            m_type.c_str(), m_mode.c_str());
        return 1;
    }

    //Send Message to each coordinator node

    bool bAtleastSentToOneCoordinator = false;
    int nRet = 0;
    std::map<std::string, MsgBucket>::iterator iter = m_mapIpToMsg.begin();
    while (iter != m_mapIpToMsg.end())
    {
        MsgBucket &msgBucket = iter->second;

        if (msgBucket.bGotExcluded)
        {
            iter++;
            continue;
        }

        nRet = SendMessageToCoordinatorNode(iter->first, msgType, strData);
        if (!nRet)
        {
            bAtleastSentToOneCoordinator = true;
        }
        iter++;
    }

    if (!bAtleastSentToOneCoordinator)
    {
        DebugPrintf("\nError:Could not send the message to even one Client Node.\n");
        nRet = 1;
    }
    else
    {
        nRet = 0;
    }

    return nRet;
}

int Communicator::SendMessageToCoordinatorNode(const std::string& node,
    ENUM_MSG_TYPE msgType,
    std::string strData)
{
    int nRet = 0;
    //1.Prepare the Message Packet
    stPacket preparePkt;
    preparePkt.Clear();
    PrepareMessageToBeSent(true,
        DVACP_NEW_PACKET_REQUEST_ID,
        msgType,
        DIR_MASTER_TO_COORD,
        g_strMasterHostName,
        node,
        strData,
        preparePkt);

    //2.Send the Message Packet
    bool bSent = false;
    for (int nRetry = 0; nRetry < 5; nRetry++)
    {
        nRet = SendPacket(preparePkt);
        if (0 != nRet)
        {
            if (!is_client_loggedin(node, m_type))
                break;

            DebugPrintf("\nRetrying ...\n");
            ACE_OS::sleep(5);
            continue;
        }
        else
        {
            bSent = true;
            break;
        }
    }

    if (!bSent)
    {
        DebugPrintf("\nFailed to send %d message to %s coordinator node.Error = %d\n",
            (int)msgType, node.c_str(), nRet);
    }

    return nRet;
}

std::string Communicator::GetLocalSystemTime()
{

    //steady_clock::time_point currentTime = steady_clock::now();

    int64_t msecs;
    time_t time;

    system_clock::time_point currentTime = system_clock::now();
    time = system_clock::to_time_t(currentTime);
    msecs = currentTime.time_since_epoch().count();

    struct tm * timeinfo;
    char time_in_readable_format[80];
    bool bRet = true;

    //if (ACE_OS::time(&time) != -1)
    {
        //msecs = ACE_OS::gettimeofday().msec();
        timeinfo = ACE_OS::localtime(&time);
        if (ACE_OS::strftime(time_in_readable_format, 80, "%Y-%m-%d-%H:%M:%S", timeinfo) == 0)
            bRet = false;
    }
    //else
    //{
        //bRet = false;
    //}

    std::string strTime;
    if (bRet)
    {
        strTime = time_in_readable_format;
        char strMsecs[20] = { 0 };
        inm_sprintf_s(strMsecs, ARRAYSIZE(strMsecs), "%llu", msecs);
        strTime += " ";
        strTime += strMsecs;
    }

    return strTime;
}

unsigned int stPacket::ConvertToNative(unsigned char* recvdBytes,
    ENUM_DATA_TYPE type)
{
    unsigned int toNativeValue = 0xFFFFFFFE;//some error value
    if (type == ENUM_UNSIGNED_SHORT) // 2 bytes
    {
        toNativeValue = ((recvdBytes[0] << 8) | (recvdBytes[1]));
    }
    else if (type == ENUM_UNSIGNED_INT)// 4 bytes
    {
        toNativeValue = ((recvdBytes[0] << 24) | (recvdBytes[1] << 16) | (recvdBytes[2] << 8) | (recvdBytes[3]));
    }
    else if (type == ENUM_UNSIGNED_CHAR)
    {
        toNativeValue = recvdBytes[0];
    }
    return toNativeValue;
}

unsigned char* stPacket::ConvertToNetworkByteOrder(unsigned int intNum,
    ENUM_DATA_TYPE type,
    unsigned char* sendBytes)
{
    if (type == ENUM_UNSIGNED_CHAR)
    {
        sendBytes[0] = intNum;
    }
    else if (type == ENUM_UNSIGNED_SHORT) // 2 bytes
    {
        sendBytes[0] = (unsigned char)(intNum >> 8);
        sendBytes[1] = (unsigned char)intNum;
    }
    else if (type == ENUM_UNSIGNED_INT)// 4 bytes
    {
        sendBytes[0] = (unsigned char)(intNum >> 24);
        sendBytes[1] = (unsigned char)(intNum >> 16);
        sendBytes[2] = (unsigned char)(intNum >> 8);
        sendBytes[3] = (unsigned char)intNum;
    }

    return sendBytes;
}

unsigned char* stPacket::ReadBytes(unsigned char* pBuffer,
    unsigned short nStartPos,
    unsigned short numberOfBytes)
{
    unsigned char* pByte = NULL;
    if (numberOfBytes)
    {
        //memset((void*)&m_readBytesBuffer[0],0x00,256);
        pByte = new(unsigned char[numberOfBytes]);
        memset((void*)pByte, 0x00, (size_t)numberOfBytes);
        inm_memcpy_s((void*)pByte, numberOfBytes, (const void*)(pBuffer + nStartPos), (size_t)numberOfBytes);
    }
    return pByte;
}

std::string stPacket::ConvertToString(unsigned char* pRecvdBytes, unsigned int nLen)
{
    std::string strValue;
    if (pRecvdBytes)
    {
        strValue.assign((char*)pRecvdBytes, nLen);
    }
    return strValue;
}

void stPacket::Clear()
{
    socketId = 0;
    commSequenceId = 0;
    //requestId = 0;
    direction = 0;
    FunctionId = INVALID_FUNCTION_ID;//0xFF;
    srcNodeLen = 0;
    strSrcNode.clear();
    destNodeLen = 0;
    strDestNode.clear();
    dataLen = 0;
    strData.clear();
}
