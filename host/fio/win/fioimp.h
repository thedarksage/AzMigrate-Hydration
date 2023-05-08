
///
/// \file win/fioimp.h
///
/// \brief windows implementation 
///

#ifndef FIOIMP_H
#define FIOIMP_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>
#include <iomanip>
#include <algorithm>

#include "errorexception.h"

#include "fiotypes.h"

#include "setpermissions.h"

namespace FIO {

    /// \brief windows implementation that wraps windows native IO functions (CreateFile, ReadFile, WriteFile, etc.)
    class FioImp {        
    public:

        /// \brief empty constructor
        FioImp()
            : m_handle(INVALID_HANDLE_VALUE),
              m_error(ERROR_SUCCESS)
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
            : m_error(ERROR_SUCCESS)
            {
                if (!open(name, mode)) {
                    throw ERROR_EXCEPTION << "could not open file: " << name
                                          << " using mode: 0x" << std::hex << std::setw(8) << std::setfill('0') << mode << std::dec
                                          << ": " << errorAsString();
                }
            }

        /// \brief wchar version to construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \exception throws ERROR_EXCEPTION on error        
        FioImp(wchar_t const* name,  ///< name fo file to open
               int mode              ///< open options to be used
               )
            : m_error(ERROR_SUCCESS)
            {
                if (!open(name, mode)) {
                    throw ERROR_EXCEPTION << "could not open file: " << name
                                          << " using mode: 0x" << std::hex << std::setw(8) << std::setfill('0') << mode << std::dec
                                          << ": " << errorAsString();
                }
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
                return (INVALID_HANDLE_VALUE != m_handle);
            }
                    
        /// \brief construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(char const* name,  ///< name of file
                  int mode           ///< options to be used
                  )
            {
                DWORD access;
                DWORD share;
                DWORD disp;
                DWORD flags;
                LPSECURITY_ATTRIBUTES attribute = securitylib::defaultSecurityAttributes();

                if (!translateMode(mode, access, share, disp, flags)) {
                    m_error = ERROR_INVALID_PARAMETER;
                    return false;
                }
                m_handle = CreateFileA(name, access, share, attribute, disp, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
                if (INVALID_HANDLE_VALUE == m_handle) {
                    m_error = GetLastError();
                    return false;
                }
                if (FIO_MODE_ON(FIO_NOATIME, mode)) {
                    FILETIME fileTime;
                    fileTime.dwLowDateTime = 0xFFFFFFFF;
                    fileTime.dwHighDateTime = 0xFFFFFFFF;
                    SetFileTime(m_handle, 0, &fileTime, &fileTime);
                }
                return true;
            }

        /// \brief wchar version to construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(wchar_t const* name,  ///< name of file
                  int mode              ///< options to be used
                  )
            {
                DWORD access;
                DWORD share;
                DWORD disp;
                DWORD flags;
                LPSECURITY_ATTRIBUTES attribute = securitylib::defaultSecurityAttributes();

                if (!translateMode(mode, access, share, disp, flags)) {
                    m_error = ERROR_INVALID_PARAMETER;
                    return false;
                }
                std::wstring wName(name);
                std::replace(wName.begin(), wName.end(), L'/', L'\\');
                m_handle = CreateFileW(wName.c_str(), access, share, attribute, disp, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
                if (INVALID_HANDLE_VALUE == m_handle) {
                    m_error = GetLastError();
                    return false;
                }

                return true;
            }

        /// \brief close the file
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool close()
            {                
                m_error = ERROR_SUCCESS;
                BOOL rc = TRUE;
                if (INVALID_HANDLE_VALUE != m_handle) {
                    rc = CloseHandle(m_handle);
                    m_handle = INVALID_HANDLE_VALUE;
                }

                return (rc == TRUE); // only need to quiet compiler because BOOL conversion to bool
            }

        /// \brief read data from the file
        ///
        /// \return long indicating the number of bytes read
        /// \li \c >0 : indicates success and is the number of bytes read (this can be less then requested length)
        /// \li \c =0 : indicates EOF
        /// \li \c <0 : indicates error. Use Fio::error() or FIo::errorAsString for the actual error
        long read(char* buffer,  ///< buffer to receive read data
                  long length    ///< number of bytes to read
                  )
            {
                if (INVALID_HANDLE_VALUE == m_handle) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }

                unsigned long bytesRead;
                if (!ReadFile(m_handle, buffer, length, &bytesRead, NULL)) {
                    DWORD err = GetLastError();
                    if (ERROR_HANDLE_EOF != err ) {
                        return -1;
                    }
                } else if (0 == bytesRead) {
                    m_error = ERROR_HANDLE_EOF;
                }

                // this cast is safe as the read length is a long
                return (long)bytesRead;
            }

        /// \brief write data to the file
        ///
        /// \return long indciating the number of bytes written
        /// \li \c >=0 : indicates success and is the number of bytes writen
        /// \li \c <0 : indicates error Use Fio::error() or FIo::errorAsString for the actual error
        long write(char const* buffer,  ///< holds data to be written. must be at least length bytes
                   long length          ///< number of bytes in buffer to write
                   )
            {
                if (INVALID_HANDLE_VALUE == m_handle) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }

                long bytesLeft = length;
                long totalBytes = 0;
                unsigned long bytesWritten;
                do {
                    bytesWritten = 0;
                    if (!WriteFile(m_handle, buffer, length, &bytesWritten, NULL)) {
                        m_error = GetLastError();
                        return -1;
                    }
                    // casts are safe since length is type long, bytesWritten will never exceed max long
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
        offset_t seek(long long offset,  ///< offet to seek to
                      int from           ///< where to start seek from
                      )
            {
                if (INVALID_HANDLE_VALUE == m_handle) {
                    m_error = ERROR_INVALID_HANDLE;
                    return -1;
                }

                LARGE_INTEGER seekOffset;
                seekOffset.QuadPart = offset;

                LARGE_INTEGER newOffset;
                newOffset.QuadPart = 0;

                if (!SetFilePointerEx(m_handle, seekOffset, &newOffset, from)) {
                    m_error = GetLastError();
                    return -1;
                }

                return newOffset.QuadPart;

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
                m_error = ERROR_SUCCESS;
            }

        /// \brief checks if currently at EOF
        ///
        /// \return bool
        /// \li \c true - indicates at EOF
        /// \li \c false - indicates not at EOF
        bool eof() const
            {
                return (ERROR_HANDLE_EOF == m_error);
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

        /// \brief flush file data to disk
        /// \return
        /// \li true: data flushed successfully
        /// \li false: flush data failed
        bool flushToDisk()
            {
                if (!FlushFileBuffers(m_handle)) {
                    m_error = GetLastError();
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
        bool translateMode(int mode,       ///< open options requested
                           DWORD& access,  ///< receives the access options based on mode
                           DWORD& share,   ///< receives share options based on mode
                           DWORD& disp,    ///< recives dispotion based on mode
                           DWORD& flags    ///< receives flag options based on mode
                           )
            {
                // access - can combine
                access = 0; // query info
                if (FIO_MODE_ON(FIO_READ, mode)) {
                    access |= GENERIC_READ;
                }
                if (FIO_MODE_ON(FIO_APPEND | FIO_WRITE, mode)) {
                    access |= FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE;
                } else if (FIO_MODE_ON(FIO_WRITE, mode)) {
                    access |= GENERIC_WRITE;
                }
                // TODO: for now to be consitant with non-windows require read and/or write
                // no query only support
                if (0 == access) {
                    m_error = ERROR_INVALID_PARAMETER;
                    return false;
                }

                // share - can combine these
                share = 0; // share none
                if (FIO_MODE_ON(FIO_SHARE_DELETE, mode)) {
                    share |= FILE_SHARE_DELETE;
                }
                if (FIO_MODE_ON(FIO_SHARE_READ, mode)) {
                    share |= FILE_SHARE_READ;
                }
                if (FIO_MODE_ON(FIO_SHARE_WRITE, mode)) {
                    share |= FILE_SHARE_WRITE;
                }

                // check if default sharing should be set
                if (0 == share && !FIO_MODE_ON(FIO_SHARE_NONE, mode)) {
                    share = (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);
                }

                // dsip only one and must be specified
                if (FIO_MODE_ON(FIO_OPEN | FIO_CREATE | FIO_TRUNCATE, mode)) {
                    disp = CREATE_ALWAYS;
                } else if (FIO_MODE_ON(FIO_OPEN | FIO_TRUNCATE, mode)) {
                    disp = TRUNCATE_EXISTING;
                } else if (FIO_MODE_ON(FIO_OPEN | FIO_CREATE, mode)) {
                    disp = OPEN_ALWAYS;
                } else if (FIO_MODE_ON(FIO_OPEN, mode)) {
                    disp = OPEN_EXISTING;
                } else if (FIO_MODE_ON(FIO_CREATE, mode)) {
                    disp = CREATE_NEW;
                } else {
                    // disp is required
                    m_error = ERROR_INVALID_PARAMETER;
                    return false;
                }

                // flags
                flags = FILE_ATTRIBUTE_NORMAL; // default ???
                if (FIO_MODE_ON(FIO_NO_BUFFERING, mode)) {
                    flags |= FILE_FLAG_NO_BUFFERING;
                }
                if (FIO_MODE_ON(FIO_WRITE_THROUGH, mode)) {
                    flags |= FILE_FLAG_WRITE_THROUGH;
                }
                if (FIO_MODE_ON(FIO_NONBLOCK, mode)) {
                    flags |= FILE_FLAG_OVERLAPPED;
                }

                return true;
            }

    private:
        // for now no copying of Fio
        FioImp(FioImp const & fio);       ///< prevent copying
        FioImp& operator=(FioImp* fio);   ///< prevent copying

        HANDLE m_handle; ///< file handle

        DWORD m_error; ///< last error

        std::string m_errorStr; ///< error code converted to a string
    };

} // namespace FIO

#endif // FIOIMP_H
