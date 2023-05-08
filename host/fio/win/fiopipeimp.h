
///
/// \file fiopipeimp.h
///
/// \brief
///

#ifndef FIOPIPEIMP_H
#define FIOPIPEIMP_H

#include <windows.h>

#include <string>
#include <iomanip>

#include <boost/bind.hpp>

#include "fiotypesmajor.h"
#include "errorexception.h"
#include "scopeguard.h"

namespace FIO {

    typedef DWORD EXIT_CODE;

    class FioPipeImp {
    public:
        enum PIPE_ACTION {
            PIPE_READ = 0,
            PIPE_WRITE = 1
        };

        FioPipeImp()
            : m_error(ERROR_INVALID_HANDLE),
              m_processStarted(false),
              m_exitCode(0)
            {
                initHandles();
            }

        explicit FioPipeImp(cmdArgs_t& cmdArgs, bool tieStderrStdout = false)
            : m_error(ERROR_INVALID_HANDLE),
              m_processStarted(false),
              m_exitCode(0)
            {
                initHandles();
                if (!create(cmdArgs, tieStderrStdout)) {
                    throw ERROR_EXCEPTION << "could not create pipe: " << errorAsString();
                }
            }

        ~FioPipeImp()
            {
                close();
            }

        bool create(cmdArgs_t& cmdArgs, bool tieStderrStdout = false)
            {
                close();

                STARTUPINFOA startInfo;

                SECURITY_ATTRIBUTES securityAttributes;

                securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
                securityAttributes.bInheritHandle = TRUE;
                securityAttributes.lpSecurityDescriptor = NULL;

                SCOPE_GUARD guard = MAKE_SCOPE_GUARD(boost::bind(&FioPipeImp::close, this));
                // setup stdin
                if (!CreatePipe(&m_stdin[PIPE_READ], &m_stdin[PIPE_WRITE], &securityAttributes, 0)) {
                    m_error = GetLastError();
                    return false;
                }
                if (!SetHandleInformation(m_stdin[PIPE_WRITE], HANDLE_FLAG_INHERIT, 0)) {
                    m_error = GetLastError();
                    return false;
                }

                // setup stdout
                if (!CreatePipe(&m_stdout[PIPE_READ], &m_stdout[PIPE_WRITE], &securityAttributes, 0)) {
                    m_error = GetLastError();
                    return false;
                }
                if (!SetHandleInformation(m_stdout[PIPE_READ], HANDLE_FLAG_INHERIT, 0)) {
                    m_error = GetLastError();
                    return false;
                }

                // setup stderr
                if (!CreatePipe(&m_stderr[PIPE_READ], &m_stderr[PIPE_WRITE], &securityAttributes, 0)) {
                    m_error = GetLastError();
                    return false;
                }
                if (!SetHandleInformation(m_stderr[PIPE_READ], HANDLE_FLAG_INHERIT, 0)) {
                    m_error = GetLastError();
                    return false;
                }

                memset(&m_processInfo, 0, sizeof(PROCESS_INFORMATION));
                memset(&startInfo, 0,sizeof(STARTUPINFO));
                startInfo.cb = sizeof(STARTUPINFO);
                startInfo.hStdError = (tieStderrStdout ? m_stdout[PIPE_WRITE] : m_stderr[PIPE_WRITE]);
                startInfo.hStdOutput = m_stdout[PIPE_WRITE];
                startInfo.hStdInput = m_stdin[PIPE_READ];
                startInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                startInfo.wShowWindow = SW_HIDE;;

                std::string cmdAndArgs;
                cmdArgs_t::iterator iter(cmdArgs.begin());
                cmdArgs_t::iterator iterEnd(cmdArgs.end());
                if (iter != iterEnd) {
                    cmdAndArgs += *iter;
                }
                ++iter;
                for (/* empty */; iter != iterEnd; ++iter) {
                    if (0 != *iter) {
                        cmdAndArgs += " ";
                        cmdAndArgs += *iter;
                    }
                }

                if (!CreateProcessA(NULL, (LPSTR)cmdAndArgs.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startInfo, &m_processInfo)) {
                    m_error = GetLastError();
                    return false;
                }
                m_processStarted = true;
                guard.dismiss();
                m_error = FIO_SUCCESS;
                return true;
            }

        void close()
            {
                if (m_processStarted) {
                    checkExit(true);
                    CloseHandle(m_processInfo.hProcess); // MAYBE: leave open to monitor
                    CloseHandle(m_processInfo.hThread);  // MAYBE: leave open to monitor
                }
                m_processStarted = false;
                closeStdin();
                closeStdout();
                closeStderr();
            }

        void close(int pipe)
            {
                switch (pipe) {
                    case PIPE_STDIN:
                        closeStdin();
                        break;
                    case PIPE_STDOUT:
                        closeStdout();
                        break;
                    case PIPE_STDERR:
                        closeStderr();
                        break;
                    default:
                        break;
                }
            }

        long read(char* buffer, long length, bool block = true)
            {
                if (INVALID_HANDLE_VALUE == m_stdout[PIPE_READ]) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }
                if (!block && !ready(false)) {
                    checkExit(false);
                    return -1;
                }

                DWORD bytes;
                if (!ReadFile(m_stdout[PIPE_READ], buffer, length, &bytes, NULL)) {
                    m_error = GetLastError();
                    return -1;
                }
                return bytes;
            }

        long readErr(char* buffer, long length, bool block)
            {
                if (INVALID_HANDLE_VALUE == m_stderr[PIPE_READ]) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }
                if (!block && !ready(true)) {
                    checkExit(false);
                    return -1;
                }

                DWORD bytes;
                if (!WriteFile(m_stderr[PIPE_READ], buffer, length, &bytes, NULL)) {
                    m_error = GetLastError();
                    return false;
                }
                return bytes;
            }

        long write(char const* buffer, long length)
            {
                if (INVALID_HANDLE_VALUE == m_stdin[PIPE_WRITE]) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }

                DWORD bytes;
                if (!WriteFile(m_stdin[PIPE_WRITE], buffer, length, &bytes, NULL)) {
                    m_error = GetLastError();
                    return false;
                }
                return bytes;
            }

        unsigned long error()
            {
                return m_error;
            }

        std::string errorAsString()
            {
                char* buffer = 0;
                if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                       NULL,
                                       m_error,
                                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                       (LPTSTR) &buffer,
                                       0,
                                       NULL)) {
                    try {
                        std::stringstream err;
                        err << "failed to get message for error code: " << m_error;
                        m_errorStr = err.str();
                    } catch (...) {
                        // no throw
                    }

                } else {

                    try {
                        m_errorStr = buffer;
                    } catch (...) {
                        // no throw
                    }
                }

                if (0 != buffer) {
                    LocalFree(buffer);
                }
                return m_errorStr;
            }


    protected:
        void initHandles()
            {
                m_stdin[0] = INVALID_HANDLE_VALUE;
                m_stdin[1] = INVALID_HANDLE_VALUE;
                m_stdout[0] = INVALID_HANDLE_VALUE;
                m_stdout[1] = INVALID_HANDLE_VALUE;
                m_stderr[0] = INVALID_HANDLE_VALUE;
                m_stderr[1] = INVALID_HANDLE_VALUE;
            }

        void checkExit(bool wait)
            {
                if (m_processStarted) {
                    if (WAIT_TIMEOUT != WaitForSingleObject(m_processInfo.hProcess, wait ? INFINITE : 1)) {
                        GetExitCodeProcess(m_processInfo.hProcess, &m_exitCode);
                        m_error = m_exitCode;
                    }
                }
            }

        bool ready(bool stdErr = false)
            {
                DWORD bytes;
                DWORD bytesAvail;
                DWORD bytesLeft;
                if (!PeekNamedPipe(stdErr ? m_stderr[PIPE_READ] : m_stdout[PIPE_READ], 0, 0, &bytes, &bytesAvail, &bytesLeft)) {
                    m_error = GetLastError();
                    return false;
                }
                if (0 == bytesAvail) {
                    m_error = FIO_WOULD_BLOCK;
                }
                return (bytesAvail > 0);
            }

        void closeStdin()
            {
                if (INVALID_HANDLE_VALUE != m_stdin[PIPE_READ]) {
                    CloseHandle(m_stdin[PIPE_READ]);
                    m_stdin[PIPE_READ] = INVALID_HANDLE_VALUE;
                }
                if (INVALID_HANDLE_VALUE != m_stdin[PIPE_WRITE]) {
                    CloseHandle(m_stdin[PIPE_WRITE]);
                    m_stdin[PIPE_WRITE] = INVALID_HANDLE_VALUE;
                }
            }

        void closeStdout()
            {
                if (INVALID_HANDLE_VALUE != m_stdout[PIPE_READ]) {
                    CloseHandle(m_stdout[PIPE_READ]);
                    m_stdout[PIPE_READ] = INVALID_HANDLE_VALUE;
                }
                if (INVALID_HANDLE_VALUE != m_stdout[PIPE_WRITE]) {
                    CloseHandle(m_stdout[PIPE_WRITE]);
                    m_stdout[PIPE_WRITE] = INVALID_HANDLE_VALUE;
                }
            }

        void closeStderr()
            {
                if (INVALID_HANDLE_VALUE != m_stderr[PIPE_READ]) {
                    CloseHandle(m_stderr[PIPE_READ]);
                    m_stderr[PIPE_READ] = INVALID_HANDLE_VALUE;
                }
                if (INVALID_HANDLE_VALUE != m_stderr[PIPE_WRITE]) {
                    CloseHandle(m_stderr[PIPE_WRITE]);
                    m_stderr[PIPE_WRITE] = INVALID_HANDLE_VALUE;
                }
            }

    private:
        FioPipeImp(FioPipeImp& fioPipeImp);
        FioPipeImp& operator=(FioPipeImp* fioPipeImp);


        HANDLE m_stdin[2];
        HANDLE m_stdout[2];
        HANDLE m_stderr[2];

        unsigned long m_error;

        std::string m_errorStr;

        PROCESS_INFORMATION m_processInfo;
        bool m_processStarted;
        EXIT_CODE m_exitCode;
    };

} //namespace FIO

#endif // FIOPIPEIMP_H
