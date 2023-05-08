#include "bitmap.h"
#include "basicio.h"
#include "logger.h"
#include "localconfigurator.h"
#include "inm_md5.h"
#include <fstream>
#include "file.h"
#include <sstream>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "theconfigurator.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <sstream>
#ifdef WIN32
#include<stdlib.h>
#endif

//#define BITMAP_DUMP_ACTIVE 1
const SV_UINT HashSize = 16 ;

PersistentBitMap::PersistentBitMap(const string &fileName, const SV_ULONG  bitMapGranularity)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    m_bitMapHeader.m_bitMapGranularity = bitMapGranularity;
    m_bitMapHeader.m_bytesProcessed = 0 ;
    m_bitMapHeader.m_bytesNotProcessed = 0 ;
    this->setFileName(fileName);
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}
//getters
string PersistentBitMap::getFileName()
{
    return this->m_fileName;
}

//setters
void PersistentBitMap::setFileName( string fileName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    m_fileName = fileName;
    DebugPrintf(SV_LOG_DEBUG,  "FileName is %s\n", fileName.c_str());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}


void PersistentBitMap::setBitMapGranularity(SV_ULONG bitMapGranularity)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    m_bitMapHeader.m_bitMapGranularity = bitMapGranularity;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void PersistentBitMap::setBytesProcessed( SV_ULONGLONG bytesProcessed )
{
    stringstream msg;
    msg << "Setting Bytes Processed as " << bytesProcessed;
    DebugPrintf(SV_LOG_DEBUG, msg.str().c_str() );
    m_bitMapHeader.m_bytesProcessed = bytesProcessed ;
}

void PersistentBitMap::setBytesNotProcessed( SV_ULONGLONG bytesNotProcessed )
{
    stringstream msg;
    msg<<"Setting Bytes Not Processed as " <<  bytesNotProcessed;
    DebugPrintf(SV_LOG_DEBUG, msg.str().c_str() );
    m_bitMapHeader.m_bytesNotProcessed = bytesNotProcessed ;
}

SV_ULONGLONG PersistentBitMap::getBytesProcessed() const
{
    stringstream msg;
    DebugPrintf(SV_LOG_DEBUG, msg.str().c_str() );
    return m_bitMapHeader.m_bytesProcessed ;
}
SV_ULONGLONG PersistentBitMap::getBytesNotProcessed(  ) const
{
    stringstream msg;
    return m_bitMapHeader.m_bytesNotProcessed ;
}

PersistentBitMap::~PersistentBitMap()
{

}

SVERROR PersistentBitMap::createBitMapFileDataFromMemory(boost::shared_array <char> &buffer, SV_UINT &Size, const std::string& bitMapData )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",  __FUNCTION__ );
    stringstream bitMapDataStream ;
    unsigned char hash[HashSize];

    INM_MD5_CTX ctx;

    bitMapDataStream<< '\n' <<
        getBitMapGranularity()<< '\n' <<
        getBytesProcessed() << '\n' <<
        getBytesNotProcessed() << '\n' << bitMapData;

    DebugPrintf(SV_LOG_DEBUG, "The BitMap string written to %s is %s\n", this->getFileName().c_str(), bitMapDataStream.str().c_str());

    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)bitMapDataStream.str().c_str(), bitMapDataStream.str().length() );
    INM_MD5Final(hash, &ctx);

    INM_SAFE_ARITHMETIC(Size = HashSize + InmSafeInt<size_t>::Type(bitMapDataStream.str().length()), INMAGE_EX(HashSize)(bitMapDataStream.str().length()))
    buffer.reset( new char[Size] ) ;

    inm_memcpy_s( buffer.get(),Size, hash, HashSize ) ;
	inm_memcpy_s(buffer.get() + HashSize, (Size - HashSize),bitMapDataStream.str().c_str(), Size - HashSize);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",  __FUNCTION__ );
    return SVS_OK ;
}

SVERROR PersistentBitMap::loadBitMapFileDataToMemory(const char* data, const SV_UINT infoSize, std::string &  bitMapData )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
    SVERROR status ;
    std::stringstream bitMapDataStream ;
    unsigned char hash[HashSize];

    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)data + HashSize, infoSize - HashSize);
    INM_MD5Final(hash, &ctx);
    if( memcmp(data, hash, HashSize ) != 0 )
    {
        stringstream msg ;
        msg << getFileName() << " is Corrupted.." << " Reinitializing the bitmap " << endl;
        throw DataProtectionException( msg .str() );
    }
    else
    {
        std::string str ;
        data += HashSize ;
        bitMapDataStream.write( data, infoSize - HashSize ) ;
        bitMapDataStream >> str;
        this->setBitMapGranularity(atol(str.c_str()));
        bitMapDataStream >> str ;
#ifdef WIN32
        this->setBytesProcessed( (SV_ULONGLONG) _atoi64( str.c_str() ) ) ;
        bitMapDataStream >> str ;
        this->setBytesNotProcessed( (SV_ULONGLONG) _atoi64( str.c_str() ) ) ;
#else
        this->setBytesProcessed( strtoull( str.c_str(),  NULL, 0 ) ) ;
        bitMapDataStream >> str ;
        this->setBytesNotProcessed( strtoull( str.c_str(),  NULL, 0 ) ) ;
#endif

        bitMapDataStream >> bitMapData;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ );
    return status;
}
OnDiskBitMap::OnDiskBitMap( string fileName,  SV_ULONG bitMapGranularity ) : PersistentBitMap( fileName, bitMapGranularity )
{
}

SVERROR OnDiskBitMap::read(string &bitMapData, SV_ULONG expectedBitMapSize, CxTransport::ptr cxTransport, const bool &bShouldCreateIfNotPresent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	SV_ULONG const fileSize = (SV_ULONG)File::GetNumberOfBytes(this->getFileName());
    ACE_HANDLE handle = ACE_INVALID_HANDLE;
    SVERROR status = SVS_OK;

    if( !fileSize )
    {
        DebugPrintf(SV_LOG_DEBUG, "The file %s's size is empty...\n",  this->getFileName().c_str());
        std::stringstream bitMapDataStream;
        bitMapData = "";
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "File %s's size is %ld\n", getFileName().c_str(), fileSize);
		// PR#10815: Long Path support
		handle = ACE_OS::open( getLongPathName(this->getFileName().c_str()).c_str(), O_RDONLY );
        if( handle == ACE_INVALID_HANDLE )
        {
            DebugPrintf(SV_LOG_ERROR, "The %s is not readable.. Error %d\n", this->getFileName().c_str(), ACE_OS::last_error());
            DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
            status = SVS_FALSE;
        }
        else
        {
            size_t infolen;
            INM_SAFE_ARITHMETIC(infolen = InmSafeInt<SV_ULONG>::Type(fileSize) + 1, INMAGE_EX(fileSize))
            boost::shared_array<char> info (new (nothrow) char[infolen]);
            if( fileSize == ACE_OS::read(handle, info.get(), fileSize) )
            {
                info[ fileSize ] = '\0';
                try
                {
                    loadBitMapFileDataToMemory(info.get(), fileSize, bitMapData) ;
                    if (!bitMapData.empty())
                    {
                        size_t bmlen = bitMapData.length();
                        if (expectedBitMapSize != bmlen)
                        {
                            std::stringstream msg;
                            msg << "For bitmap file " << this->getFileName() 
                                << ", number of bits in bitmap from bit map file = " << bmlen
                                << ", does not match locally calculated number of bits = " << expectedBitMapSize;
                            if (bShouldCreateIfNotPresent)
                            {
                                msg << ". Hence reinitializing the bitmap with locally calculated size."
                                    << "This may result in resync percentage going back\n";
                            }
                            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                            bitMapData.clear();
                            throw DataProtectionException(msg.str());
                        }
                    }
                }
                catch( DataProtectionException de )
                {
                    DebugPrintf(SV_LOG_ERROR, "Removing the stale bitmap %s from filesystem. Error: %s\n", getFileName().c_str(), de.what());
                    ACE_OS::close( handle );
					// PR#10815: Long Path support
                    if (bShouldCreateIfNotPresent)
                    {
					    ACE_OS::unlink( getLongPathName(getFileName().c_str()).c_str() ) ;
                    }
                    else
                    {
                        status = SVS_FALSE;
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to read the entire bitmap file %s. Requested Read size %d\n", getFileName().c_str(), fileSize );
                status = SVS_FALSE;
            }
            ACE_OS::close( handle );
        }
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
    return status;
}

SVERROR OnDiskBitMap::write(const string& str, CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    SVERROR status;
    ACE_HANDLE handle = ACE_INVALID_HANDLE;
    std::stringstream bitMapDataStream;

	// PR#10815: Long Path support
#ifdef WIN32
	handle = ACE_OS::open(getLongPathName(this->getFileName().c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC, ACE_DEFAULT_FILE_PERMS );
#else
	handle = ACE_OS::open(getLongPathName(this->getFileName().c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    if( handle == ACE_INVALID_HANDLE )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to open %s for writing.. Error %d\n", this->getFileName().c_str(), ACE_OS::last_error());
        DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
        status = SVS_FALSE;
    }
    else
    {
        SV_UINT size ;
        boost::shared_array<char> info ;
        createBitMapFileDataFromMemory(info, size, str );

        if( ACE_OS::write(handle, info.get(), size ) != size )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to write to %s. Error is %d\n", this->getFileName().c_str(), ACE_OS::last_error());
            status =  SVS_FALSE;
        }

        ACE_OS::close( handle );
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
    return status;
}

SVERROR OnDiskBitMap::deleteFromPersistentStore(CxTransport::ptr cxTransport)
{
	// PR#10815: Long Path support
	ACE_OS::unlink( getLongPathName(getFileName().c_str()).c_str()) ;
    return SVS_OK ;
}


RemoteBitMap::RemoteBitMap(const std::string &RemoteName, const bool bForceRename, const SV_ULONG bitMapGranularity, bool isSecure) :
    m_bForceRename(bForceRename),
    PersistentBitMap(RemoteName, bitMapGranularity ),
    m_Secure( isSecure )
{
}
SVERROR RemoteBitMap::read(string& bitmapData, SV_ULONG expectedBitMapSize, CxTransport::ptr cxTransport, const bool &bShouldCreateIfNotPresent)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    SVERROR  status;
    char* buffer = 0 ;
    std::size_t size = 0 ;
    SV_ULONG estimatedBufferSize = 0;
    FileInfos_t fileInfos;

    boost::tribool tblf = cxTransport->listFile(getFileName(), fileInfos);
    if( !tblf )
    {
		/* transport error */
        DebugPrintf(SV_LOG_ERROR, "GetFileList with Spec %s failed: %s\n", getFileName().c_str(), cxTransport->status());
        return SVS_FALSE;
    }
    else if ( tblf )
    {
		/* file present */
        size = fileInfos.at(0).m_size;
        if( size == 0 )
        {
			DebugPrintf(SV_LOG_DEBUG,"The bitmap file %s at cx is having its size as zero.\n", getFileName().c_str());
            // cxTransport->deleteFile( getFileName() );
        }
        else
        {
            std::size_t savesize = size;
            size_t bytesReturned = 0;
            std::stringstream filelistmsg;
            filelistmsg << "Size of the remote data from GetFileList is " << size 
                        << " for file " << getFileName() << '\n';
            DebugPrintf(SV_LOG_DEBUG, "%s", filelistmsg.str().c_str());
            boost::tribool tb = cxTransport->getFile(getFileName(), size, &buffer, bytesReturned);
            if( !tb || CxTransportNotFound(tb) )
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get the bitmap file %s on cx to buffer.. Tal status %s\n", getFileName().c_str(), cxTransport->status());
                return SVS_FALSE;
            }
            std::stringstream getremotedatamsg;
            getremotedatamsg << "Size of the remote data from GetRemoteDataToBuffer is " << size 
                             << " for file " << getFileName() << '\n';
            DebugPrintf(SV_LOG_DEBUG, "%s", getremotedatamsg.str().c_str());
            if( buffer == NULL )
            {
                DebugPrintf(SV_LOG_ERROR, "The invalid buffer.. Unable to get the remote data in %s to buffer\n", getFileName().c_str() );
                return SVS_FALSE ;
            }
			if( savesize != size )
			{
                std::stringstream msg;
                msg << "The buffer is invalid for file " << getFileName()
                    << ". size received = " << size << "size requested = " << savesize << '\n';
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
				return SVS_FALSE ;
			}
            boost::shared_array<char> info ( buffer, free );
            try
            {
                loadBitMapFileDataToMemory(info.get(), size, bitmapData )   ;
                if (!bitmapData.empty())
                {
                    size_t bmlen = bitmapData.length();
                    if (expectedBitMapSize != bmlen)
                    {
                        std::stringstream msg;
                        msg << "For bitmap file " << this->getFileName() 
                            << ", number of bits in bitmap from bit map file = " << bmlen
                            << ", does not match locally calculated number of bits = " << expectedBitMapSize;
                        if (bShouldCreateIfNotPresent)
                        {
                            msg << ". Hence reinitializing the bitmap with locally calculated size."
                                << "This may result in resync percentage going back\n";
                        }
                        DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                        bitmapData.clear();
                        throw DataProtectionException(msg.str());
                    }
                }
            }
            catch( DataProtectionException de )
            {
                DebugPrintf(SV_LOG_ERROR, "Removing the stale bitmap %s from CX. Error: %s\n", getFileName().c_str(), de.what());
                if (bShouldCreateIfNotPresent)
                {
                    if( !cxTransport->deleteFile( getFileName() ) )
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to remove file %s from CX.. Tal Error %s \n", getFileName().c_str(), cxTransport->status() );
                        status = SVS_FALSE;
                    }
                }
                else
                {
                    status = SVS_FALSE;
                }
            }
        }
    }
    else 
    {
		/* File not present */
        DebugPrintf(SV_LOG_DEBUG,"The %s doesnt exist at cx\n", getFileName().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return status;
}

SVERROR RemoteBitMap::write(const string& bitmapData, CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    SVERROR status = SVS_OK;
    SV_UINT size = 0;
    boost::shared_array<char> info ;
    createBitMapFileDataFromMemory(info, size, bitmapData );
    std::string bmfilename = getFileName();
    const char DELIM = '/';

    std::string filetosend;
    /* 
    * Adds pre_ to bmfilename if not added else filetosend is same as bmfilename
    */
    bool bForceRename = IsPreAdded(bmfilename, DELIM, filetosend);
    
    if (m_bForceRename)
    {
        filetosend = bmfilename;
        bForceRename = true;
    }

    if( !cxTransport->putFile(filetosend, size, info.get(), false,  COMPRESS_NONE) )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to write bitmapData to %s at CX. tal status %s\n", bmfilename.c_str(), cxTransport->status() );
        status = SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully written bitmapData to %s at cx\n", bmfilename.c_str());
        if (bForceRename)
        {
            /* rename from filetosend to bmfilename */
            boost::tribool tb = cxTransport->renameFile(filetosend, bmfilename, COMPRESS_NONE);
            if( !tb || CxTransportNotFound(tb) )
            {
                std::stringstream msg;
                msg << "FAILED RemoteBitMap::write rename From " <<  filetosend << " To "
                    << bmfilename <<" Tal Status : " << cxTransport->status()  << '\n';
                if ( !cxTransport->deleteFile(filetosend ) ) 
                {
                    msg << "also failed to delete " << filetosend << ": " << cxTransport->status() << '\n';
                }
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                status = SVS_FALSE;
            }
        }
    }

    //persisting bitmap to local directory for to debug anything if required..
    string filename ;    
    std::string::size_type index = bmfilename.find_last_of( "/\\" );
    if (std::string::npos == index)
    {
        index = -1; // -1 because below index + 1 is used 
    }
    LocalConfigurator lc ;
    filename = lc.getInstallPath() +
        ACE_DIRECTORY_SEPARATOR_CHAR_A + Resync_Folder +
        ACE_DIRECTORY_SEPARATOR_CHAR_A +
        bmfilename.substr( index + 1 );
    ACE_HANDLE handle = ACE_INVALID_HANDLE ;

// PR#10815: Long Path support
#ifdef WIN32
	handle = ACE_OS::open( getLongPathName(filename.c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC, ACE_DEFAULT_FILE_PERMS );
#else
	handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif

    if( ACE_OS::write( handle, info.get(), size ) != size )
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to write the bitmap data to local directory. file name %s and error %d\n",
                    filename.c_str(),
                    ACE_OS::last_error() );
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully written %d bytes to file %s\n", size, filename.c_str());
    }
    ACE_OS::close( handle ) ;
    handle = ACE_INVALID_HANDLE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return status;
}

SVERROR RemoteBitMap::deleteFromPersistentStore(CxTransport::ptr cxTransport)
{
    FileInfos_t finfo;
    if( cxTransport->listFile(getFileName(), finfo ) && finfo.size() == 1 )
    {
        if( !cxTransport->deleteFile(getFileName() ) )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to Remove the file %s at CX: %s\n", getFileName().c_str(), cxTransport->status());
            return SVS_FALSE ;
        }
        else
            DebugPrintf(SV_LOG_DEBUG, "Successfully removed %s from CX\n", getFileName().c_str());
    }
    return SVS_OK ;
}


