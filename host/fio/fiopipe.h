
///
/// \file fiopipe.h
///
/// \brief wrapper over the system native pipe functions
///

#ifndef FIOPIPE_H
#define FIOPIPE_H

#include <string>
#include <limits>
#include <vector>

namespace FIO {
    typedef std::vector<char const*> cmdArgs_t; // the last entry must be a NULL

    enum PIPE {
        PIPE_STDIN = 0,
        PIPE_STDOUT = 1,
        PIPE_STDERR = 2
    };
}

#include "fiopipeimp.h"

namespace FIO {

    /// \brief wraps the system native pipe functions
    /// 
    /// For the most part, functionality is the same across all platforms.
    class FioPipe {
    public:
        
        FioPipe()
            : m_imp()
            {}

        explicit FioPipe(cmdArgs_t& cmdArgs, bool tieStderrStdout = false)
            : m_imp(cmdArgs, tieStderrStdout)
            {
            }

        bool create(cmdArgs_t& cmdArgs, bool tieStderrStdout = false)
            {
                return m_imp.create(cmdArgs, tieStderrStdout);
            }

        void close()
            {
                return m_imp.close();
            }
        void close(int pipe)
            {
                return m_imp.close(pipe);
            }
        
        long read(char* buffer, long length, bool block = true)
            {
                return m_imp.read(buffer, length, block);
            }

        long write(char const* buffer, long length)
            {
                return m_imp.write(buffer, length);
            }

        long readErr(char* buffer, long length, bool block = true)
            {
                return m_imp.readErr(buffer, length, block);
            }

        unsigned long error()
            {
                return m_imp.error();
            }

        std::string errorAsString()
            {
                return m_imp.errorAsString();
            }

    protected:

    private:
        FioPipe(FioPipe& fioPipe);
        FioPipe& operator=(FioPipe* fioPipe);
        FioPipeImp m_imp;
    };

} //namespace FIO

#endif // FIOPIPE_H
