
///
/// \file fio.h
///
/// \brief wrapper over the system native io functions
///

#ifndef FIO_H
#define FIO_H

#include <string>
#include <limits>

#include "fiotypes.h"
#include "fioimp.h"

/// \brief FIO namespace
namespace FIO {

    /// \brief wraps the system native io functions
    /// 
    /// Windows uses native APIs (CreateFile, ReadFile, WriteFile, etc.).
    /// 
    /// UNIX/Linux use the low-level C I/O APIS (open, read, write, etc.).
    /// 
    /// alwyas uses binary mode (there is no support for text mode)
    ///
    /// Not all available features for a given platform are currently supported
    /// 
    /// See fiotypes.h for details on the vairous open options that are supported.
    /// 
    /// For the most part, functionality is the same across all platforms. The areas where functionality
    /// is different between windows and non-windows are
    ///
    /// \li required options - windows and UNIX/Linux have a different set of required options.
    ///     To be consitent across all platforms, all the required options for all platforms are required.
    ///     I.E. on windows you need one of the create/open modes and on UNIX/Linux, you need one of the
    ///     access modes. As such you need to provide at least one of each on all platforms. 
    ///
    /// \li file attributes - defaults are used and can not be specified:
    ///     \n\n
    ///     Windows: uses FILE_ATTRIBUTE_NORMAL
    ///     \n\n
    ///     UNIX: none
    ///
    /// \li permissions/security - defaults are used and can not be specified:
    ///    \n\n
    ///    Windows: uses the system default security
    ///    \n\n
    ///    UNIX: uses S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH with the normal behavior of umask
    ///
    /// \li file sharing on open:\n\n
    ///     Windows: the FIO_SHARE options match the windows FILE_SHARE options. By default if no share options are
    ///              specified it will share all. To prevent sharing on windows use FIO_SHARE_NONE
    ///     \n\n
    ///     UNIX: for the most part they are ignored and shares all.
    ///           The one exception is when FIO_SHARE_NONE is specifeid, then O_EXCL will be used (with associated rules enforced).
    ///     
    /// Refer to the appropriate platform's documentation for complete details.
    class Fio {

    public:
        /// \brief empty constructor
        Fio()
            {
            }

        /// \brief construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \exception throws ERROR_EXCEPTION on error        
        Fio(char const* name,  ///< name of file to open
            int mode           ///< open options to use
            )
            : m_fioImp(name, mode)
            {
            }

        /// \brief wchar version to construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \exception throws ERROR_EXCEPTION on error        
        Fio(wchar_t const* name,  ///< name of file to open
            int mode              ///< open options to use
            )
            : m_fioImp(name, mode)
            {
            }

        /// \brief destructor closes file if open
        ~Fio()
            {
            }

        /// \brief checks if file currently open
        ///
        /// \return bool
        /// \li \c true: file is open
        /// \li \c false: file not open
        bool is_open()
            {
                return m_fioImp.is_open();
            }

        /// \brief construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(char const* name, ///< name of file 
                  int mode          ///< options to use
                  )            
            {
                return m_fioImp.open(name, mode);
            }

        /// \brief wchar version to construct and open the file
        ///
        /// \note
        /// \li file is always opened as binary. there is no support for text mode
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool open(wchar_t const* name, ///< name of file
                  int mode             ///< options to use
                  )
            {
                return m_fioImp.open(name, mode);
            }

        /// \brief close the file
        ///
        /// \return  bool
        /// \li \c true on success
        /// \li \c false on error. Use Fio::error or Fio::errorAsString to get the actual error
        bool close()
            {
                return m_fioImp.close();
            }

        /// \brief read data from the file until length bytes read, eof, or error
        ///
        /// \return long indicating the number of bytes read
        /// \li \c >0 : indicates success and is the number of bytes read
        ///             (can be less then requested length, if less, then indicates eof was reached while reading)
        /// \li \c =0 : indicates EOF
        /// \li \c <0 : indicates error. Use Fio::error() or FIo::errorAsString for the actual error
        long read(char* buffer, ///< buffer to receive data read
                  long length   ///< number of bytes to read
                  )
            {
                if (0 == length) {
                    return 0;
                }
                long bytesRead = 0;
                int bytes = 0;
                do {
                    bytes = m_fioImp.read(buffer + bytesRead, length - bytesRead);
                    if (bytes < 0) {
                        return -1;
                    }
                    bytesRead += bytes;
                } while (bytes > 0 && bytesRead < length);
                return bytesRead;
            }

        /// \brief write data to the file
        ///
        /// \return long indciating the number of bytes written
        /// \li \c >=0 : indicates success and is the number of bytes writen
        /// \li \c <0 : indicates error Use Fio::error() or FIo::errorAsString for the actual error
        long write(char const* buffer, ///< holds to to be written must be at least length bytes
                   long length         ///< number of bytes in buffer to be written
                   )
            {
                if (0 == length) {
                    return 0;
                }
                return m_fioImp.write(buffer, length);
            }

        /// \brief reads until the buffer is full, error or EOF
        ///
        /// \return long long indicating number of bytes read
        /// \li \c >0 : indicates success and is the number of bytes read (will be less then requested length only if EOF)
        /// \li \c =0 : indicates EOF
        /// \li \c <0 : indicates error Use Fio::error() or FIo::errorAsString for the actual error
        long long fullRead(char* buffer,     ///< buffer to receive the data. must be at least length bytes
                           long long length  ///< number of bytes to read
                           )
            {
                if (0 == length) {
                    return 0;
                }
                
                long bytesRead = 0;
                long long totalBytesRead = 0;
                do {
                    bytesRead = m_fioImp.read(buffer + totalBytesRead,
                                              (long)((std::numeric_limits<long>::max)() < (length - totalBytesRead)
                                                     ? (std::numeric_limits<long>::max)() : length - totalBytesRead));
                    if (bytesRead < 0) {
                        return -1;
                    } else if (0 == bytesRead) {
                        return 0;
                    }
                    totalBytesRead += bytesRead;
                } while (totalBytesRead < length);
                return totalBytesRead;
            }

        /// \brief seek to an offset in the file
        ///
        /// \return Fio::offset_t indicating the new offset
        /// \li \c >=0 - indicates success and is the value of the new offset
        /// \li \c <0 - indicates error Use Fio::error() or FIo::errorAsString for the actual error
        offset_t seek(offset_t offset,  ///< offset to seek to
                      int from          ///< where to start the seek from
                      )
            {
                return m_fioImp.seek(offset, from);
            }

        /// \brief get the current offset in the file
        ///
        /// \return Fio::offset_t indicating the current offset
        /// \li \c >=0 - indicates success and is the value of the currentoffset
        /// \li \c <0 - indicates error Use Fio::error() or FIo::errorAsString for the actual error
        offset_t tell()
            {
                return m_fioImp.tell();
            }

        /// \brief clear internal error state
        void clear()
            {
                m_fioImp.clear();
            }
        
        /// \brief checks if currently at EOF
        ///
        /// \return bool
        /// \li \c true - indicates at EOF
        /// \li \c false - indicates not at EOF
        bool eof() const
            {
                return m_fioImp.eof();
            }

        /// \brief check if in good state (no error nor eof)
        ///
        /// \return
        /// \li true: no errors and not at eof
        /// \li false: otherwise
        bool good()
            {
                return (FIO_SUCCESS == m_fioImp.error());
            }

        /// \brief checks if in bad state (error but no eof)
        ///
        /// \return
        /// \li true: if error
        /// \li false: otherwise
        bool bad()
            {
                return (FIO_SUCCESS != m_fioImp.error() && !eof());
            }

        /// \brief checks if in fail state (error or eof)
        ///
        /// \return
        /// \li true: if error or at eof
        /// \li false: otherwise
        bool fail()
            {
                return !good();
            }
        
        /// \brief get the last error
        ///
        ///
        /// \return unsigned long
        /// \li currently this will be the platform's specific error. So unless checking for
        /// a specific error code, probably should use errorAsString
        unsigned long error()
            {
                return m_fioImp.error();
            }
        
        /// \brief get the last error converted to the systems error message. Does not include the actual error code.
        ///
        /// \return std::string holding the systems error message of the last error
        std::string errorAsString()
            {
                return m_fioImp.errorAsString();
            }

        /// \brief flush file data to disk
        /// \return
        /// \li true: data flushed successfully
        /// \li false: flush data failed
        bool flushToDisk()
            {
                return m_fioImp.flushToDisk();
            }
        
    protected:

    private:
        Fio(Fio const & fio);        ///< prevent copying
        Fio& operator=(Fio* fio);    ///< prevent copying

        FioImp m_fioImp; ///< implementation that will perfrom the actual IO
    };
    
} //namespace FIO

#endif // FIO_H
