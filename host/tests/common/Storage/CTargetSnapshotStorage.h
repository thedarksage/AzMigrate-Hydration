///
///  \file  CTargetSnapshotStorage.h
///
///  \brief contains CTargetSnapshotStorage class
///

#ifndef CTARGETSNAPSHOTSTORAGE_H
#define CTARGETSNAPSHOTSTORAGE_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "IStorage.h"

#include "errorexception.h"

/// \brief CTargetSnapshotStorage class
class CTargetSnapshotStorage : public IBlockStorage
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CTargetSnapshotStorage> Ptr; ///< Pointer type

	/// \brief Constructor
	CTargetSnapshotStorage(const std::wstring &id) ///< wstring id
		: m_pwsID(new std::wstring(id))
	{
	}

	/// \brief Constructor
	CTargetSnapshotStorage(const std::string &id) ///< string id
		: m_pwsID(new std::wstring(id.begin(), id.end()))
	{
	}

	bool Online(bool readOnly)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	bool Offline(bool readonly)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	DWORD GetCopyBlockSize()
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	ULONGLONG GetDeviceSize()
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	bool Read(LPVOID buffer, DWORD dwBytesToRead, DWORD& dwBytesRead)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	bool Write(LPCVOID buffer, DWORD dwBytesToBytes, DWORD& dwBytesWritten)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}
	
	bool DeviceIoControlSync(
		_In_        DWORD        dwIoControlCode,
		_In_opt_    LPVOID       lpInBuffer,
		_In_        DWORD        nInBufferSize,
		_Out_opt_   LPVOID       lpOutBuffer,
		_In_        DWORD        nOutBufferSize,
		_Out_opt_   LPDWORD      lpBytesReturned
		)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	DWORD   GetDeviceNumber()
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	std::string  GetDeviceName()
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

    std::string  GetDeviceId()
	{
		return std::string(m_pwsID->begin(), m_pwsID->end());
	}

	DWORD   GetMaxRwSizeInBytes()
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}
	
	bool    SeekFile(FileMoveMethod dwMoveMethod, unsigned long long offset)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

private:
	boost::shared_ptr<std::wstring> m_pwsID; ///< wstring ID
};

#endif /* CTARGETSNAPSHOTSTORAGE_H */
