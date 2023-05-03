
#include "DataProcessor.h"


#define ENTER 	{m_pLogger->LogInfo("Enter %s", __FUNCTION__);}
#define EXIT 	{m_pLogger->LogInfo("Exit %s", __FUNCTION__);}


CNullDataProcessor::CNullDataProcessor(int srcDisk, string testRoot, IPlatformAPIs *platform, ILogger *logger) :
m_pLogger(logger),
m_platformApis(platform),
m_testRoot(testRoot),
m_tagProcessed(false, true, std::string())
{
}

CNullDataProcessor::~CNullDataProcessor()
{
}

void CNullDataProcessor::WaitForTSO(SV_ULONG timeoutMS)
{
}

bool CNullDataProcessor::WaitForTag(string tagName, SV_ULONG timeout, bool dontApply)
{
	return true;
}

void CNullDataProcessor::ProcessTSO()
{
}


void CNullDataProcessor::ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length)
{
    ENTER
    EXIT
}

// Only one thread should enter this section.
void CNullDataProcessor::ProcessTags(list<string> tags)
{
    ENTER
    EXIT
}

void CNullDataProcessor::InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset)
{
    ENTER
    EXIT
}