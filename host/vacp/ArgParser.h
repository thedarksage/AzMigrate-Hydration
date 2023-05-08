#pragma once
#include "stdafx.h"
#include "CmdRequest.h"

class TagArgParser
{
    CLIRequest_t* m_Context;
    void ParseArguments(vector<const char*>& cont,_TCHAR* tokens, _TCHAR *seps = ",;" );
    bool ParseTagList(vector<const char*>& cont, _TCHAR* tokens, _TCHAR* seps = ",;");
    void ParseApplicationArguments(vector<AppParams_t>& cont ,_TCHAR* tokens, _TCHAR* seps = ",;" );
    bool IsDuplicateEntryPresent(vector<const char *>& cont,_TCHAR* token);
    bool IsDupilicateAppCmpPresent(vector<AppParams_t>& cont,AppParams_t token);
    void ParseServerDeviceIDArguments(vector<const char *>& cont ,_TCHAR* tokens, _TCHAR* seps = ",;" );
    void ParseSnapshotArguments(vector<const char *>& container, _TCHAR* tokens, _TCHAR* seps = ",;");
    void ParseSnapshotSetArguments(vector<const char *>& container, _TCHAR* tokens, _TCHAR* seps = ",;");
    void ParseCoordinatorNodesArguments(vector<const char *>& cont ,_TCHAR* tokens, _TCHAR* seps = ",;");

    void ParseArguments(std::set<std::string>& cont, _TCHAR* tokens, _TCHAR *seps = ",;");
    bool IsValidGuid(const std::string& strGuid);
public:
    TagArgParser(CLIRequest_t* ctx, int argc, _TCHAR* argv[]);
    std::vector<const char*> *GetCoordNodes();
    virtual ~TagArgParser();
#ifdef TEST
    void Print();
#endif

};



