#include <string>
#include <sstream>
#include <cstddef>
#include <cstring>
#include "inmsafecapis.h"

#include "platformbasedscsicommandissuer.h"


/* we do not need to include ace header files as dependencies
 * as this is declared in portablehelpersmajor.h and that indirectly 
 * includes ace headers */
HANDLE SVCreateFile(LPCTSTR lpFileName,
                    DWORD dwDesiredAccess,
                    DWORD dwShareMode,
                    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                    DWORD dwCreationDisposition,
                    DWORD dwFlagsAndAttributes,
                    HANDLE hTemplateFile);

PlatformBasedScsiCommandIssuer::PlatformBasedScsiCommandIssuer()
 : m_Handle(INVALID_HANDLE_VALUE)
{
}


PlatformBasedScsiCommandIssuer::~PlatformBasedScsiCommandIssuer()
{
    EndSession();
}


bool PlatformBasedScsiCommandIssuer::StartSession(const std::string &device)
{
    EndSession();
	
    std::string devicenameforscsi = GetNameToIssueScsiCommand(device);
    m_Handle = SVCreateFile(devicenameforscsi.c_str(), 
                            GENERIC_WRITE | GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, 
                            NULL, 
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, 
                            NULL);
    if (INVALID_HANDLE_VALUE == m_Handle)
    {
		std::stringstream sserr;
		sserr << "Failed to open: " << devicenameforscsi << " with error: " << GetLastError();
        m_ErrMsg = sserr.str();
    }
    else
    {
        m_Device = device;
    }

    return (INVALID_HANDLE_VALUE != m_Handle);
}


void PlatformBasedScsiCommandIssuer::EndSession(void)
{
    if (INVALID_HANDLE_VALUE != m_Handle)
    {
        CloseHandle(m_Handle);
        m_Handle = INVALID_HANDLE_VALUE;
        m_Device.clear();
    }
}


std::string PlatformBasedScsiCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool PlatformBasedScsiCommandIssuer::Issue(ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
	if (INVALID_HANDLE_VALUE == m_Handle)
    {
        m_ErrMsg = "Session did not start\n";
        return false;
    }

	bool bissuccess = false;
	if (AllocateXferBufferIfNeeded(scmd->m_XferLen))
	{
		memset(&m_ScsiPassThruDirectInput, 0, sizeof m_ScsiPassThruDirectInput);

		m_ScsiPassThruDirectInput.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
		m_ScsiPassThruDirectInput.sptd.CdbLength = scmd->m_CmdLen;
		m_ScsiPassThruDirectInput.sptd.SenseInfoLength = scmd->m_SenseLen;
		m_ScsiPassThruDirectInput.sptd.DataIn = SCSI_IOCTL_DATA_IN;
		m_ScsiPassThruDirectInput.sptd.DataTransferLength = scmd->m_XferLen;
		m_ScsiPassThruDirectInput.sptd.TimeOutValue = scmd->m_TimeOut;
		m_ScsiPassThruDirectInput.sptd.DataBuffer = m_Xfer.Get();
		m_ScsiPassThruDirectInput.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
		inm_memcpy_s(m_ScsiPassThruDirectInput.sptd.Cdb, sizeof m_ScsiPassThruDirectInput.sptd.Cdb, scmd->m_Cmd, scmd->m_CmdLen);

		ULONG length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
		ULONG returned = 0;
		BOOL rval =  DeviceIoControl(m_Handle,
									IOCTL_SCSI_PASS_THROUGH_DIRECT,
									&m_ScsiPassThruDirectInput,
									length,
									&m_ScsiPassThruDirectInput,
									length,
									&returned,
									FALSE);
		if (rval)
		{
			scmd->m_SenseLen = m_ScsiPassThruDirectInput.sptd.SenseInfoLength;
			scmd->m_XferLen = m_ScsiPassThruDirectInput.sptd.DataTransferLength;
			scmd->m_ScsiStatus = m_ScsiPassThruDirectInput.sptd.ScsiStatus;
			inm_memcpy_s(scmd->m_Xfer, scmd->m_XferLen, m_ScsiPassThruDirectInput.sptd.DataBuffer, m_ScsiPassThruDirectInput.sptd.DataTransferLength);			
			inm_memcpy_s(scmd->m_Sense, scmd->m_SenseLen, m_ScsiPassThruDirectInput.ucSenseBuf, m_ScsiPassThruDirectInput.sptd.SenseInfoLength);
			shouldinspectscsistatus = true;
			bissuccess = true;
		}
		else
		{
			std::stringstream sserr;
			sserr << "For device: " << m_Device << " IOCTL_SCSI_PASS_THROUGH_DIRECT failed with error: " << GetLastError();
			m_ErrMsg = sserr.str();
		}

	}
	else
	{
		m_ErrMsg = "For device " + m_Device + ", failed to allocate transfer data length with error: " + m_ErrMsg;
	}

	return bissuccess;
}


std::string PlatformBasedScsiCommandIssuer::GetNameToIssueScsiCommand(const std::string &device)
{
    /*
    * From spti.htm in C:\WinDDK\6001.18000\src\storage\tools\spti:
    * ============================================================
    * Typically the name is related to the device (e.g. - "\\.\Scanner0", "\\.\Tape1"). 
    * Except for COMn and LPTn, all device filenames must be prepended with a \\.\ to 
    * inform the I/O Manager that this is a device and not a standard file name
    */
	const std::string PREFIX = "\\\\.\\";
	const std::string COMSTR = "COM";
	const std::string LPTSTR = "LPT";
	std::string name;

    if((device.compare(0, PREFIX.length() , PREFIX) == 0) ||
       (device.compare(0, COMSTR.length() , COMSTR) == 0) ||
	   (device.compare(0, LPTSTR.length() , LPTSTR) == 0))
    {
		/* TODO:
		* -> For COM, LPT should there be ignore case comparision ?
		* -> Also should we verify that COM and LPT be followed by
		*    numbers only, as spti.htm mentions COMn and LPTn ?
		*/
		name = device;
	}
	else
	{
		name = PREFIX;
		name += device;
	}

	return name;
}


bool PlatformBasedScsiCommandIssuer::AllocateXferBufferIfNeeded(const size_t &inputxferbuflen)
{
	bool needtoallocate;
	int currentalignment;
		
	if (!GetXferBufferAlignment(&currentalignment))
	{
		m_ErrMsg = std::string("For device ") + m_Device + ", could not get alignment for scsi data buffer.";
		return false;
	}

    if (0 == m_Xfer.Size())
	{
		/* initial time */
		needtoallocate = true;
	}
	else if ((currentalignment == m_Xfer.Alignment()) &&
             (inputxferbuflen <= (size_t)m_Xfer.Size()))
	{
		/* fits */
		memset(m_Xfer.Get(), 0, m_Xfer.Size());
		needtoallocate = false;
	}
	else
	{
		/* does not align or fit */
		needtoallocate = true;
	}

	bool allocated = true;
	std::stringstream sserr;
	sserr << "Failed to allocate " << inputxferbuflen << " bytes aligned at " << currentalignment;
	if (needtoallocate)
	{
		try 
		{
			m_Xfer.Reset(inputxferbuflen, currentalignment);
			/* memset not needed here as Reset already does */
		}
	    catch ( ContextualException const& ce ) 
		{
			allocated = false;
			sserr << ", with exception: " << ce.what();
	        m_ErrMsg = sserr.str();
		}
		catch( std::exception const& e ) 
		{
			allocated = false;
			sserr << ", with exception: " << e.what();
		    m_ErrMsg = sserr.str();
		}
		catch(...) 
		{
			allocated = false;
			m_ErrMsg = sserr.str();
	    }
	}

	return allocated;
}


/* refered from sdk example: stpi.c */
bool PlatformBasedScsiCommandIssuer::GetXferBufferAlignment(int *palignment)
{
    PSTORAGE_ADAPTER_DESCRIPTOR adapterDescriptor = NULL;
    STORAGE_DESCRIPTOR_HEADER header = {0};

    BOOL ok = TRUE;
    bool failed = true;
    ULONG i;

    *palignment = 0; // default to no alignment

    // Loop twice:
    //  First, get size required for storage adapter descriptor
    //  Second, allocate and retrieve storage adapter descriptor
    for (i=0;i<2;i++) {

        PVOID buffer;
        ULONG bufferSize;
        ULONG returnedData;

        STORAGE_PROPERTY_QUERY query;
		memset(&query, 0, sizeof query);

        switch(i) {
            case 0: {
                query.QueryType = PropertyStandardQuery;
                query.PropertyId = StorageAdapterProperty;
                bufferSize = sizeof(STORAGE_DESCRIPTOR_HEADER);
                buffer = &header;
                break;
            }
            case 1: {
                query.QueryType = PropertyStandardQuery;
                query.PropertyId = StorageAdapterProperty;
                bufferSize = header.Size;
                if (bufferSize != 0) {
                    adapterDescriptor = (PSTORAGE_ADAPTER_DESCRIPTOR)LocalAlloc(LPTR, bufferSize);
                    if (adapterDescriptor == NULL) {
                        goto Cleanup;
                    }
                }
                buffer = adapterDescriptor;
                break;
            }
        }

        // buffer can be NULL if the property queried DNE.
        if (buffer != NULL) {
            RtlZeroMemory(buffer, bufferSize);

            // all setup, do the ioctl
            ok = DeviceIoControl(m_Handle,
                                 IOCTL_STORAGE_QUERY_PROPERTY,
                                 &query,
                                 sizeof(STORAGE_PROPERTY_QUERY),
                                 buffer,
                                 bufferSize,
                                 &returnedData,
                                 FALSE);
            if (!ok) {
                if (GetLastError() == ERROR_MORE_DATA) {
                    // this is ok, we'll ignore it here
                } else if (GetLastError() == ERROR_INVALID_FUNCTION) {
                    // this is also ok, the property DNE
                } else if (GetLastError() == ERROR_NOT_SUPPORTED) {
                    // this is also ok, the property DNE
                } else {
                    // some unexpected error -- exit out
                    goto Cleanup;
                }
                // zero it out, just in case it was partially filled in.
                RtlZeroMemory(buffer, bufferSize);
            }
        }
    } // end i loop

    // adapterDescriptor is now allocated and full of data.
    if (adapterDescriptor) {
        /* The valid mask values are 0 (byte aligned), 1 (word aligned), 3 (DWORD aligned), and 7 (double DWORD aligned). */
        *palignment = adapterDescriptor->AlignmentMask+1;
    }

    failed = false;

Cleanup:
    if (adapterDescriptor != NULL) {
        LocalFree( adapterDescriptor );
    }
    return (!failed);
}
