#ifndef _INMAGE_MACROS__H
#define _INMAGE_MACROS__H

#include <stdio.h>

#pragma once

/////////////////////////////////////////////////////////////////////////
//  utility macros
//

#define STRING_MERGE(A, B) A##B
#define GEN_WSTRING(A) STRING_MERGE(L, A)

#ifndef __WFILE__
#define __WFILE__ GEN_WSTRING(__FILE__)
#endif
#define __WFUNCTION__ GEN_WSTRING(__FUNCTION__)

#if (_WIN32_WINNT > 0x500)
std::string MapVssErrorCode2String(const HRESULT vssErrorCode);
#endif

#define CHECK_SUCCESS_RETURN_VAL(func, retval)	{					\
		retval = func;												\
		if (retval != S_OK)											\
		{															\
            std::stringstream ss;                                   \
            ss << "FAILED FILE: " << __FILE__ << " , LINE "         \
               << __LINE__ << ", hr = " << std::hex << retval << " "\
               << MapVssErrorCode2String(retval);                   \
            DebugPrintf("%s\n", ss.str().c_str());                  \
            AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();                    \
            AppConsistency::Get().m_vacpLastError.m_errorCode = retval;                   \
			break;													\
		}															\
	}

#define CHECK_FAILURE_RETURN_VAL(func, retval)	{					\
		retval = func;												\
		if (FAILED(retval))											\
		{															\
            std::stringstream ss;                                   \
            ss << "FAILED FILE: " << __FILE__ << " , LINE "         \
               << __LINE__ << ", hr = " << std::hex << retval << " "\
               << MapVssErrorCode2String(retval);                   \
            DebugPrintf("%s\n", ss.str().c_str());                  \
            AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();                    \
            AppConsistency::Get().m_vacpLastError.m_errorCode = retval;                   \
			break;													\
		}															\
	}

#define ADD_TO_VACP_FAIL_MSGS(fmod, errmsg, errcode) g_VacpFailMsgsList.push_back(FailMsg(fmod, errmsg, errcode))

#define ADD_TO_MULTIVM_FAIL_MSGS(fmod, errmsg, errcode) g_MultivmFailMsgsList.push_back(FailMsg(fmod, errmsg, errcode))

//
//  Very simple ASSERT definition
//
/*#ifdef _DEBUG
    #define _ASSERTE(x) {                               \
        if (!(x))                                       \
        {                                           \
            wprintf(L"\nASSERTION FAILED: %S\n", #x);       \
            wprintf(L"- File: %s\n- Line: %d\n- Function: %s\n", __WFILE__, __LINE__, __WFUNCTION__); \
            wprintf(L"\nPress <ENTER> to continue...");     \
            getchar();                              \
        }                                           \
    }
#else
    #define _ASSERTE(x)
#endif*/

#endif /* _INMAGE_MACROS_H */