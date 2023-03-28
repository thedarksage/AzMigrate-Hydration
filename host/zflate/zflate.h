
///
/// \file zflate.h
///
/// \brief wrapper over zlib for various compress/uncompress operations
///

#ifndef ZFLATE_H
#define ZFLATE_H

#include <vector>
#include <string>
#include "zlib.h"

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/operations.hpp>

#include "errorexception.h"
#include "scopeguard.h"
#include "fio.h"

/// \brief return type from the zflate process api
///
/// \note
/// \li first - number of bytesProcessed
/// \li second - determines if more processing is needed. true: yes false: no
/// if second is true, need to use the continue memory to memory API when using memory to memory
/// (see the various process APIs for more details)
typedef std::pair<long, bool> zflateResult_t;


/// \brief abstract base class for the implementation
class ZflateImp {
public:
    /// \brief constructor
    explicit ZflateImp()
        : m_gzFile(NULL)
        { }

    /// \brief destructor
    virtual ~ZflateImp()
        {
            reset();
        }

    /// \brief continue memory to memory when inData was not fully processed
    ///
    ///  must be used when zflateResult_t::second is false until it is true
    ///
    /// e.g.
    ///
    /// do {
    ///     inFile.read(inBuf, inBufSize);
    ///     inSize = inFile.gcount();
    ///     moreDate = !inFile.eof();
    ///     zflateResult_t result = process(in, inSize, out, outSize, moreData);
    ///     if (result.first > 0) {
    ///         // do something with out
    ///     }
    ///     if (!result.second) {
    ///         do {
    ///             result = process(out, outSize, moreData);
    ///             if (result.first > 0) {
    ///                 // do something with out
    ///             }
    ///         } while (!result.second);
    ///     }
    /// } while (moreData);
    ///
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true if the remaining inData was fully process, false if more inData to process
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char* outData, ///< pointer to buffer to receive the processed data
                                   int outSize,   ///< size of the outData buffer
                                   bool moreData  ///< indicates if there will be more data to process true: yes, false: no
                                   ) = 0;

    /// \brief memory to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true of the remaining inData was fully process, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData, ///< pointer to data to process
                                   int inSize,         ///< size of data to be processed
                                   char* outData,      ///< pointer to buffer to received processed data
                                   int outSize,        ///< size of outData
                                   bool moreData       ///< indicate if there will be more data to process true: yes, false: no
                                   ) = 0;

    /// \brief memory to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData,         ///< pointer to data to process
                                   int inSize,                 ///< size of inData
                                   std::string const& outName, ///< name of file to write processed data
                                   bool moreData               ///< indicate if there will be more data to process true: yes, false: no
                                   ) = 0;

    /// \brief file to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    /// \li moreData - true if there is more data in inName file to process, false all data processed
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName, ///< name of file to process
                                   char* outData,             ///< pointer to buffer to hold processed data
                                   int outSize,               ///< size of outData
                                   bool deleteInFile,         ///< determines if the inName file should be deleted on succesful completion
                                   bool& moreData             ///< set to indicate if there is more inName file data to be processed
                                   ) = 0;

    /// \brief file to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName,   ///< name of file to process
                                   std::string const& outName,  ///< name of file to write processed data
                                   bool deleteInFile            ///< determines if the inName file should be deleted on succesful completion
                                   ) = 0;

    /// \brief reset internal variables
    void reset()
        {
            if (NULL != m_gzFile) {
                gzclose(m_gzFile);
                m_gzFile = NULL;
            }
            m_iFio.close();
            m_iFio.clear();
            m_oFio.close();
            m_oFio.clear();
        }

    /// \brief delete named file
    void deleteFile(std::string const& name)
        {
            reset();
            if (boost::filesystem::exists(name)) {
                boost::filesystem::remove(name);
            }
        }

protected:
    z_stream m_zStream;   ///< holds zlib state needed for memory to memory processing

    gzFile m_gzFile;      ///< holds gz file used when compressing/uncompressing to/from a file

    FIO::Fio m_iFio;      ///< for reading non compress file

    FIO::Fio m_oFio;      ///< for writing to non compress file

private:

};

/// \brief compress implementation using zlib APIs
class ZdeflateImp : public ZflateImp {
public:
    /// \brief constructor
    explicit ZdeflateImp()
        {
            m_zStream.zalloc = Z_NULL;
            m_zStream.zfree = Z_NULL;
            m_zStream.opaque = Z_NULL;
            int result = ::deflateInit2(&m_zStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15+16, 8, Z_DEFAULT_STRATEGY);
            if (Z_OK != result) {
                throw ERROR_EXCEPTION << "deflateInit2 failed: " << result;
            }
        }

    /// \brief destructor
    virtual ~ZdeflateImp()
        {
            ::deflateEnd(&m_zStream);
        }

    /// \brief continue compress memory to memory (see ZflateImp for more details)
    ///
    ///  must be used when zflateResult_t::second is false until it is true
    ///
    /// e.g.
    ///
    /// do {
    ///     inFile.read(inBuf, inBufSize);
    ///     inSize = inFile.gcount();
    ///     moreDate = !inFile.eof();
    ///     zflateResult_t result = process(in, inSize, out, outSize, moreData);
    ///     if (zflateResult_t.first > 0) {
    ///         // do something with out
    ///     }
    ///     if (!zflateResult_t.second) {
    ///         do {
    ///             result = process(out, outSize, moreData);
    ///             if (zflateResult_t.first > 0) {
    ///                 // do something with out
    ///             }
    ///         } while (!zflateResult_t.second);
    ///     }
    /// } while (moreData);
    ///
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true of the remaining inData was fully process, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char* outData, ///< pointer to buffer to receive the processed data
                                   int outSize,   ///< size of the outData buffer
                                   bool moreData  ///< indicates if there will be more data to process true: yes, false: no
                                   )
        {
            return process((char const*)m_zStream.next_in, m_zStream.avail_in, outData, outSize, moreData);
        }

    /// \brief compress memory to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true if the remaining inData was fully process, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData, ///< pointer to data to process
                                   int inSize,         ///< size of data to be processed
                                   char* outData,      ///< pointer to buffer to received processed data
                                   int outSize,        ///< size of outData
                                   bool moreData       ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            m_zStream.avail_in = inSize;
            m_zStream.next_in = (Bytef*)inData;

            int result;

            m_zStream.avail_out = outSize;
            m_zStream.next_out = (Bytef*)(outData);
            result = ::deflate(&m_zStream, (moreData ? Z_NO_FLUSH : Z_FINISH));
            switch (result) {
                case Z_OK:
                case Z_STREAM_END:
                case Z_BUF_ERROR:
                    break;
                    
                default:
                    throw ERROR_EXCEPTION << "defalate failed: " << result;
            }
            return zflateResult_t(outSize - m_zStream.avail_out, 0 == m_zStream.avail_out);
        }

    /// \brief compress memory to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData,         ///< pointer to data to process
                                   int inSize,                 ///< size of inData
                                   std::string const& outName, ///< name of file to write processed data
                                   bool moreData               ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            if (NULL == m_gzFile) {
                m_gzFile = gzopen(outName.c_str(), "wb6 ");
                if (NULL == m_gzFile) {
                    throw ERROR_EXCEPTION << "gzopen failed: " << errno;
                }
            }

            SCOPE_GUARD outNameGuard(MAKE_SCOPE_GUARD(boost::bind(&ZflateImp::deleteFile, this, outName)));

            if (gzwrite(m_gzFile, inData, static_cast<unsigned>(inSize)) != inSize) {
                throw ERROR_EXCEPTION << "gzwrite data to " << outName << " failed: " << errno;
            }

            outNameGuard.dismiss();

            if (!moreData) {
                reset();
            }

            return zflateResult_t(inSize, false);
        }

    /// \brief compress file to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    /// \li moreData - true if there is more data in inName file to process, false all data processed
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName, ///< name of file to process
                                   char* outData,             ///< pointer to buffer to hold processed data
                                   int outSize,               ///< size of outData
                                   bool deleteInFile,         ///< determines if the inName file should be deleted on succesful completion
                                   bool& moreData             ///< set to indicate if there is more inName file data to be processed
                                   )
        {
            if (!m_iFio.is_open()) {
                try {
                    m_iFio.open(inName.c_str(), FIO::FIO_READ_EXISTING);
                } catch (std::exception const& e) {
                    throw ERROR_EXCEPTION << "open in file " << inName << " failed: " << e.what();
                }
            }

            std::vector<char> in(outSize);
            // cast (int)in.size() is safe as in is set to outSize which is an int
            long bytesRead = m_iFio.read(&in[0], (int)in.size());
            if (bytesRead < 0) {                    
                throw ERROR_EXCEPTION << "read in file " << inName << " failed: " << m_iFio.errorAsString();
            }

            moreData = !m_iFio.eof();
            if (!moreData) {
                if (deleteInFile) {
                    deleteFile(inName);
                } else {
                    reset();
                }
            }

            return process(&in[0], bytesRead, outData, outSize, moreData);
        }

    /// \brief compress file to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName,   ///< name of file to process
                                   std::string const& outName,  ///< name of file to write processed data
                                   bool deleteInFile            ///< determines if the inName file should be deleted on succesful completion
                                   )
        {
            reset();

            ON_BLOCK_EXIT(boost::bind(&ZflateImp::reset, this));

            m_gzFile = gzopen(outName.c_str(), "wb6 ");
            if (NULL == m_gzFile) {
                throw ERROR_EXCEPTION << "gzopen failed: " << errno;
            }

            SCOPE_GUARD outNameGuard(MAKE_SCOPE_GUARD(boost::bind(&ZflateImp::deleteFile, this, outName)));

            try {
                m_iFio.open(inName.c_str(), FIO::FIO_READ_EXISTING);
            } catch (std::exception const& e) {
                throw ERROR_EXCEPTION << "open in file " << inName << " failed: " << e.what();
            }

            std::vector<char> inBuffer(1048576);

            long bytesToProcess;

            long bytesProcessed = 0;
            
            while (true) {
                // cast (int)inBuffer.size() is safe as inBuffer is 1mb
                bytesToProcess = m_iFio.read(&inBuffer[0], (int)inBuffer.size());
                if (bytesToProcess < 0) {
                    throw ERROR_EXCEPTION << "read file " << inName << " failed: " << errno;
                }

                if (0 == bytesToProcess) {
                    break;
                }

                if (gzwrite(m_gzFile, &inBuffer[0], static_cast<unsigned>(bytesToProcess)) != bytesToProcess) {
                    throw ERROR_EXCEPTION << "gzwrite data to " << outName << " failed: " << errno;
                }

                bytesProcessed += bytesToProcess;
            }

            outNameGuard.dismiss();

            if (boost::filesystem::exists(outName) && deleteInFile) {
                deleteFile(inName);
            }

            return zflateResult_t(bytesProcessed, false);
        }

protected:

private:

};

/// \brief uncompress implementation using zlib APIs
class ZinflateImp : public ZflateImp {
public:

    /// \brief constructor
    explicit ZinflateImp()
        {
            m_zStream.zalloc = Z_NULL;
            m_zStream.zfree = Z_NULL;
            m_zStream.opaque = Z_NULL;
            int result = ::inflateInit2(&m_zStream, 15+16);
            if (Z_OK != result) {
                throw ERROR_EXCEPTION << "inflateInit2 failed: " << result;
            }
        }

    /// \brief destructor
    virtual ~ZinflateImp()
        {
            ::inflateEnd(&m_zStream);
        }

    /// \brief continue uncompress memory to memory when inData was not fully processed
    ///
    ///  must be used when zflateResult_t::second is false until it is true
    ///
    /// e.g.
    ///
    /// do {
    ///     inFile.read(inBuf, inBufSize);
    ///     inSize = inFile.gcount();
    ///     moreDate = !inFile.eof();
    ///     zflateResult_t result = process(in, inSize, out, outSize, moreData);
    ///     if (zflateResult_t.first > 0) {
    ///         // do something with out
    ///     }
    ///     if (!zflateResult_t.second) {
    ///         do {
    ///             result = process(out, outSize, moreData);
    ///             if (zflateResult_t.first > 0) {
    ///                 // do something with out
    ///             }
    ///         } while (!zflateResult_t.second);
    ///     }
    /// } while (moreData);
    ///
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true of the remaining inData was fully process, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char* outData, ///< pointer to buffer to receive the processed data
                                   int outSize,   ///< size of the outData buffer
                                   bool moreData  ///< indicates if there will be more data to process true: yes, false: no
                                   )
        {
            return process((char const*)m_zStream.next_in, m_zStream.avail_in, outData, outSize, moreData);
        }

    /// \brief uncompress memory to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true if the remaining inData was fully processed, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData, ///< pointer to data to process
                                   int inSize,         ///< size of data to be processed
                                   char* outData,      ///< pointer to buffer to received processed data
                                   int outSize,        ///< size of outData
                                   bool moreData       ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            m_zStream.avail_in = inSize;
            m_zStream.next_in = (Bytef*)inData;

            int result;

            m_zStream.avail_out = outSize;
            m_zStream.next_out = (Bytef*)(outData);
            result = ::inflate(&m_zStream, Z_NO_FLUSH);
            switch (result) {
                case Z_OK:
                case Z_STREAM_END:
                case Z_BUF_ERROR:
                    break;

                default:
                    throw ERROR_EXCEPTION << "inflate failed: " << result;
                    break;
            }
            return zflateResult_t(outSize - m_zStream.avail_out, 0 == m_zStream.avail_out);
        }

    /// \brief uncompress memory to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData,         ///< pointer to data to process
                                   int inSize,                 ///< size of inData
                                   std::string const& outName, ///< name of file to write processed data
                                   bool moreData               ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            std::vector<char> outData(inSize);

            zflateResult_t result(0, true);

            if (!m_oFio.is_open()) {
                try {
                    m_oFio.open(outName.c_str(), FIO::FIO_OVERWRITE);
                } catch (std::exception const& e) {
                    throw ERROR_EXCEPTION << "open in file " << outName << " failed: " << e.what();
                }
            }

            SCOPE_GUARD outNameGuard(MAKE_SCOPE_GUARD(boost::bind(&ZflateImp::deleteFile, this, outName)));

            // cast (int)outData.size() is safe as outData was set to inSize which is int
            result = process(inData, inSize, &outData[0], (int)outData.size(), moreData);
            if (m_oFio.write(&outData[0], result.first) < 0) {
                throw ERROR_EXCEPTION << "write to file " << outName << " failed: " << m_oFio.errorAsString();
            }
            if (!result.second) {
                do {
                    // cast (int)outData.size() is safe as outData was set to inSize which is int
                    result = process(&outData[0], (int)outData.size(), moreData);
                    if (m_oFio.write(&outData[0], result.first) < 0) {
                        throw ERROR_EXCEPTION << "write to file " << outName << " failed: " << m_oFio.errorAsString();
                    }
                } while (!result.second);
            }

            outNameGuard.dismiss();

            if (!moreData) {
                reset();
            }

            return zflateResult_t(inSize, false);
        }

    /// \brief uncompress file to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    /// \li moreData - true if there is more data in inName file to process, false all data processed
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName, ///< name of file to process
                                   char* outData,             ///< pointer to buffer to hold processed data
                                   int outSize,               ///< size of outData
                                   bool deleteInFile,         ///< determines if the inName file should be deleted on succesful completion
                                   bool& moreData             ///< set to indicate if there is more inName file data to be processed
                                   )
        {
            if (NULL == m_gzFile) {
                m_gzFile = gzopen(inName.c_str(), "rb6 ");
                if (NULL == m_gzFile) {
                    throw ERROR_EXCEPTION << "gzopen failed: " << errno;
                }
            }

            if (gzread(m_gzFile, outData, static_cast<unsigned>(outSize)) != outSize) {
                throw ERROR_EXCEPTION << "gzwrite data to " << inName << " failed: " << errno;
            }

            moreData = (0 == gzeof(m_gzFile));
            if (!moreData) {
                if (deleteInFile) {
                    deleteFile(inName);
                } else {
                    reset();
                }
            }

            return zflateResult_t(outSize, false);
        }

    /// \brief uncompress file to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName,   ///< name of file to process
                                   std::string const& outName,  ///< name of file to write processed data
                                   bool deleteInFile            ///< determines if the inName file should be deleted on succesful completion
                                   )
        {
            reset();

            ON_BLOCK_EXIT(boost::bind(&ZflateImp::reset, this));

            m_gzFile = gzopen(inName.c_str(), "rb6 ");
            if (NULL == m_gzFile) {
                throw ERROR_EXCEPTION << "gzopen failed: " << errno;
            }

            try {
                m_oFio.open(outName.c_str(), FIO::FIO_OVERWRITE);
            } catch (std::exception const& e) {
                throw ERROR_EXCEPTION << "open in file " << outName << " failed: " << e.what();
            }

            SCOPE_GUARD outNameGuard(MAKE_SCOPE_GUARD(boost::bind(&ZflateImp::deleteFile, this, outName)));

            std::vector<char> inBuffer(1048576);

            long bytesToProcess;

            long bytesProcessed = 0;

            while (true) {
                bytesToProcess = gzread(m_gzFile, &inBuffer[0], inBuffer.size());
                if (-1 == bytesToProcess) {
                    throw ERROR_EXCEPTION << "gzread file " << inName << " failed: " << errno;
                }

                if (0 == bytesToProcess) {
                    break; // eof
                }

                if (m_oFio.write(&inBuffer[0], bytesToProcess) < 0) {
                    throw ERROR_EXCEPTION << "write data to " << outName << " failed: " << m_oFio.errorAsString();
                }

                bytesProcessed += bytesToProcess;
            }

            outNameGuard.dismiss();

            if (boost::filesystem::exists(outName) && deleteInFile) {
                deleteFile(inName);
            }

            return zflateResult_t(bytesProcessed, false);
        }

protected:

private:

};

/// \brief class that should be used when compress/uncompress data is needed
class Zflate {
public:

    /// \brief mode that should be used 
    typedef enum {COMPRESS, UNCOMPRESS } mode_t;

    /// \brief constructor
    ///
    /// \exception
    /// throws ERROR_EXCEPTION is mode not COMRESS nor UNCOMPRESS
    explicit Zflate(mode_t mode) ///< mode to use COMPRESS or UNCOMPRESS
        {
            if (UNCOMPRESS == mode) {
                m_zflate.reset(new ZinflateImp);
            } else if (COMPRESS == mode) {
                m_zflate.reset(new ZdeflateImp);
            } else {
                throw ERROR_EXCEPTION << "unknown mode :" << uncompress;
            }
        }

    /// \brief destructor
    ~Zflate()
        { }

    /// \brief continue memory to memory when inData was not fully processed
    ///
    ///  must be used when zflateResult_t::second is false until it is true
    ///
    /// e.g.
    ///
    /// do {
    ///     inFile.read(inBuf, inBufSize);
    ///     inSize = inFile.gcount();
    ///     moreDate = !inFile.eof();
    ///     zflateResult_t result = process(in, inSize, out, outSize, moreData);
    ///     if (result.first > 0) {
    ///         // do something with out
    ///     }
    ///     if (!result.second) {
    ///         do {
    ///             result = process(out, outSize, moreData);
    ///             if (result.first > 0) {
    ///                 // do something with out
    ///             }
    ///         } while (!result.second);
    ///     }
    /// } while (moreData);
    ///
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true if the remaining inData was fully process, false if more inData to process
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char* outData, ///< pointer to buffer to receive the processed data
                                   int outSize,   ///< size of the outData buffer
                                   bool moreData  ///< indicates if there will be more data to process true: yes, false: no
                                   )
        {
            return m_zflate->process(outData,  outSize,  moreData);
        }

    /// \brief memory to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - true of the remaining inData was fully process, false if more inData to process
    ///                              note inData is from previous process call that passed in inData
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData, ///< pointer to data to process
                                   int inSize,         ///< size of data to be processed
                                   char* outData,      ///< pointer to buffer to received processed data
                                   int outSize,        ///< size of outData
                                   bool moreData       ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            return m_zflate->process(inData, inSize, outData,  outSize,  moreData);
        }

    /// \brief memory to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(char const* inData,         ///< pointer to data to process
                                   int inSize,                 ///< size of inData
                                   std::string const& outName, ///< name of file to write processed data
                                   bool moreData               ///< indicate if there will be more data to process true: yes, false: no
                                   )
        {
            return m_zflate->process(inData, inSize, outName, moreData);
        }

    /// \brief file to memory
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    /// \li moreData - true if there is more data in inName file to process, false all data processed
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName, ///< name of file to process
                                   char* outData,             ///< pointer to buffer to hold processed data
                                   int outSize,               ///< size of outData
                                   bool deleteInFile,         ///< determines if the inName file should be deleted on succesful completion
                                   bool& moreData             ///< set to indicate if there is more inName file data to be processed
                                   )
        {
            return m_zflate->process(inName, outData,  outSize,  deleteInFile, moreData);
        }

    /// \brief file to file
    ///
    /// \return zflateResult_t
    /// \li zflateResult_t::first - set to bytes processed
    /// \li zflateResult_t::second - will always be true
    ///
    /// \exception
    /// throws ERROR_EXCEPTION on error
    virtual zflateResult_t process(std::string const& inName,   ///< name of file to process
                                   std::string const& outName,  ///< name of file to write processed data
                                   bool deleteInFile            ///< determines if the inName file should be deleted on succesful completion
                                   )
        {
            return m_zflate->process(inName, outName, deleteInFile);
        }


protected:
    boost::shared_ptr<ZflateImp> m_zflate;

private:

};

#endif // ZFLATE_H
