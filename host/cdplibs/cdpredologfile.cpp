#include <ace/OS.h>
#include "cdpredologfile.h"
#include "sharedalignedbuffer.h"
#include "cdpglobals.h"
#include "cdputil.h"
#include "svdparse.h"
#include "inmsafecapis.h"

bool CdpRedoLogFile::Parse()
{
	bool rv = true, done = false;
	SV_UINT readin = 0;
	SVD_PREFIX prefix;

	do 
	{
		ACE_stat filestat = {0};

		if(sv_stat(m_filename.c_str(),&filestat) < 0)
		{
			DebugPrintf(SV_LOG_ERROR,"FUNCTION: %s. Unable to find file %s\n",FUNCTION_NAME,m_filename.c_str());
			rv = false;
			break;
		}

		int openmode = O_RDONLY;


		m_handle = ACE_OS::open(getLongPathName(m_filename.c_str()).c_str(),openmode);

		if(ACE_INVALID_HANDLE == m_handle)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
				FUNCTION_NAME, m_filename.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}

		while (true)
		{
			readin = ACE_OS::read(m_handle,&prefix,SVD_PREFIX_SIZE);

			if (SVD_PREFIX_SIZE != readin)
			{
				std::stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << m_filename << "is corrupt.\n" 
					<< " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
					<< " Actual Read Bytes: "  << readin << "\n"
					<< " Error Code: " << ACE_OS::last_error() << "\n"
					<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

				rv = false;
				break;
			}
			else
			{
				std::stringstream l_stdfatal;
				switch (prefix.tag)
				{
				case SVD_TAG_REDOLOG_HEADER:
					rv = ReadHeader(m_handle,prefix);
					break;

				case SVD_TAG_LENGTH_OF_DRTD_CHANGES:
					rv = ReadLODC(m_handle,prefix);
					break;

				case SVD_TAG_DIRTY_BLOCK_DATA:
					rv = ReadDRTD(m_handle,prefix);
					done = true;
					break;

					// Unkown chunk
				default:

					l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
						<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
						<< "\n" << m_filename   << "is corrupt.\n" 
						<< "Encountered an unknown tag: " << prefix.tag << "\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
					rv = false;
					break;
				}
			}

			if ( !rv || done )
			{
				break;
			}
		}
	}while(0);

	return rv;
}

cdp_drtdv2s_t & CdpRedoLogFile::GetDrtds()
{
	return m_redo_drtds;
}

bool CdpRedoLogFile::GetData(char *buffer,SV_UINT length_of_data)
{
	bool rv = true;
	SV_UINT bytes_read = 0;

	do 
	{
		if((bytes_read = ACE_OS::read(m_handle,buffer,length_of_data))!=length_of_data)
		{
			rv = false;
			std::stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "\n" << m_filename << "Failed to read.\n" 
				<< " Expected Read Bytes: " << length_of_data << "\n"
				<< " Actual Read Bytes: "  << bytes_read << "\n"
				<< " Error Code: " <<ACE_OS::last_error() << "\n"
				<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			break;
		}

	}while(0);

	return rv;
}

bool CdpRedoLogFile::WriteMetaData(cdp_drtdv2s_t & redo_drtds)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
	bool rv = true;
	SV_UINT length_metadata = 0;
	SV_UINT current_position = 0;

	if(redo_drtds.size())
	{
		// fill in length of data changes
		length_metadata = GetSizeOfHeader();

		length_metadata += SVD_PREFIX_SIZE; 
		length_metadata += ((redo_drtds.size()) * (SVD_DIRTY_BLOCK_SIZE));
	}

	SharedAlignedBuffer metadata(length_metadata,m_sector_size); 
	int size = metadata.Size(); // To find buffer length
	SVD_PREFIX prefix;

	//fill in the svd header.
	FILL_PREFIX(prefix, SVD_TAG_REDOLOG_HEADER, 1, 0);
	inm_memcpy_s(metadata.Get() + current_position, (size - current_position), &prefix, SVD_PREFIX_SIZE);
	current_position += SVD_PREFIX_SIZE;

	SVD_REDOLOG_HEADER hdr;
	hdr.Version.Major = 1;
	hdr.Version.Minor = 0;
	inm_memcpy_s(metadata.Get() + current_position, (size - current_position), &hdr, SVD_REDOLOG_HEADER_SIZE);
	current_position += SVD_REDOLOG_HEADER_SIZE;

	// fill in length of data changes
	FILL_PREFIX(prefix, SVD_TAG_LENGTH_OF_DRTD_CHANGES, 1, 0);
	inm_memcpy_s(metadata.Get() + current_position, (size - current_position), &prefix, SVD_PREFIX_SIZE);
	current_position += SVD_PREFIX_SIZE;

	SV_ULARGE_INTEGER lodc = {0};
	cdp_drtdv2s_iter_t drtdsIter = redo_drtds.begin();
	SV_ULONGLONG length_of_changes = 0;
	for ( /* empty */ ; drtdsIter != redo_drtds.end() ; ++drtdsIter )
	{
		length_of_changes += (*drtdsIter).get_length();
	}
	lodc.QuadPart += length_of_changes;
	lodc.QuadPart += SVD_PREFIX_SIZE;
	lodc.QuadPart += ((redo_drtds.size()) * (SVD_DIRTY_BLOCK_SIZE));

	inm_memcpy_s(metadata.Get() + current_position, (size - current_position), (const char*)(&lodc),
		SVD_LODC_SIZE);
	current_position += SVD_LODC_SIZE;

	// fill in drtd count information
	FILL_PREFIX(prefix, SVD_TAG_DIRTY_BLOCK_DATA, 
		redo_drtds.size() , 0);
	inm_memcpy_s(metadata.Get() + current_position, (size - current_position), &prefix, SVD_PREFIX_SIZE);
	current_position += SVD_PREFIX_SIZE;

	//fill in the drtd information.
	SVD_DIRTY_BLOCK drtdhdr;
	drtdsIter = redo_drtds.begin();
	for ( /* empty */ ; drtdsIter != redo_drtds.end() ; ++drtdsIter )
	{
		cdp_drtdv2_t cdp_drtd = *drtdsIter;

		drtdhdr.ByteOffset = cdp_drtd.get_volume_offset();
		drtdhdr.Length = cdp_drtd.get_length();

		inm_memcpy_s(metadata.Get() + current_position, (size - current_position), (const char*)&drtdhdr, SVD_DIRTY_BLOCK_SIZE);
		current_position += SVD_DIRTY_BLOCK_SIZE;
	}

	do
	{
		int openmode = O_WRONLY | O_CREAT | O_TRUNC;

		m_handle = ACE_OS::open(getLongPathName(m_filename.c_str()).c_str(),openmode);

		if(ACE_INVALID_HANDLE == m_handle)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
				FUNCTION_NAME, m_filename.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}


		SV_UINT remaining_bytes_to_write = length_metadata;
		SV_UINT offset = 0;

		while(remaining_bytes_to_write)
		{
			SV_UINT bytes_to_write = std::min<SV_UINT>(remaining_bytes_to_write,(SV_UINT)m_max_rw_size);
			SV_UINT bytes_written = 0;

			if((bytes_written = ACE_OS::write(m_handle,(metadata.Get()+offset),bytes_to_write)) != bytes_to_write)
			{
				rv = false;
				std::stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << m_filename << "Failed to write.\n" 
					<< " Expected Bytes to write: " << bytes_to_write << "\n"
					<< " Actual Bytes written: "  << bytes_written << "\n"
					<< " Error Code: " <<ACE_OS::last_error() << "\n"
					<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
				break;
			}
			offset += bytes_to_write;
			remaining_bytes_to_write -= bytes_to_write;
		}
		if(ACE_OS::fsync(m_handle) < 0)
		{

			rv = false;
			std::stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "\n" << m_filename << " Failed to flush data to disk.\n" 
				<< " Error Code: " <<ACE_OS::last_error() << "\n"
				<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

			break;
		}

	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
	return rv;
}

bool CdpRedoLogFile::WriteData(char * buffer,SV_UINT length_of_data)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s for file %s\n",FUNCTION_NAME,m_filename.c_str());
	bool rv = true;
	SV_UINT remaining_bytes_to_write = length_of_data;
	SV_UINT data_offset = 0;
	do
	{
		SV_UINT offset = 0;
		while(remaining_bytes_to_write)
		{
			SV_ULONGLONG bytes_to_write = std::min<SV_UINT>(remaining_bytes_to_write,(SV_UINT)m_max_rw_size);
			SV_ULONGLONG bytes_written = 0;

			if((bytes_written = ACE_OS::write(m_handle,(buffer+offset),bytes_to_write)) != bytes_to_write)
			{
				rv = false;
				std::stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << m_filename << "Failed to read.\n" 
					<< " Expected Bytes to write: " << bytes_to_write << "\n"
					<< " Actual Bytes written: "  << bytes_written << "\n"
					<< " Error Code: " <<ACE_OS::last_error() << "\n"
					<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
				break;
			}
			offset += bytes_to_write;
			remaining_bytes_to_write -= bytes_to_write;
		}

		if(ACE_OS::fsync(m_handle) < 0)
		{
			rv = false;
			std::stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "\n" << m_filename << " Failed to flush data to disk.\n" 
				<< " Error Code: " <<ACE_OS::last_error() << "\n"
				<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			break;
		}

	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Leaving %s for file %s\n",FUNCTION_NAME,m_filename.c_str());
	
	return rv;
}

bool CdpRedoLogFile::ReadHeader(ACE_HANDLE handle, const SVD_PREFIX & prefix)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	SV_UINT readin = 0;
	do 
	{
		SVD_REDOLOG_HEADER hdr;
		SharedAlignedBuffer buffer(SVD_REDOLOG_HEADER_SIZE,m_sector_size);
		readin = ACE_OS::read(handle,buffer.Get(),SVD_REDOLOG_HEADER_SIZE);
		if(SVD_REDOLOG_HEADER_SIZE != readin)
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: Failed to read file header, Filename %s\n",FUNCTION_NAME,m_filename.c_str());
			break;
		}
		inm_memcpy_s(&hdr, sizeof(hdr), buffer.Get(), SVD_REDOLOG_HEADER_SIZE);
		if(hdr.Version.Major != 1 || hdr.Version.Minor != 0)
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR,"Mismatch in version for file %s, Expected 1.0 but found %u.%u\n",m_filename.c_str(),
				hdr.Version.Major,hdr.Version.Minor);
			break;
		}
	}while(0);
	return rv;
}

bool CdpRedoLogFile::ReadLODC(ACE_HANDLE handle, const SVD_PREFIX & prefix)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = true;
    SV_UINT readin = 0;
    SV_OFFSET_TYPE offset;

    do {
		SV_ULARGE_INTEGER lodc;

		readin = ACE_OS::read(handle,&lodc,SVD_LODC_SIZE);

		if (SVD_LODC_SIZE != readin)
        {
            rv = false;
			DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: Failed to read LODC from file %s\n",FUNCTION_NAME,m_filename.c_str());
            break;
        }

        //Store the current offset
		offset = ACE_OS::lseek(handle,0,SEEK_CUR);

        //Seek to the end of LODC chunk to verify that the file contains the LODC size of data.
		if (ACE_OS::lseek(handle,(SV_OFFSET_TYPE)lodc.QuadPart, SEEK_CUR) < 0)
        {
			std::stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << m_filename    << "is corrupt.\n" 
                << "Encountered an error on seek to end of LODC chunk " << "\n"
				<< " Error Code: " << ACE_OS::last_error() << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        //Restore the offset.
		if(ACE_OS::lseek(handle,offset,SEEK_SET) != offset)
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR,"FUNCTION %s: Error while restoring offset for file %s, Error no: %s\n",
				FUNCTION_NAME,m_filename.c_str(),ACE_OS::last_error());
			break;
		}
		m_length_of_data_changes = lodc.QuadPart;

    } while (FALSE);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CdpRedoLogFile::ReadDRTD(ACE_HANDLE handle, const SVD_PREFIX & prefix)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	bool rv =true;
	SV_ULONG i = 0;
	SV_UINT  readin = 0;

	SV_OFFSET_TYPE   offset = 0;
	SVD_DIRTY_BLOCK header = {0};

	SV_UINT length_processed = 0;
	SV_UINT length_pending = 0;

	// Cycle through all records, reading each one
	m_number_of_drtds = prefix.count;
	for (i = 0; i < prefix.count; i++)
	{	
		// Read the record header
		readin = ACE_OS::read(handle,&header,SVD_DIRTY_BLOCK_SIZE);

		if ( SVD_DIRTY_BLOCK_SIZE != readin)
		{
			std::stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
				<< "\n" << m_filename    << "is corrupt.\n" 
				<< " Record " << (i+1) << " of SVD_DIRTY_BLOCK " << " does not exist" << "\n" 
				<< " Expected Read Bytes: " << SVD_DIRTY_BLOCK_SIZE << "\n"
				<< " Actual Read Bytes: "  << readin << "\n"
				<< " Error Code: " << ACE_OS::last_error() << "\n"
				<< " Error Message: " << Error::Msg(ACE_OS::last_error()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			rv = false;
			break;
		}
		cdp_drtdv2_t drtd(header.Length,header.ByteOffset,0,0,0,0);
		m_redo_drtds.push_back(drtd);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

