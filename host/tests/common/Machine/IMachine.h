///
///  \file  IMachine.h
///
///  \brief contains IMachine interface
///

#ifndef IMACHINE_H
#define IMACHINE_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "IStorage.h"
#include "IProcessProvider.h"

/// \brief IMachine interface
class IMachine
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IMachine> Ptr; ///< Pointer type

	/// \brief Gets ID
	virtual std::string GetID(void) = 0;     

	/// \brief Gets storage objects
	virtual IStorageObjects GetStorageObjects(void) = 0;

	/// \brief Runs a synchronous process. The function does not return until process execution completes.
	///
	/// throws std::exception
	virtual void RunSyncProcess(const std::string &name,    ///< name
		IProcessProvider::ProcessArguments_t &args, ///< args
		std::string &output,                        ///< out: stdout
		std::string &error,                         ///< out: stderr
		int &exitStatus                             ///< out: exit status
		) = 0;

	/// \brief destructor
	virtual ~IMachine() {}
};

#endif /* IMACHINE_H */
