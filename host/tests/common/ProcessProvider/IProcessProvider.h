///
///  \file  IProcessProvider.h
///
///  \brief contains IProcessProvider interface
///

#ifndef IPROCESSPROVIDER_H
#define IPROCESSPROVIDER_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

/// \brief IProcessProvider interface
class IProcessProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IProcessProvider> Ptr; ///< Pointer type

	/// \brief Process arguments type
	typedef std::vector<std::string> ProcessArguments_t;

	/// \brief Const arguments iterator
	typedef ProcessArguments_t::const_iterator ConstProcessArgumentsIter_t;

	/// \brief Runs a synchronous process. The function does not return until process execution completes.
	///
	/// throws std::exception
	virtual void RunSyncProcess(const std::string &name, ///< name
		const ProcessArguments_t &args,                  ///< args
		std::string &output,                             ///< out: stdout
		std::string &error,                              ///< out: stderr
		int &exitStatus                                  ///< out: exit status
		) = 0;

	/// \brief destructor
	virtual ~IProcessProvider() {}
};

#endif /* IPROCESSPROVIDER_H */