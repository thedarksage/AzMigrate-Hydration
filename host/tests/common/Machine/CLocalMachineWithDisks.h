///
///  \file  CLocalMachineWithDisks.h
///
///  \brief contains CLocalMachineWithDisks class
///

#ifndef CLOCALMACHINEWITHDISKS_H
#define CLOCALMACHINEWITHDISKS_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "IMachine.h"
#include "CLocalProcessProvider.h"
#include "CDiskDevice.h"
#include "PlatformAPIs.h"

/// \brief CLocalMachineWithDisks class
class CLocalMachineWithDisks : public IMachine
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CLocalMachineWithDisks> Ptr; ///< Pointer type

	/// \brief Disks_t type
	typedef std::vector<std::string> Disks_t;

	/// \brief const iterator for Disks_t
	typedef Disks_t::const_iterator ConstDisksIter_t;

	CLocalMachineWithDisks(const std::string &id, ///< id
		const Disks_t &disks,                     ///< disk names
		IPlatformAPIs *pCPlatformAPIs,            ///< Platform apis needed for disk
		ILoggerPtr pILogger                       ///< logger
		)     
		: m_ID(id),
		m_CLocalProcessProvider(pILogger)
	{
		// Create disks
		for (ConstDisksIter_t it = disks.begin(); it != disks.end(); it++) {
			const std::string &disk = *it;
			std::wstring wdisk(disk.begin(), disk.end());
			boost::shared_ptr<IBlockStorage> p(new CDiskDevice((wchar_t*)wdisk.c_str(), pCPlatformAPIs));
			m_Disks.push_back(p);
		}
	}

	/// \brief Gets ID
	std::string GetID(void)
	{
		return m_ID;
	}

	/// \brief Gets storage objects
	IStorageObjects GetStorageObjects(void)
	{
		return m_Disks;
	}

	/// \brief Runs a synchronous process. The function does not return until process execution completes.
	///
	/// throws std::exception
	void RunSyncProcess(const std::string &name,    ///< name
		IProcessProvider::ProcessArguments_t &args, ///< args
		std::string &output,                        ///< out: stdout
		std::string &error,                         ///< out: stderr
		int &exitStatus                             ///< out: exit status
	)
	{
		m_CLocalProcessProvider.RunSyncProcess(name, args, output, error, exitStatus);
	}

private:
	std::string m_ID;                              ///< ID

	IStorageObjects m_Disks;                       ///< Disks

	CLocalProcessProvider m_CLocalProcessProvider; ///< Local process provider
};

#endif /* CLOCALMACHINEWITHDISKS_H */
