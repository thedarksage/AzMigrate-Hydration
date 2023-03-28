
///
/// \file unix/fioimp.h
///
/// \brief unix implementation
///

#ifndef FIOIMP_H
#define FIOIMP_H

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <iomanip>
#include <cstring>
#include <vector>

#include "errorexception.h"

#include "fiotypes.h"
#include "converterrortostringminor.h"

namespace FIO {

    /// \brief unix implementation that wraps unix low-level C IO functions (open, read, write, etc.)
    class FioImp {
    public:

        /// \brief empty constructor
        FioImp()
            : m_fd(-1),
              m_error(0)
            {
            }

        /// \brief construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \exception throws ERROR_EXCEPTION on error
        FioImp(char const* name,  ///< name of file to open
               int mode           ///< open options to be used
               )
            : m_error(0)
            {
                if (!open(name, mode)) {
                    throw ERROR_EXCEPTION << "could not open file: " << name
                                          << " using mode: 0x" << std::hex << std::setw(8) << std::setfill('0') << mode << std::dec
                                          << ": " << errorAsString();
                }
            }

        /// \brief wchar version to construct and open the file
        ///
        /// currently not implemented and will alwyas throw an error
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \exception throws ERROR_EXCEPTION on error
        FioImp(wchar_t const* name,  ///< name of file to open
               int mode              ///< open options to be used
               )
            : m_error(EINVAL)
            {
                throw ERROR_EXCEPTION << "wchar_t version not implemented";
            }

        /// \brief destructor closes file if open
        ~FioImp()
            {
                close();
            }

        /// \brief checks if file currently open
        ///
        /// \return bool
        /// \li \c true: file is open
        /// \li \c false: file not open
        bool is_open()
            {
                return (-1 != m_fd);
            }

        /// \brief construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(char const* name,   ///< name of file
                  int mode            ///< options to be used
                  )
            {
                int flags;
                mode_t openMode;

                if (!translateMode(mode, flags, openMode, name)) {
                    return false;
                }

                m_fd = ::open(name, flags,  openMode);
                if (-1 == m_fd) {
                    m_error = errno;
                    return false;
                }

                return true;
            }

        /// \brief wchar construct and open the file
        ///
        /// currently not implemented and will always return false
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(wchar_t const* name,  ///< name of tile
                  int mode              ///< options to be used
                  )
            {
                // not supported on unux
                m_error = EINVAL;
                return false;
            }

        /// \brief close the file
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool close()
            {
                m_error = 0;
                int rc = 0;
                if (-1 != m_fd) {
                    rc = ::close(m_fd);
                    m_fd = -1;
                }

                return (0 == rc);
            }

        /// \brief read data from the file
        ///
        /// \return long indicating the number of bytes read
        /// \li \c >0 : indicates success and is the number of bytes read (this can be less then requested length)
        /// \li \c =0 : indicates EOF
        /// \li \c <0 : indicates error. Use Fio::error() or FIo::errorAsString for the actual error
        long read(char* buffer,  ///< receives data read. Must be at least length bytes
                  long length    ///< number of bytes to read
                  )
            {
                if (-1 == m_fd) {
                    m_error = EBADF;
                    return -1;
                }

                ssize_t bytesRead = ::read(m_fd, buffer, length);
                if (-1 == bytesRead) {
                    m_error = errno;
                } else if (0 == bytesRead) {
                    m_error = FIO_EOF; // non-windows does not have an actual eof error code
                }

                // this cast is safe as the read length is a long
                return (long)bytesRead;
            }

        /// \brief write data to the file
        ///
        /// \return long indciating the number of bytes written
        /// \li \c >=0 : indicates success and is the number of bytes writen
        /// \li \c <0 : indicates error Use Fio::error() or FIo::errorAsString for the actual error
        long write(char const* buffer,  ///< data to be written. Must be at least length bytes
                   long length          ///< number of bytes in buffer to be written
                   )
            {
                if (-1 == m_fd) {
                    m_error = EBADF;
                    return -1;
                }

                long bytesLeft = length;
                long totalBytes = 0;                
                do {
                    ssize_t bytesWritten = ::write(m_fd, buffer, length);                                    
                    if (-1 == bytesWritten) {
                        m_error = errno;
                        return -1;
                    }
                    // casts are safe since length is long, bytesWritten will never exceed max long
                    totalBytes += (long)bytesWritten;
                    bytesLeft -= (long)bytesWritten; 
                } while (bytesLeft > 0);
                return totalBytes;
            }

        /// \brief seek to an offset in the file
        ///
        /// \return Fio::offset_t indicating the new offset
        /// \li \c >=0 - indicates success and is the value of the new offset
        /// \li \c <0 - indicates error Use Fio::error() or FIo::errorAsString for the actual error
        offset_t seek(offset_t offset,  ///< offset to seek to
                      int from          ///< where to start seek from
                      )
            {
                if (-1 == m_fd) {
                    m_error = EBADF;
                    return -1;
                }
#ifdef off64_t
                offset_t newOffset = ::lseek64(m_fd, offset, from);
#else
                offset_t newOffset = ::lseek(m_fd, offset, from);
#endif
                if (-1 == newOffset) {
                    m_error = errno;
                    return -1;
                }

                return newOffset;
            }

        /// \brief get the current offset in the file
        ///
        /// \return Fio::offset_t indicating the current offset
        /// \li \c >=0 - indicates success and is the value of the currentoffset
        /// \li \c <0 - indicates error Use Fio::error() or FIo::errorAsString for the actual error
        offset_t tell()
            {
                return seek(0, FIO_CUR);
            }

        /// \brief clear internal error state
        void clear()
            {
                m_error = 0;
            }

        /// \brief checks if currently at EOF
        ///
        /// \return bool
        /// \li \c true - indicates at EOF
        /// \li \c false - indicates not at EOF
        bool eof() const
            {
                return (-1 == m_error);
            }

        /// \brief get the last error
        ///
        ///
        /// \return unsigned long
        /// \li currently this will be the platform's specific error. So unless checking for
        /// a specific error code, probably should use errorAsString
        unsigned long error()
            {
                return m_error;
            }

        /// \brief get the last error converted to the systems error message. Does not include the actual error code.
        ///
        /// \return std::string holding the systems error message of the last error
        std::string errorAsString()
            {
                if (FIO_EOF == m_error) {
                    m_errorStr = "EOF";
                } else {
                    ::convertErrorToString(m_error, m_errorStr);
                }
                return m_errorStr;
            }

        /// \brief flush file data to disk
        /// \return
        /// \li true: data flushed successfully
        /// \li false: flush data failed
        bool flushToDisk()
            {
                if (-1 == fsync(m_fd)) {
                    m_error = errno;
                    return false;
                }
                return true;
            }


    protected:
        /// \brief translates the Fio open options to windows
        ///
        /// \return bool
        /// \li \c true - indicates success
        /// \li \c false - indicates error (missing required option)
        bool translateMode(int mode,          ///< open mode options requested
                           int& flags,        ///< flags based on mode
                           mode_t& openMode,  ///< default permissions
                           char const* name   ///< name of file being opened
                           )
            {
                // flags
                flags = 0;
                // need one of these
                if (FIO_MODE_ON(FIO_READ | FIO_WRITE, mode)) {
                    flags |= O_RDWR;
                } else if (FIO_MODE_ON(FIO_READ, mode)) {
                    flags |= O_RDONLY;
                } else if (FIO_MODE_ON(FIO_WRITE, mode)) {
                    flags |= O_WRONLY;
                } else {
                    m_error = EINVAL;
                    return false;
                }

                if (FIO_MODE_ON(FIO_APPEND, mode)) {
                    flags |= O_APPEND;
                }

                // to be consistant with windows require one of these
                if (FIO_MODE_ON(FIO_OPEN | FIO_CREATE | FIO_TRUNCATE, mode)) {
                    flags |= O_CREAT | O_TRUNC;
                } else if (FIO_MODE_ON(FIO_OPEN | FIO_TRUNCATE, mode)) {
                    flags |= O_TRUNC;
                } else if (FIO_MODE_ON(FIO_CREATE | FIO_OPEN, mode)) {
                    flags |= O_CREAT;
                } else if (FIO_MODE_ON(FIO_CREATE, mode)) {
                    flags |= O_CREAT | O_EXCL;
                } else if (FIO_MODE_ON(FIO_OPEN, mode)) {
                    // nothing needs to be added as this is default
                } else {
                    m_error = EINVAL;
                    return false;
                }

                // optional
#ifdef O_DIRECT
                if (FIO_MODE_ON(FIO_NO_BUFFERING, mode) || FIO_MODE_ON(FIO_WRITE_THROUGH, mode)) {
                    flags |= O_DIRECT;
                }
#endif
                if (FIO_MODE_ON(FIO_NONBLOCK, mode)) {
                    flags |= O_NONBLOCK;
                }
#ifdef O_LARGEFILE
                if (FIO_MODE_ON(FIO_LARGEFILE, mode)) {
                    flags |= O_LARGEFILE;
                }
#endif
#ifdef O_NOATIME
                if (FIO_MODE_ON(FIO_NOATIME, mode)) {
                    flags |= O_NOATIME;
                }
#endif
                // openMode
                openMode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

                return true;
            }

        bool fileExists(char const* name)
            {
                struct stat statInfo;
                if (-1 == stat(name, &statInfo)) {
                    m_error = errno;
                    return false;
                }
                return true;
            }

    private:
        FioImp(FioImp const & fio);      ///< prevent copying
        FioImp& operator=(FioImp* fio);  ///< prevent copying

        int m_fd; ///< file handle

        unsigned long m_error; ///< last error

        std::string m_errorStr;  ///< error code converted to a string
    };

} //namespace FIO

#endif // FIOIMP_H
