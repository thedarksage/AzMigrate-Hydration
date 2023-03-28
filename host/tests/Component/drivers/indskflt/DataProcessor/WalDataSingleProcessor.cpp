
#include "DataProcessor.h"


#define ENTER 	{m_pLogger->LogInfo("Enter %s", __FUNCTION__);}
#define EXIT 	{m_pLogger->LogInfo("Exit %s", __FUNCTION__);}


CDataWALSingleProcessor::CDataWALSingleProcessor(int srcDisk, string testRoot, IPlatformAPIs *platform, ILogger *logger) :
m_platformApis(platform),
m_pLogger(logger),
m_currentDirIndex(0),
m_bTagInProgress(false),
m_bInitialSyncCompleted(false),
m_tagProcessed(true, false, std::string("")),
m_waitTSO(false, true, std::string(""))
{
    m_pSrcDisk = new CDiskDevice(srcDisk, m_platformApis);
    m_bufferLength = m_pSrcDisk->GetMaxRwSizeInBytes();
    m_buffer = new char[m_bufferLength];

    m_testRoot = testRoot + "\\";
    m_testRoot += to_string(m_pSrcDisk->GetDeviceNumber());

    CreateTagDirectory();
}

CDataWALSingleProcessor::~CDataWALSingleProcessor()
{
    SafeDelete(m_buffer);
    SafeDelete(m_pSrcDisk);
    SafeDelete(m_targetDisk);
}

void CDataWALSingleProcessor::WaitForTSO(SV_ULONG timeoutMS)
{
	ENTER
		m_waitTSO.Wait(0, timeoutMS);
    EXIT
}

bool CDataWALSingleProcessor::WaitForTag(string tagName, SV_ULONG timeout, bool dontApply)
{
	return true;
}

void CDataWALSingleProcessor::ProcessTSO()
{
	ENTER
		m_waitTSO.Signal(true);
	EXIT
}


void CDataWALSingleProcessor::WriteDataFile(string filename, char* buffer, int length)
{
    if ((NULL == buffer) || (0 == length))
    {
        return;
    }

	void* hFile = m_platformApis->Open(filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);

    if (!m_platformApis->IsValidHandle(hFile))
    {
        throw CDataProcessorException("opening handle on file %s failed with err=0x%x", filename.c_str(), m_platformApis->GetLastError());
    }

    SV_ULONG dwBytes;
    if (!m_platformApis->Write(hFile, buffer, length, dwBytes))
    {
		m_platformApis->Close(hFile);
        throw CDataProcessorException("Writefile: file %s failed with err=0x%x", filename.c_str(), m_platformApis->GetLastError());
    }

	m_platformApis->Close(hFile);
}


void CDataWALSingleProcessor::ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length)
{
    //	ENTER
    size_t StreamOffset = 0;

    // Park these changes
    if (!m_bInitialSyncCompleted || NULL == pSourceStream)
    {
        // these changes are in meta data mode.
        // nothing to do
        EXIT
            return;
    }

    if (length <= 0)
    {
        throw CDataProcessorException("length <= 0");
    }

    // Wait till Tags have been processed.
    m_dirDiskMutex.lock();
    StreamOffset = pSourceStream->StreamOffset;

    // Not locking this section as processing will be always in-order
    stringstream    ssCurrentFile;
    ssCurrentFile << m_currentDirectory << "\\" << m_currentFileIndex++;

    string nextfile = ssCurrentFile.str();

    // Create a new file
    pSourceStream->DataStream->copy(StreamOffset, (size_t)length, m_buffer);
    WriteDataFile(nextfile, m_buffer, length);

    m_dirDiskMutex.unlock();
    //	EXIT
}


// Only one thread should enter this section.
void CDataWALSingleProcessor::ProcessTags(list<string> tags)
{
	ENTER

		// Rename current directory to tag name
		//m_currentDirectory
	size_t lastIndex = m_currentDirectory.find_last_of("/\\");
	string directoryname = m_currentDirectory.substr(0, lastIndex) + "\\" + tags.front();

	int error = rename(m_currentDirectory.c_str(), directoryname.c_str());
	if (0 != error)
	{
		throw CDataProcessorException("Failed to rename directory");
	}

    CreateTagDirectory();

    EXIT
}

// This function should be called with mutex held
void CDataWALSingleProcessor::CreateTagDirectory()
{
    ENTER
    m_dirDiskMutex.lock();
    {
        stringstream    ssCurrentDir;
        ssCurrentDir << m_testRoot << "\\";

		if (!m_platformApis->PathExists(ssCurrentDir.str().c_str()))
		{
			if (!CreateDirectoryA(ssCurrentDir.str().c_str(), NULL))
			{
				int error = m_platformApis->GetLastError();
				throw CDataProcessorException("Failed to created directory %s err=0x%x", m_currentDirectory.c_str(), m_platformApis->GetLastError());
			}
		}

        ssCurrentDir << to_string(++m_currentDirIndex);

        // Create own directory
        // Everyone Creates their own directory
        m_currentDirectory = ssCurrentDir.str();

        m_pLogger->LogInfo("%s: Creating folder %s", __FUNCTION__, m_currentDirectory.c_str());
        if (!CreateDirectoryA(m_currentDirectory.c_str(), NULL))
        {
			int error = m_platformApis->GetLastError();
            throw CDataProcessorException("Failed to created directory %s err=0x%x", m_currentDirectory.c_str(), m_platformApis->GetLastError());
        }
    }
    m_currentFileIndex = 1;
    m_dirDiskMutex.unlock();

    EXIT
}

void CDataWALSingleProcessor::InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset)
{
    ENTER
        m_bInitialSyncCompleted = true;
    EXIT
}