// S2Agent.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "S2Agent.h"
#include <map>

// Export functions for S2 Interface

extern "C" S2AGENT_API CS2Agent* S2AgentCreate(int replicationType, const char *testRoot)
{
	return new CS2Agent(replicationType, testRoot);
}

extern "C" S2AGENT_API void S2AgentDispose(CS2Agent* pObject)
{
	if (pObject != NULL)
	{
		delete pObject;
		pObject = NULL;
	}
}

extern "C" S2AGENT_API void S2AgentStartReplication(CS2Agent* pObject, SV_ULONGLONG startOffset, SV_ULONGLONG endOffset)
{
	if (pObject != NULL)
	{
		pObject->StartReplication(startOffset, endOffset);
	}
}

extern "C" S2AGENT_API void S2AgentAddReplicationSettings(CS2Agent* pObject, int uiSrcDisk, int uiDestDisk, const char *logFile)
{
	if (pObject != NULL)
	{
		pObject->AddReplicationSettings(uiSrcDisk, uiDestDisk, logFile);
	}
}

extern "C" S2AGENT_API void S2AgentAddFileReplicationSettings(CS2Agent* pObject, int uiSrcDisk, const char* szFileName, const char *logFile)
{
	if (pObject != NULL)
	{
		pObject->AddReplicationSettings(uiSrcDisk, szFileName, logFile);
	}
}

extern "C" S2AGENT_API bool S2AgentWaitForConsistentState(CS2Agent* pObject, bool dontApply=false)
{
	if (pObject != NULL)
	{
		return pObject->WaitForConsistentState(dontApply);
	}
	return false;
}

extern "C" S2AGENT_API void S2AgentResetTSO(CS2Agent* pObject, int uiSrcDisk)
{
	if (pObject != NULL)
	{
		pObject->ResetTSO(uiSrcDisk);
	}
}

extern "C" S2AGENT_API void S2AgentWaitForTSO(CS2Agent* pObject, int uiSrcDisk)
{
	if (pObject != NULL)
	{
		pObject->WaitForTSO(uiSrcDisk);
	}
}

extern "C" S2AGENT_API bool S2AgentInsertTag(CS2Agent* pObject, const char* tagName)
{
	list<string> taglist;
	taglist.push_back(tagName);

	if (pObject != NULL)
	{
		return pObject->InsertCrashTag(tagName, taglist);
	}

	return false;
}

extern "C" S2AGENT_API bool S2AgentWaitForTAG(CS2Agent* pObject, int uiSrcDisk, const char* tag, int timeout)
{
	if (pObject != NULL)
	{
		return pObject->WaitForTAG(uiSrcDisk, tag, timeout);
	}
	return false;
}

extern "C" S2AGENT_API void StopFiltering(CS2Agent* pObject)
{
	if (pObject != NULL)
	{
		pObject->StopFiltering();
	}
}

extern "C" S2AGENT_API void StartFiltering(CS2Agent* pObject)
{
	if (pObject != NULL)
	{
		pObject->StartFiltering();
	}
}

extern "C" S2AGENT_API void ValidateDI(CS2Agent* pObject)
{
	if (pObject != NULL)
	{
		pObject->Validate();
	}
}

extern "C" S2AGENT_API void StartSVAgents(CS2Agent* pObject, unsigned long ulFlags = 0)
{
	if (pObject != NULL)
	{
		pObject->StartSVAgents(ulFlags);
	}
}

extern "C" S2AGENT_API void SetDriverFlags(CS2Agent* pObject, etBitOperation flag)
{
	if (pObject != NULL)
	{
		pObject->SetDriverFlags(flag);
	}
}