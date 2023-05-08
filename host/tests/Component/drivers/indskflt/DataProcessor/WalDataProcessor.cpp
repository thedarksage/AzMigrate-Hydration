#include "stdafx.h"
#include "DataProcessor.h"

//#define ENTER 
//#define EXIT 

#define ENTER 	{m_pLogger->LogInfo("Enter %s", __FUNCTION__);}
#define EXIT 	{m_pLogger->LogInfo("Exit %s", __FUNCTION__);}

int CDataWALProcessor::m_currentGlobalDirIndex = 0;
mutex CDataWALProcessor::m_dirGlobalMutex;
volatile SHORT  CDataWALProcessor::s_numDisksInSet = 0;
volatile SHORT CDataWALProcessor::s_numDisksReceivedTag = 0;
bool CDataWALProcessor::m_bRootDirectoryCreated = false;
volatile SHORT CDataWALProcessor::s_numDisksCommitedTag = 0;

volatile HANDLE	CDataWALProcessor::s_allDisksReceivedTag = INVALID_HANDLE_VALUE;
volatile HANDLE	CDataWALProcessor::s_allDisksCommittedTag = INVALID_HANDLE_VALUE;

CDataWALProcessor::CDataWALProcessor(int srcDisk, string testRoot, CPlatformAPIs *platform, CLogger *logger) :
m_platformApis(platform),
m_pLogger(logger),
m_currentDirIndex(0),
m_testRoot(testRoot),
m_bTagInProgress(false),
m_bInitialSyncCompleted(false)
{
	m_pSrcDisk = new CDiskDevice(srcDisk, m_platformApis);
	m_bufferLength = m_pSrcDisk->GetMaxRwSizeInBytes();

	m_buffer = new char[m_bufferLength];

	m_dirGlobalMutex.lock();
	if (0 == s_numDisksInSet)
	{
		s_allDisksReceivedTag = CreateEvent(NULL, TRUE, FALSE, NULL);
		s_allDisksCommittedTag = CreateEvent(NULL, TRUE, FALSE, NULL);;
	}
	++s_numDisksInSet;
	m_dirGlobalMutex.unlock();

	CreateTagDirectory();
}

CDataWALProcessor::~CDataWALProcessor()
{
	SafeDelete(m_buffer);
	SafeDelete(m_pSrcDisk);
	SafeDelete(m_targetDisk);
}

void CDataWALProcessor::WaitForTSO()
{
	ENTER
		WaitForSingleObject(m_waitTSO, INFINITE);
	EXIT
}

bool CDataWALProcessor::WaitForTag(string tagName, DWORD timeout)
{
	return true;
}


void CDataWALProcessor::ProcessTSO()
{
	ENTER
		SetEvent(m_waitTSO);
	EXIT
}

void WriteDataFile(string filename, char* buffer, int length)
{
	if ((NULL == buffer) || (0 == length))
	{
		return;
	}

	HANDLE hFile = CreateFileA(filename.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CREATE_NEW,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
		NULL);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		DWORD dwErr = GetLastError();
		CloseHandle(hFile);
		throw new CDataProcessorException("opening handle on file %s failed with err=0x%x", filename.c_str(), dwErr);
	}

	DWORD dwBytes;
	if (!WriteFile(hFile, buffer, length, &dwBytes, NULL))
	{
		DWORD dwErr = GetLastError();
		CloseHandle(hFile);
		throw new CDataProcessorException("Writefile: file %s failed with err=0x%x", filename.c_str(), dwErr);
	}

	CloseHandle(hFile);
}

void CDataWALProcessor::ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length)
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
		throw new CDataProcessorException("length <= 0");
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
void CDataWALProcessor::ProcessTags(list<string> tags)
{
	ENTER
		bool bTagSucceeded = false;

	// Wait to receive tags to all of Disks
	m_dirDiskMutex.lock();

	// There is already one tag in progress
	if (m_bTagInProgress)
	{
		m_pLogger->LogInfo("%s: There is already one tag in progress", __FUNCTION__);
		m_dirDiskMutex.unlock();
		EXIT
			return;
	}

	m_bTagInProgress = true;

	m_dirGlobalMutex.lock();
	SHORT numDisksReceivedTag = InterlockedIncrement16(&s_numDisksReceivedTag);

	if (1 == numDisksReceivedTag && 1 != s_numDisksInSet)
	{
		ResetEvent(s_allDisksCommittedTag);
		ResetEvent(s_allDisksReceivedTag);
	}
	m_dirGlobalMutex.unlock();

	DWORD status = WAIT_OBJECT_0;
	if (s_numDisksInSet == numDisksReceivedTag)
	{
		m_pLogger->LogInfo("Setting event numDisksReceivedTag");
		SetEvent(s_allDisksReceivedTag);
	}
	else
	{
		m_pLogger->LogInfo("%s: numDisksReceivedTag = %d", __FUNCTION__, numDisksReceivedTag);
		status = WaitForSingleObject(s_allDisksReceivedTag, 2 * 60 * 1000);
	}

	if (WAIT_OBJECT_0 != status)
	{
		InterlockedDecrement16(&s_numDisksReceivedTag);
		m_bTagInProgress = false;
		m_pLogger->LogInfo("%s: Tag Failure.. not all disks received tags s_numDisksReceivedTag = %d status=0x%x", __FUNCTION__, s_numDisksReceivedTag, status);
		m_dirDiskMutex.unlock();
		EXIT
			return;
	}

	m_pLogger->LogInfo("Everyone Received Tags");

	// increase current directory index and create it
	// Following code is inside static mutex
	CreateTagDirectory();

	SHORT numDisksCommitedTag = InterlockedIncrement16(&s_numDisksCommitedTag);
	if (numDisksCommitedTag == s_numDisksReceivedTag)
	{
		SetEvent(s_allDisksCommittedTag);
		// All the guys have commited tag
		s_numDisksCommitedTag = 0;
	}
	else
	{
		WaitForSingleObject(s_allDisksCommittedTag, INFINITE);
	}

	m_bTagInProgress = false;
	m_dirDiskMutex.unlock();
	EXIT
}

// This function should be called with mutex held
void CDataWALProcessor::CreateTagDirectory()
{
	ENTER
		stringstream    ssCurrentDir;
	ssCurrentDir << m_testRoot << "\\";

	m_dirGlobalMutex.lock();
	{
		m_pLogger->LogInfo("%s: s_numDisksReceivedTag = %d", __FUNCTION__, s_numDisksReceivedTag);

		// Wait till all disks have received tags
		if (m_currentDirIndex > m_currentGlobalDirIndex)
		{
			// Should never happen
			throw new CDataProcessorException("(m_currentDirIndex > m_currentGlobalDirIndex");
		}

		// This is first guy which has received Tag
		if (m_currentDirIndex == m_currentGlobalDirIndex)
		{
			ssCurrentDir << ++m_currentGlobalDirIndex;
			m_pLogger->LogInfo("%s: Creating folder %s", __FUNCTION__, ssCurrentDir.str().c_str());
			if (!CreateDirectoryA(ssCurrentDir.str().c_str(), NULL))
			{
				//TODO: Handle directory already exists error
				DWORD dwErr = m_platformApis->GetLastError();
				//if (ERROR_ALREADY_EXISTS != dwErr)
				{
					throw new CDataProcessorException("Failed to created directory %s", ssCurrentDir.str().c_str());
				}
			}
		}
		else
		{
			ssCurrentDir << m_currentGlobalDirIndex;
		}
		m_currentDirIndex = m_currentGlobalDirIndex;
	}
	m_dirGlobalMutex.unlock();

	// Create own directory
	// Everyone Creates their own directory
	ssCurrentDir << "\\" << m_pSrcDisk->GetDeviceNumber();
	m_currentDirectory = ssCurrentDir.str();

	m_pLogger->LogInfo("%s: Creating folder %s", __FUNCTION__, m_currentDirectory.c_str());
	if (!CreateDirectoryA(m_currentDirectory.c_str(), NULL))
	{
		throw new CDataProcessorException("Failed to created directory %s", m_currentDirectory.c_str());
	}

	m_currentFileIndex = 1;
	EXIT
}

void CDataWALProcessor::InitialSync(ULONGLONG currentOffset, ULONGLONG endOffset)
{
	ENTER
		m_bInitialSyncCompleted = true;
	EXIT
}