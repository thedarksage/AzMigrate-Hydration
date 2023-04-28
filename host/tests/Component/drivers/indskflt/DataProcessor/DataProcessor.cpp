// DataProcessor.cpp : Defines the exported functions for the DLL application.
//

#include "DataProcessor.h"


// This is the constructor of a class that has been exported.
// see DataProcessor.h for the class definition
CDataApplier::CDataApplier(int srcDisk, int targetDisk, IPlatformAPIs *platform, ILogger *logger) :
m_platformApis(platform),
m_pLogger(logger),
m_bInitialSyncCompleted(false),
#ifdef SV_UNIX
m_allowDraining(true, false, std::string("AllowDraining")),
#endif
m_tagProcessed(true, false, std::string("TagProcessed")),
m_waitTSO(false, true, std::string("WaitForTSO"))
{
    m_pSrcDisk = new CDiskDevice(srcDisk, m_platformApis);
    m_targetDisk = new CDiskDevice(targetDisk, m_platformApis);
    m_bufferLength = (m_pSrcDisk->GetMaxRwSizeInBytes() > m_targetDisk->GetMaxRwSizeInBytes()) ?
        m_targetDisk->GetMaxRwSizeInBytes() :
        m_pSrcDisk->GetMaxRwSizeInBytes();

    m_buffer.resize(m_bufferLength);
}

CDataApplier::CDataApplier(IBlockDevice *srcDevice, IBlockDevice *tgtDevice, IPlatformAPIs *platform, ILogger *logger) :
m_platformApis(platform),
m_pLogger(logger),
m_bInitialSyncCompleted(false),
m_tagProcessed(true, false, std::string("TagProcessed")),
m_waitTSO(false, true, std::string("WaitForTSO")),
#ifdef SV_UNIX
m_allowDraining(true, false, std::string("AllowDraining")),
#endif
m_DontApply(false),
m_DropTag(false)
{
    m_pSrcDisk = srcDevice;
    m_targetDisk = tgtDevice;

    m_bufferLength = m_pSrcDisk->GetMaxRwSizeInBytes();
    m_buffer.resize(m_bufferLength);
}

CDataApplier::~CDataApplier()
{
    SafeDelete(m_pSrcDisk);
    SafeDelete(m_targetDisk);
}

void CDataApplier::ProcessChanges(PSOURCESTREAM pSourceStream,
                                    SV_LONGLONG ByteOffset,
                                    SV_LONGLONG length)
{
    SV_ULONG dwBytesRead = 0, dwBytesWritten = 0;
    SV_ULONG dwBytesReturned = 0;
    SV_LONGLONG lWriteCount = 0;
    SV_LONGLONG lToRead = 0;
    size_t StreamOffset = 0;
    bool bSourceDisk = 1;

    m_tagProcessed.Wait(0, TIMEOUT_INFINITE);
    if (m_DontApply)
    {
        m_pLogger->LogInfo("Not applying any changes");
        return;
    }

    // Either input will be disk or stream. If disk then stream will be null
    bSourceDisk = (NULL == pSourceStream);

    // Length can be -1 if want to do sync step1. But it cannot be when source is not disk
    if ((length < 0) && (!bSourceDisk))
    {
        throw CDataProcessorException("Error in pSourceStream");
    }

    if (length <= 0)
    {
        throw CDataProcessorException("length <= 0");
    }

    if (bSourceDisk)
    {
        m_pLogger->LogInfo("MD: Offset: %I64x length: %I64x", ByteOffset, length);
        m_pSrcDisk->SeekFile(BEGIN, ByteOffset);
    }
    else
    {
        m_pLogger->LogInfo("ST: Offset: %I64x length: %I64x", ByteOffset, length);
        StreamOffset = pSourceStream->StreamOffset;
    }

    // Set Target to specified byte offset
    m_targetDisk->SeekFile(BEGIN, ByteOffset);

    // Set success flag
    lToRead = length;

    //m_logger->LogInfo("%s Offset:0x%x Length: 0x%x", __FUNCTION__, ByteOffset, length);
    while (lWriteCount < length)
    {
        lToRead = length - lWriteCount;
        lToRead = (m_bufferLength < lToRead) ? m_bufferLength : (length - lWriteCount);

        // Read from disk else from stream
        if (bSourceDisk)
        {
            dwBytesRead = 0;
            m_pSrcDisk->Read(&m_buffer[0], (SV_ULONG)lToRead, dwBytesRead);
        }
        else
        {
            pSourceStream->DataStream->copy(StreamOffset, (size_t)lToRead, &m_buffer[0]);
            StreamOffset = StreamOffset + (SV_ULONG)lToRead;
            dwBytesRead = lToRead;
        }

        // Write to destination
        m_targetDisk->Write(&m_buffer[0], dwBytesRead, dwBytesWritten);
        lWriteCount += dwBytesWritten;
    }
}

bool CDataApplier::WaitForTag(string tagName, SV_ULONG timeout, bool dontApply)
{
    m_pLogger->LogInfo("Enter %s tag: %s", __FUNCTION__, tagName.c_str());


#ifdef SV_WINDOWS
    m_pLogger->LogInfo("Before Dont apply : %d", m_DontApply);
    m_DontApply = dontApply;
    m_pLogger->LogInfo("AFter Dont apply : %d", m_DontApply);
#endif
    m_tagMutex.lock();
    if (m_TagReceived.find(tagName) != m_TagReceived.end())
    {
        m_pLogger->LogInfo("\nThis tag %s is already received and no need to wait for this tag.\n",
            tagName.c_str());
        m_tagMutex.unlock();
        return true;
    }

    Event* hEvent;
    if (m_tagWaitQueue.find(tagName) != m_tagWaitQueue.end())
    {
        m_pLogger->LogInfo("\nThere is already an Event associated with this tag %s\n",
            tagName.c_str());
        hEvent = m_tagWaitQueue[tagName];
    }
    else
    {
        hEvent = new Event(false, true, std::string("TagWait") + tagName);
        m_tagWaitQueue[tagName] = hEvent;
        m_pLogger->LogInfo(
            "\nCreated a new Event for associating it with a tag %s this is waiting for.\n",
            tagName.c_str());
    }
    m_tagMutex.unlock();
    m_pLogger->LogInfo("\nWaiting for the tag %s and it's corresponding event to be signalled. \n",
        tagName.c_str());
    SV_ULONG status = hEvent->Wait(0, timeout);
    m_tagMutex.lock();
    delete hEvent;
    m_tagWaitQueue.erase(tagName);
    m_tagMutex.unlock();
    if (WAIT_SUCCESS == status)
    {
        m_pLogger->LogInfo("\n The tag %s is received and signalled successfully.\n",
            tagName.c_str());
    }
    else if (WAIT_TIMED_OUT == status)
    {
        m_pLogger->LogError("\nTimeout happened while waiting for the tag %s.\n",
            tagName.c_str());
    }
    else
    {
        m_pLogger->LogError("Wait Failure happened while waiting for the tag %s.\n",
            tagName.c_str());
    }
    return WAIT_SUCCESS == status;
}

void CDataApplier::WaitForTSO(SV_ULONG timeoutMs)
{
    m_pLogger->LogInfo("%s : TSO", __FUNCTION__);
    m_waitTSO.Wait(0, timeoutMs);
}

void CDataApplier::ProcessTSO()
{
    m_pLogger->LogInfo("%s : TSO", __FUNCTION__);
    m_waitTSO.Signal(true);
}

bool CDataApplier::CreateDrainBarrierOnTag(std::string tag)
{
    m_pLogger->LogInfo("Enter %s tag: %s", __FUNCTION__, tag.c_str());

    m_tagMutex.lock();
    if (m_TagReceived.find(tag) != m_TagReceived.end())
    {
        m_tagMutex.unlock();
        m_pLogger->LogInfo(
            "\nReceived a tag by name %s which is already existing "
            "in the set of tags received earlier.\n ", tag.c_str());
        return false;
    }

    Event* hEvent = new Event(false, true, std::string("CreateDrainBarrier") + tag);
    m_tagWaitQueue[tag] = hEvent;
    m_pLogger->LogInfo(
        "\nCreating a new event based on the request to create a Drain Barrier with the tag %s\n",
        tag.c_str());
    m_tagMutex.unlock();
    return true;
}

void CDataApplier::ProcessTags(list<string> tags)
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);

    if (m_DropTag)
    {
        m_pLogger->LogInfo("Dropping Tag");
        return;
    }

    if (m_DontApply)
    {
        m_pLogger->LogInfo("Setting drop tag");
        m_DropTag = true;
    }
    list<string>::iterator  it = tags.begin();
    m_tagMutex.lock();
    for (; it != tags.end(); it++)
    {
        m_pLogger->LogInfo("Enter %s srcDisk: %d tag: %s",
            __FUNCTION__, m_pSrcDisk->GetDeviceNumber(), it->c_str());
        if (m_TagReceived.end() == m_TagReceived.find(*it))
        {
            m_pLogger->LogInfo("Received a new Tag called:%s", (*it).c_str());
            m_TagReceived.insert(*it);
        }

        // Wake anybody waiting for this tag
        if (m_tagWaitQueue.find(*it) != m_tagWaitQueue.end())
        {
            m_pLogger->LogInfo("Waking up anyone waiting for the tag %s", (*it).c_str());
            m_tagProcessed.Signal(false);
            m_tagWaitQueue.find(*it)->second->Signal(true);
        }
    }
    m_tagMutex.unlock();
}

void CDataApplier::InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset)
{
    m_pLogger->LogInfo("Enter %s srcDisk: %d targetDisk: %d startOffset: %I64x endOffset: %I64x",
        __FUNCTION__,
        m_pSrcDisk->GetDeviceNumber(),
        m_targetDisk->GetDeviceNumber(),
        currentOffset,
        endOffset);
#ifdef SV_UNIX
    m_tagProcessed.Signal(false); // Stop draining during IR (for Linux)
#endif
    m_pSrcDisk->SeekFile(BEGIN, 0);
    m_targetDisk->SeekFile(BEGIN, 0);

    SV_ULONG dwCopySize = m_pSrcDisk->GetCopyBlockSize();

    SV_ULONG dwBytes;
    SV_ULONG dwBytesToCopy;

    double tgtWriteTime = 0.0;
    double srcReadTime = 0.0;

    boost::timer    irTimer;
    while (currentOffset != endOffset)
    {
        dwBytesToCopy = (endOffset - currentOffset > m_bufferLength) ? m_bufferLength :
            (endOffset - currentOffset);

        irTimer.restart();
        m_pSrcDisk->Read(&m_buffer[0], dwBytesToCopy, dwBytes);
        srcReadTime += irTimer.elapsed();

        irTimer.restart();
        m_targetDisk->Write(&m_buffer[0], dwBytesToCopy, dwBytes);
        tgtWriteTime += irTimer.elapsed();

        currentOffset += dwBytesToCopy;
    }

    m_pLogger->LogInfo("%s: Time Spent for reading from source: %f secs Write to target: %f secs",
        __FUNCTION__,
        srcReadTime,
        tgtWriteTime);

    m_pLogger->LogInfo("Exit %s srcDisk: %s targetDisk: %s",
        __FUNCTION__, m_pSrcDisk->GetDeviceId().c_str(), m_targetDisk->GetDeviceId().c_str());
    m_bInitialSyncCompleted = true;
    m_DontApply = false;
#ifdef SV_UNIX
    m_tagProcessed.Signal(true); // Continue draining after IR (for Linux)
#endif
}

void CDataApplier::ReleaseDrainBarrier()
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);
    m_tagProcessed.Signal(true);
    m_pLogger->LogInfo("Exit %s", __FUNCTION__);
}

#ifdef SV_UNIX
void CDataApplier::WaitForAllowDraining()
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);
    m_allowDraining.Wait(0, TIMEOUT_INFINITE);
    m_pLogger->LogInfo("Exit %s", __FUNCTION__);
}

void CDataApplier::PauseDraining()
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);
    m_allowDraining.Signal(false);
    m_pLogger->LogInfo("Exit %s", __FUNCTION__);
}

void CDataApplier::AllowDraining()
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);
    m_allowDraining.Signal(true);
    m_pLogger->LogInfo("Exit %s", __FUNCTION__);
}

void CDataApplier::ResetTSO()
{
    m_pLogger->LogInfo("Enter %s", __FUNCTION__);
    m_waitTSO.ResetEvent();
    m_pLogger->LogInfo("Exit %s", __FUNCTION__);
}
#endif

