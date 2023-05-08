#include "basicio.h"
#include "portable.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "localconfigurator.h"
#include "cdpemulatedvolumeimpl.h"
#ifdef SV_WINDOWS
#include <WinIoCtl.h>
#endif
#include<sstream>
#include<string>

#include "inmageex.h"
#include <exception>

using namespace std;


cdp_emulatedVolumeimpl_t:: cdp_emulatedVolumeimpl_t(const std::string & device_name)
:m_volumename(device_name)
{
    m_good = true;
    is_open = false;
    m_error = SV_SUCCESS;
    m_currentoffset = 0;
    m_volumesize = -1;
    m_maxfilesize = -1;
    m_invalidhandle = ACE_INVALID_HANDLE;
    m_share = 0;
    m_access = 0;
	m_max_rw_size = 0;
	m_sparse_enabled = true; //Sparse is enabled by default;
	m_punch_hole_supported = true;
    std::string sparsefile;
    bool new_sparsefile_format = false;    
    if(IsVolPackDevice(device_name.c_str(),sparsefile,new_sparsefile_format))
    {
        m_volumename = sparsefile;
        if(!new_sparsefile_format)
        {
            m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
            m_good = false;
        }
    }
    else
    {
        m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
        m_good = false;       
    }

    if(m_good)
    {

    	m_volumesize = 0;
        ACE_stat s;
        std::stringstream sparsepartfile;
        int i = 0;
        while(true)
        {
            sparsepartfile.clear();
            sparsepartfile.str("");
            sparsepartfile << m_volumename << SPARSE_PARTFILE_EXT << i;
            if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
            {
                break;
            }

            if(i==0)
                m_maxfilesize = s.st_size;
            m_volumesize += s.st_size;
            i++;
        }
    }
}

//destructor
// closing the handles if any open handles left 
cdp_emulatedVolumeimpl_t::~cdp_emulatedVolumeimpl_t()
{
    Close();
}

bool cdp_emulatedVolumeimpl_t:: OpenExclusive()
{
	bool retvalue = true;
	try
	{
		LocalConfigurator localconfigurator;
		m_max_rw_size  = localconfigurator.getCdpMaxIOSize();
		std::stringstream sparsepartfile;
		sparsepartfile  << m_volumename << SPARSE_PARTFILE_EXT << "0";
		m_sparse_enabled = IsSparseFile(sparsepartfile.str().c_str());
		m_punch_hole_supported = m_sparse_enabled;
		
	} catch ( ContextualException& ce )
	{
		DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n",FUNCTION_NAME, ce.what());
		retvalue = false;
	}
	catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "%s caught exception %s\n", FUNCTION_NAME, e.what());
		retvalue = false;
    } catch(...) {
        DebugPrintf(SV_LOG_ERROR, "%s caught an unknown exception\n", FUNCTION_NAME );
		retvalue = false;
    }
	if(!retvalue)  //If some exception is thrown,return from here
		return retvalue;

    if(!m_good)
        return false;

    m_mode = (BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess);
#ifdef SV_WINDOWS
    m_mode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
    m_mode |= BasicIo::BioDirect;
#endif

    if(!set_open_parameters(m_mode))
        return false;
    is_open = true;
    return true;
}

SV_INT cdp_emulatedVolumeimpl_t::Open(BasicIo::BioOpenMode mode)
{
	bool retvalue = true;
	try
	{
		LocalConfigurator localconfigurator;
		m_max_rw_size  = localconfigurator.getCdpMaxIOSize();
		std::stringstream sparsepartfile;
		sparsepartfile  << m_volumename << SPARSE_PARTFILE_EXT << "0";
		m_sparse_enabled = IsSparseFile(sparsepartfile.str().c_str());
	} catch ( ContextualException& ce )
	{
		DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n",FUNCTION_NAME, ce.what());
		retvalue = false;
	}
	catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "%s caught exception %s\n", FUNCTION_NAME, e.what());
		retvalue = false;
    } catch(...) {
        DebugPrintf(SV_LOG_ERROR, "%s caught an unknown exception\n", FUNCTION_NAME );
		retvalue = false;
    }
	if(!retvalue)  //If some exception is thrown,return from here
		return ERR_INVALID_FUNCTION;
	
	if(!m_good)
        return ERR_INVALID_FUNCTION;

    m_mode = mode;
    if(!set_open_parameters(m_mode))
        return ERR_INVALID_PARAMETER;

    is_open = true;
    return SV_SUCCESS;
}

BasicIo::BioOpenMode cdp_emulatedVolumeimpl_t::OpenMode(void)
{
    return m_mode;
}

SV_UINT cdp_emulatedVolumeimpl_t::Read(char* buffer_to_read_data, SV_UINT length)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_UINT bytes_processed = 0;

    do
    {
        if(!m_good)
            return 0;

        if(!is_open)
        {        
            m_error = Error::UserToOS(ERR_INVALID_FUNCTION);
            m_good = false;
            return 0;
        }

        if(!ModeOn(BasicIo::BioRead, m_mode))
        {
            m_error = Error::UserToOS(ERR_INVALID_FUNCTION);
            m_good = false;
            return 0;
        }

        cdp_drtds_sparsefile_t split_drtds;
        char * pbuffer = 0;

        if(!split_drtd_by_sparsefilename(m_currentoffset,length,split_drtds))
        {
            m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
            m_good = false;
            return 0;
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;
            pbuffer = buffer_to_read_data + bytes_processed;
            bytes_processed += split_drtd.get_length();

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            if(!get_sync_handle(split_drtd.get_filename(), file_handle))
            {
                // error is set inside get_sync_handle
                // m_error = Error::UserToOS(ERR_INVALID_HANDLE);
                m_good = false;
                break;
            }

			if(ACE_OS::llseek(file_handle,split_drtd.get_file_offset(),SEEK_SET) < 0)
			{
				stringstream l_stderr;
				l_stderr   << "seek to offset " << split_drtd.get_file_offset()
					<< " failed for file " << split_drtd.get_filename()
					<< ". error code: " << ACE_OS::last_error() << std::endl;

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				m_error = ACE_OS::last_error();
				m_good = false;
				break;
			}

            SV_UINT remaining_length = split_drtd.get_length();
            SV_UINT bytes_to_read = 0; 
            SV_UINT bytes_read = 0;
            while(remaining_length > 0)
            {
                bytes_to_read = min(m_max_rw_size, remaining_length);
				if(bytes_to_read != ACE_OS::read(file_handle,pbuffer + bytes_read,bytes_to_read))
                {
                    stringstream l_stderr;
                    l_stderr   << "read at offset " << (split_drtd.get_file_offset() + bytes_read)
                        << " failed for file " << split_drtd.get_filename()
                        << ". error code: " << ACE_OS::last_error() << std::endl;

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    m_error = ACE_OS::last_error();
                    m_good = false;
                    break;
                }

                bytes_read += bytes_to_read;
                remaining_length -= bytes_to_read;
                m_currentoffset += bytes_to_read;
            }

            if(!m_good)
                break;
        }

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(m_good)
        return bytes_processed;
    else
        return 0;
}


SV_UINT cdp_emulatedVolumeimpl_t:: FullRead(char* buffer, SV_UINT length)
{
    return Read(buffer,length);
}

SV_UINT cdp_emulatedVolumeimpl_t::Write(const char* in_buffer, SV_UINT length)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_UINT bytes_processed = 0;

    do
    {
        if(!m_good)
            return 0;

        if(!is_open)
        {        
            m_error = Error::UserToOS(ERR_INVALID_FUNCTION);
            m_good = false;
            return 0;
        }

        if(!ModeOn(BasicIo::BioWrite, m_mode))
        {
            m_error = Error::UserToOS(ERR_INVALID_FUNCTION);
            m_good = false;
            return 0;
        }

        cdp_drtds_sparsefile_t split_drtds;
        const char * pbuffer = 0;

        if(!split_drtd_by_sparsefilename(m_currentoffset,length,split_drtds))
        {
            m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
            m_good = false;
            return 0;
        }

        cdp_drtds_sparsefile_t::const_iterator drtd_iter = split_drtds.begin();
        for( ; drtd_iter != split_drtds.end(); ++drtd_iter)
        {
            cdp_drtd_sparsefile_t split_drtd = *drtd_iter;
            pbuffer = in_buffer + bytes_processed;
            bytes_processed += split_drtd.get_length();

            ACE_HANDLE file_handle = ACE_INVALID_HANDLE;
            if(!get_sync_handle(split_drtd.get_filename(), file_handle))
            {
                // error is set inside get_sync_handle
                //m_error = Error::UserToOS(ERR_INVALID_HANDLE);
                m_good = false;
                break;
            }

#ifdef SV_WINDOWS

            // for sparse virtual volumes 
            // writes are now happening directly to sparse file instead of going through
            // volpack driver
            // accordingly, even punching of hole in case of all data being zeroes
            // is moved to userspace

            if(m_punch_hole_supported && m_sparse_enabled && all_zeroes(pbuffer, split_drtd.get_length()))
            {
                if(!punch_hole_sync_sparsefile(split_drtd.get_filename(),
                    file_handle, 
                    split_drtd.get_file_offset(),
                    split_drtd.get_length()))
                {
                    // error is set inside punch_hole_sync_sparsefile
                    //m_error = GetLastError();
					if(!m_good)
	                    break;
                }
				else
				{
					m_currentoffset += split_drtd.get_length();
					// continue with next drtd, this drtd is all zeroes
					// and hole is punched
					continue;
				}
            }

#endif // punch hole support for virtual volume in windows

			if(ACE_OS::llseek(file_handle,split_drtd.get_file_offset(),SEEK_SET) < 0)
			{
				stringstream l_stderr;
				l_stderr   << "seek to offset " << split_drtd.get_file_offset()
					<< " failed for file " << split_drtd.get_filename()
					<< ". error code: " << ACE_OS::last_error() << std::endl;

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				m_error = ACE_OS::last_error();
				m_good = false;
				break;
			}

            SV_UINT remaining_length = split_drtd.get_length();
            SV_UINT bytes_to_write = 0; 
            SV_UINT bytes_written = 0;
            while(remaining_length > 0)
            {
                bytes_to_write = min(m_max_rw_size, remaining_length);

                if( bytes_to_write != ACE_OS::write(file_handle, 
                    pbuffer + bytes_written, 
                    bytes_to_write))
                {
                    stringstream l_stderr;
                    l_stderr   << "write at offset " << (split_drtd.get_file_offset() + bytes_written)
                        << " failed for file " << split_drtd.get_filename()
                        << ". error code: " << ACE_OS::last_error() << std::endl;

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    m_error = ACE_OS::last_error();
                    m_good = false;
                    break;
                }

                bytes_written += bytes_to_write;
                remaining_length -= bytes_to_write;
                m_currentoffset += bytes_to_write;
            }

            if(!m_good)
                break;
        }

    }while (0);

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(m_good)
        return bytes_processed;
    else
        return 0;
}


SV_LONGLONG cdp_emulatedVolumeimpl_t:: Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from)
{
    if(BasicIo::BioBeg == from)
        m_currentoffset = offset;
    else if(BasicIo::BioCur == from)
        m_currentoffset += offset;
    else if(BasicIo::BioEnd == from)
        m_currentoffset = m_volumesize - offset;
    else
    {
        m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
        m_good = false;
        return -1;
    }

    if (m_volumesize <= m_currentoffset) {
        m_currentoffset = m_volumesize;
        m_error = Error::UserToOS(ERR_HANDLE_EOF);
        m_good = false;
        return -1;
    } 

    m_error = Error::UserToOS(SV_SUCCESS);
    return m_currentoffset;
}

bool cdp_emulatedVolumeimpl_t::split_drtd_by_sparsefilename(const SV_OFFSET_TYPE & sparsevol_offset,
                                                            const SV_UINT & sparsevol_io_size,
                                                            cdp_drtds_sparsefile_t & splitdrtds)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::stringstream sparsepartfile;
    SV_UINT bytes_processed = 0;
    SV_UINT remaining_io = sparsevol_io_size;

    SV_LONGLONG fileid = sparsevol_offset / m_maxfilesize;
    SV_OFFSET_TYPE sparsefile_offset = sparsevol_offset %  m_maxfilesize;
    SV_UINT sparsefile_io_length = min((SV_OFFSET_TYPE)remaining_io, (m_maxfilesize - sparsefile_offset));
    sparsepartfile << m_volumename << SPARSE_PARTFILE_EXT << fileid;

    while(remaining_io > 0)
    {
        cdp_drtd_sparsefile_t split_drtd(sparsefile_io_length, 
            sparsevol_offset + bytes_processed,
            sparsefile_offset, sparsepartfile.str());
        splitdrtds.push_back(split_drtd);

        DebugPrintf(SV_LOG_DEBUG, "%s\n", sparsepartfile.str().c_str());

        remaining_io -= sparsefile_io_length;
        bytes_processed += sparsefile_io_length;
        ++fileid;

        sparsepartfile.clear();
        sparsepartfile.str("");
        sparsepartfile << m_volumename << SPARSE_PARTFILE_EXT << fileid;
        sparsefile_offset = 0;
        sparsefile_io_length = min((SV_OFFSET_TYPE)remaining_io, m_maxfilesize);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool cdp_emulatedVolumeimpl_t::all_zeroes(const char * const buffer, const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    for(SV_UINT i = 0; i < length; i++)
    {
        if(buffer[i] != 0)
        {
            rv = false;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool cdp_emulatedVolumeimpl_t::punch_hole_sync_sparsefile(const std::string & filename,
                                                          ACE_HANDLE file_handle,
                                                          const SV_OFFSET_TYPE & offset,
                                                          const SV_UINT & length)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());

#ifdef SV_WINDOWS

    FILE_ZERO_DATA_INFORMATION FileZeroDataInformation;
    FileZeroDataInformation.FileOffset.QuadPart = offset;
    FileZeroDataInformation.BeyondFinalZero.QuadPart = offset + length;

    SV_ULONG bytereturned = 0;
    if(!DeviceIoControl(file_handle,FSCTL_SET_ZERO_DATA,
        &FileZeroDataInformation,sizeof(FileZeroDataInformation),
        NULL,0,
        &bytereturned,
        NULL))
    {
		if(GetLastError() == ERROR_NOT_SUPPORTED)
		{
			//FSCTL_SET_ZERO_DATA is not supported for certain filesystems. Instead of erroring out we want to use
			//the normal write system apis.
			DebugPrintf(SV_LOG_ERROR, "FSCTL_SET_ZERO_DATA: failed for %s. ERROR_NOT_SUPPORTED. Use fallback method\n", 
				filename.c_str());
			m_punch_hole_supported = false;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "FSCTL_SET_ZERO_DATA: failed for %s. offset:" LLSPEC " length: %u error: %lu\n", 
				filename.c_str(), offset, length, GetLastError());
			m_error = GetLastError();
			m_good = false;
		}
        rv = false;
    }
#else
	m_punch_hole_supported = false;
    rv = false;
    DebugPrintf(SV_LOG_ERROR, "punch hole is not implemented on non-windows platforms.\n");
#endif

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}

bool cdp_emulatedVolumeimpl_t::get_sync_handle(const std::string & filename, 
                                               ACE_HANDLE & handle)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s  file: %s\n",FUNCTION_NAME, filename.c_str());
    do
    {
        cdp_synchandles_t::iterator handle_iter = m_handles.find(filename);
        if(handle_iter == m_handles.end())
        {
            // if the volume is opened for sequential access, close any previously 
            // opened handle to release system cache 
            if(ModeOn(BasicIo::BioSequentialAccess,m_mode))
            {
                if(!close_handles())
                {
                    // error is set inside close_handles
                    rv = false;
                    break;
                }
            }

            // PR#10815: Long Path support
            handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
                m_access, m_share);

            if(ACE_INVALID_HANDLE == handle)
            {
                DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
                    filename.c_str(), ACE_OS::last_error());
                m_error = ACE_OS::last_error();
                m_good = false;
                rv = false;
                break;
            }

            m_handles.insert(std::make_pair(filename, handle));
        }else
        {
            handle = handle_iter ->second;
        }

    }while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s file: %s\n",FUNCTION_NAME, filename.c_str());
    return rv;
}


//close all the handles in handle list
// returns zero on success else SV_FAILURE
SV_INT cdp_emulatedVolumeimpl_t:: Close()
{
    SV_INT rv = 0;
    if(!close_handles())
{
        rv = SV_FAILURE;
    }

    is_open = false;
    return rv;
}

bool cdp_emulatedVolumeimpl_t::close_handles()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    bool flush_required_flag = false;

#ifdef SV_SUN
    if(ModeOn(BasicIo::BioWrite,m_mode))
    {
        flush_required_flag = true;
    }
#else
    if(ModeOn(BasicIo::BioWrite,m_mode) && !ModeOn(BasicIo::BioNoBuffer, m_mode) && !ModeOn(BasicIo::BioDirect, m_mode))
    {
        flush_required_flag = true;
    }
#endif


    cdp_synchandles_t::const_iterator handle_iter =  m_handles.begin();
    for( ; handle_iter != m_handles.end(); ++handle_iter)
    {
        ACE_HANDLE handle_value = handle_iter ->second;
        if(flush_required_flag)
        {
            if(ACE_OS::fsync(handle_value) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Flush buffers failed for %s with error %d.\n",
                    handle_iter ->first.c_str(),
                    ACE_OS::last_error());
                m_error = ACE_OS::last_error();
                rv = false;
            }
        }
        ACE_OS::close(handle_value);
    }
    m_handles.clear();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool cdp_emulatedVolumeimpl_t::FlushFileBuffers()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    bool flush_required_flag = false;

#ifdef SV_SUN
    if(ModeOn(BasicIo::BioWrite,m_mode))
    {
        flush_required_flag = true;
    }
#else
    if(ModeOn(BasicIo::BioWrite,m_mode) && !ModeOn(BasicIo::BioNoBuffer, m_mode) && !ModeOn(BasicIo::BioDirect, m_mode))
    {
        flush_required_flag = true;
    }
#endif


    if(flush_required_flag)
    {
        cdp_synchandles_t::const_iterator handle_iter =  m_handles.begin();
        for( ; handle_iter != m_handles.end(); ++handle_iter)
        {
            ACE_HANDLE handle_value = handle_iter ->second;

            if(ACE_OS::fsync(handle_value) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Flush buffers failed for %s with error %d.\n",
                    handle_iter ->first.c_str(),
                    ACE_OS::last_error());
                m_error = ACE_OS::last_error();
                m_good = false;
                rv = false;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

SV_LONGLONG cdp_emulatedVolumeimpl_t::GetFormattedVolumeSize(const std::string& volumename)
{
    return cdp_emulatedVolumeimpl_t::GetRawVolumeSize(volumename);
}

SV_ULONGLONG cdp_emulatedVolumeimpl_t::GetRawVolumeSize(const std::string& volumename)
{
    SV_LONGLONG size = 0;
    ACE_stat s;
    std::string volguid = volumename;
    FormatVolumeNameToGuid(volguid);
    std::stringstream sparsepartfile;
    int i = 0;
    while(true)
    {
        sparsepartfile.clear();
        sparsepartfile.str("");
        sparsepartfile << volguid << SPARSE_PARTFILE_EXT << i;
        if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
        {
            break;
        }
        size += s.st_size;
        i++;
    }
    return size;
}

bool cdp_emulatedVolumeimpl_t::set_open_parameters(BasicIo::BioOpenMode mode)
{
    if (ModeOn(BasicIo::BioRW, mode)) 
    {
        m_access = O_RDWR;
    }
    else if (ModeOn(BasicIo::BioRead, mode)) 
    {
        m_access = O_RDONLY;
    }
    else if (ModeOn(BasicIo::BioWrite, mode)) 
    {
        m_access = O_WRONLY;
    }
    else
    {
        m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
        m_good = false;
        return false;
    }
    SV_ULONG disp;

    // check for invalid combination
    // can't specify BioTruncate nor BioOverwrite with any of the other open/create modes
    if ((ModeOn(BasicIo::BioTruncate, mode) && ModeOn(BasicIo::BioOverwrite, mode))
        || ((ModeOn(BasicIo::BioTruncate, mode) || ModeOn(BasicIo::BioOverwrite, mode))
        && (ModeOn(BasicIo::BioOpen, mode) || ModeOn(BasicIo::BioCreate, mode)))) 
    {
        m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
        m_good = false;
        return false;
    }
    // get open/create mode
    if (ModeOn(BasicIo::BioOpenCreate, mode))
    {
        disp = O_CREAT;
    }
    else if (ModeOn(BasicIo::BioOpen, mode))
    {
        disp = 0;
    }
    else if (ModeOn(BasicIo::BioCreate, mode))
    {
        disp = (O_CREAT | O_EXCL);
    }
    else if(ModeOn(BasicIo::BioTruncate, mode))
    {
        disp = O_TRUNC;
    }
    else if (ModeOn(BasicIo::BioOverwrite, mode))
    {
        disp = (O_CREAT | O_TRUNC);
    }
    else
    {
        m_error = Error::UserToOS(ERR_INVALID_PARAMETER);
        m_good = false;
        return false;
    }

    // windows share m_share

#ifdef SV_UNIX

    umask(S_IWGRP | S_IWOTH);
    m_share = ACE_DEFAULT_FILE_PERMS;

#else

    if (ModeOn(BasicIo::BioShareDelete, mode))
    {
        m_share |= FILE_SHARE_DELETE;
    }
    if (ModeOn(BasicIo::BioShareRead, mode))
    {
        m_share |= FILE_SHARE_READ;
    }
    if (ModeOn(BasicIo::BioShareWrite, mode))
    {
        m_share |= FILE_SHARE_WRITE;
    }

#endif

    // check for special flags
    SV_ULONG flags = FILE_ATTRIBUTE_NORMAL;

    if (ModeOn(BasicIo::BioNoBuffer, mode))
    {
        flags |= FILE_FLAG_NO_BUFFERING;
    }

    if (ModeOn(BasicIo::BioWriteThrough, mode))
    {
        flags |= FILE_FLAG_WRITE_THROUGH;
    }

#ifdef SV_UNIX
    if (ModeOn(BasicIo::BioDirect, mode))
    {
        setdirectmode(m_access);
    }
#endif

    m_access = (m_access | disp | flags );
    return true;
}

