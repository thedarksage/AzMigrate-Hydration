
///
/// \file fiopipeimp.h
///
/// \brief unix implementation
///

#ifndef FIOPIPEIMP_H
#define FIOPIPEIMP_H

#include <unistd.h>
#include <cstdio>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <iomanip>

#include <boost/bind.hpp>

#include "fiotypesmajor.h"
#include "errorexception.h"
#include "scopeguard.h"
#include "converterrortostringminor.h"

#define CLOSE_FD_PIPE(fdPipeToClose)            \
    do {                                        \
        if (-1 != fdPipeToClose[0]) {           \
            ::close(fdPipeToClose[0]);          \
            fdPipeToClose[0] = -1;              \
        }                                       \
        if (-1 != fdPipeToClose[1]) {           \
            ::close(fdPipeToClose[1]);          \
            fdPipeToClose[1] = -1;              \
        }                                       \
    } while(false);


namespace FIO {

    typedef int EXIT_CODE;

    /// \brief unix implementation that wrapps unix native pipe functions
    class FioPipeImp {
    public:
        FioPipeImp()
            : m_pid(-1),
              m_error(0)
            {
                initFds();
            }

        explicit FioPipeImp(cmdArgs_t& cmdArgs, bool tieStderrStdout = false)
            : m_pid(-1),
              m_error(0)
            {
                initFds();
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
                close(); // make sure everything is closed

                int in[2] = { -1, -1 };
                int out[2] = { -1, -1};
                int err[2] = { -1, -1 };

                if (-1 != pipe(in) && -1 != pipe(out) && -1 != pipe(err)) {
                    m_pid = fork();
                    if (m_pid > 0) {
                        // parent
                        ::close(in[0]);
                        ::close(out[1]);
                        ::close(err[1]);
                        m_pipes[PIPE_STDIN] = in[1];
                        m_pipes[PIPE_STDOUT] = out[0];
                        m_pipes[PIPE_STDERR] = err[0];
                        return true;
                    } else if (m_pid == 0) {
                        // child
                        ::close(in[1]);
                        ::close(out[0]);
                        ::close(err[0]);
                        dup2(in[0], 0); // child stdin to pipe stdin
                        dup2(out[1], 1); // child stdout to the pipe stdout
                        if (tieStderrStdout) {
                            dup2(out[1], 2); // child stderr to pipe stdout
                        } else {
                            dup2(err[1], 2); // child stderr to pipe stderr
                        }
                        if (-1 == execvp(cmdArgs[0], (char * const*)&cmdArgs[0])) {
                            exit(errno);
                        }
                        exit(0);
                    }
                }
                // either one of the pipe calls or the fork failed
                m_error = errno;
                close();
                CLOSE_FD_PIPE(in);
                CLOSE_FD_PIPE(out);
                CLOSE_FD_PIPE(err);
                return false;
            }

        void close()
            {
                if (-1 != m_pid) {
                    checkExit(true);
                }
                close(PIPE_STDIN);
                close(PIPE_STDOUT);
                close(PIPE_STDERR);
            }

        void close(int pipe)
            {
                if (PIPE_STDIN <= pipe && pipe <= PIPE_STDERR) {
                    if (-1 != m_pipes[pipe]) {
                        ::close(m_pipes[pipe]);
                        m_pipes[pipe] = -1;
                    }
                }
            }

        long read(char* buffer, long length, bool block = true)
            {
                if (-1 == m_pipes[PIPE_STDOUT]) {
                    m_error = EBADF;
                    return -1;
                }
                if (!block && !ready(false)) {
                    checkExit(false);
                    return -1;
                }
                ssize_t bytesRead = ::read(m_pipes[PIPE_STDOUT], buffer, length);
                if (-1 == bytesRead) {
                    m_error = errno;
                } else if (0 == bytesRead) {
                    m_error = FIO_EOF; // non-windows does not have an actual eof error code
                }
                // this cast is safe as the read length is a long
                return (long)bytesRead;
            }

        long readErr(char* buffer, long length, bool block = true)
            {
                if (-1 == m_pipes[PIPE_STDERR]) {
                    m_error = EBADF;
                    return -1;
                }
                if (!block && !ready(true)) {
                    checkExit(false);
                    return -1;
                }
                ssize_t bytesRead = ::read(m_pipes[PIPE_STDERR], buffer, length);
                if (-1 == bytesRead) {
                    m_error = errno;
                } else if (0 == bytesRead) {
                    m_error = FIO_EOF; // non-windows does not have an actual eof error code
                }
                // this cast is safe as the read length is a long
                return (long)bytesRead;
            }

        long write(char const* buffer, long length)
            {
                if (-1 == m_pipes[PIPE_STDIN]) {
                    m_error = EBADF;
                    return -1;
                }
                ssize_t bytesWritten = ::write(m_pipes[PIPE_STDIN], buffer, length);
                if (-1 == bytesWritten) {
                    m_error = errno;
                    return -1;
                }
                // note cast is safe as write length is long
                return (long)bytesWritten;
            }

        unsigned long error()
            {
                return m_error;
            }

        std::string errorAsString()
            {
                if (FIO_EOF == m_error) {
                    m_errorStr = "EOF";
                } else {
                    ::convertErrorToString(m_error, m_errorStr);
                }
                return m_errorStr;
            }

    protected:
        void initFds()
            {
                m_pipes[PIPE_STDIN] = -1;
                m_pipes[PIPE_STDOUT] = -1;
                m_pipes[PIPE_STDERR] = -1;
            }

        // possibly use this to make non-blocking io and just read
        int setNonBlocking(int fd)
            {
                int flags = 0;
#if defined(O_NONBLOCK)
                //  Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5.
                if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
                    flags = 0;
                }
                return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
                // use old way
                flags = 1;
                return ioctl(fd, FIOBIO, &flags);
#endif
            }

        bool ready(bool stdErr = false)
            {
                struct pollfd fds;
                fds.fd = (stdErr ? m_pipes[PIPE_STDERR] : m_pipes[PIPE_STDOUT]);
                fds.events = POLLIN | POLLPRI | POLLERR | POLLNVAL | POLLHUP;
                fds.revents = 0;
                int rc = poll(&fds, 1, 0);
                if (rc > 0) {
                    return true;
                }
                if (0 == rc) {
                    m_error = FIO_WOULD_BLOCK;
                } else if (rc < 0) {
                    m_error = errno;
                }
                return false;
            }

        void checkExit(bool wait)
            {
                if (-1 != m_pid) {
                    EXIT_CODE ec = 0;
                    if (m_pid == waitpid(m_pid, &ec, wait ? 0 : WNOHANG)) {
                        m_error = WEXITSTATUS(ec);
                    }
                }
            }

    private:
        FioPipeImp(FioPipeImp& fioPipeImp);
        FioPipeImp& operator=(FioPipeImp* fioPipeImp);

        int m_pid;

        int m_pipes[3];

        unsigned long m_error;

        std::string m_errorStr;
    };

} //namespace FIO

#endif // FIOPIPEIMP_H
