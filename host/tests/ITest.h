///
///  \file  ITest.h
///
///  \brief contains ITest interface
///

#ifndef ITEST_H
#define ITEST_H

#include "boost/shared_ptr.hpp"

/// \brief ITest interface
class ITest
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<ITest> Ptr; ///< Pointer type

	/// \brief status
	enum Status
	{
		NOT_STARTED, ///< not started: initial state
		IN_PROGRESS, ///< in progress: query again
		PASS,        ///< passed
		FAIL,        ///< failed
		ABORTED      ///< aborted

	};

	/// \brief required setup
	///
	/// throws std::exception
	virtual void Setup(void) = 0;

	/// \brief run
	///
	/// throws std::exception
	virtual void run(void) = 0;

	/// \brief returns status
	///
	/// \return status
	/// \li \c throws std::exception
	virtual Status GetStatus(void) = 0;

	/// \brief stop
	///
	/// throws std::exception
	virtual void Stop(void) = 0;

	/// \brief abort
	///
	/// throws std::exception
	virtual void Abort(void) = 0;

	/// \brief clean up
	///
	/// throws std::exception
	virtual void Cleanup(void) = 0;

	/// \brief destructor
	virtual ~ITest() {}
};

#endif /* ITEST_H */
