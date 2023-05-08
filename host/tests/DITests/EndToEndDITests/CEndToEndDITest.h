///
///  \file  CEndToEndDITest.h
///
///  \brief contains CEndToEndDITest class
///

#ifndef CEND_TO_END_DI_TEST_H
#define CEND_TO_END_DI_TEST_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "ITest.h"
#include "IMachine.h"
#include "IConsistencyProvider.h"
#include "ISnapshotProvider.h"
#include "ISnapshotDIValidator.h"
#include "ILogger.h"
#include "IConsistencyPoint.h"
#include "ISnapshot.h"

#include "errorexception.h"

/// \brief EndToEndDITest class
class CEndToEndDITest : public ITest
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CEndToEndDITest> Ptr; ///< Pointer type

	CEndToEndDITest(IMachine::Ptr pISourceMachine,             ///< source machine
		IConsistencyProvider::Ptr pISourceConsistencyProvider, ///< source consistency provider
		ISnapshotProvider::Ptr pISourceSnapshotProvider,       ///< source snapshot provider
		ISnapshotProvider::Ptr pITargetSnapshotProvider,       ///< target snapshot provider
		ISnapshotDIValidator::Ptr pISnapshotDIValidator,       ///< snapshot DI validator
		const std::string &testDir,                            ///< test directory
		ILoggerPtr pILogger                                    ///< logger
		) : 
		m_pISourceMachine(pISourceMachine),
		m_pISourceConsistencyProvider(pISourceConsistencyProvider),
		m_pISourceSnapshotProvider(pISourceSnapshotProvider),
		m_pITargetSnapshotProvider(pITargetSnapshotProvider),
		m_pISnapshotDIValidator(pISnapshotDIValidator),
		m_TestDir(testDir),
		m_pILogger(pILogger),
		m_Status(NOT_STARTED)
	{
	}

	/// \brief required setup
	///
	/// throws std::exception
	void Setup(void)
	{
	}

	/// \brief run
	///
	/// throws std::exception
	void run(void)
	{
		m_pILogger->LogInfo("Source machine ID %s\n", m_pISourceMachine->GetID().c_str());

		// Take source snapshot
		//
		// Note: Dummy consistency point is needed here because taking consistency point is next step.
		m_pILogger->LogInfo("Taking source snapshot.\n");
		IConsistencyPoint::Ptr pDummyConsistentPoint;
		ISnapshot::Ptr pISourceSnapshot = m_pISourceSnapshotProvider->CreateSnapshot(pDummyConsistentPoint);
		if (!pISourceSnapshot)
			throw ERROR_EXCEPTION << "Failed to take source snapshot point";

		// Create source consistency point
		m_pILogger->LogInfo("Taking consistency point.\n");
		IConsistencyPoint::Ptr pISourceConsistencyPoint = m_pISourceConsistencyProvider->CreateConsistencyPoint();
		if (!pISourceConsistencyPoint)
			throw ERROR_EXCEPTION << "Failed to take consistency point";

		// Take target snapshot for given consistency point
		m_pILogger->LogInfo("Taking target snapshot.\n");
		ISnapshot::Ptr pITargetSnapshot = m_pITargetSnapshotProvider->CreateSnapshot(pISourceConsistencyPoint);
		if (!pITargetSnapshot)
			throw ERROR_EXCEPTION << "Failed to take target snapshot point for consistency point ID " << pISourceConsistencyPoint->GetID();
		
		// Compare source snapshot and target snapshot
		m_pILogger->LogInfo("Comparing source and target snapshot.\n");
		bool isDIValid = m_pISnapshotDIValidator->Validate(pISourceSnapshot, pITargetSnapshot);
		if (isDIValid) {
			m_Status = PASS;
			m_pILogger->LogInfo("DI test passed\n");
			// Remove the source snapshot only on DI match. Keep the snapshot for debugging for test failure
			m_pISourceSnapshotProvider->DeleteSnapshot(pISourceSnapshot);
		}
		else {
			m_Status = FAIL;
			m_pILogger->LogInfo("DI test failed\n");
		}
	}

	/// \brief returns status
	///
	/// \return status
	/// \li \c throws std::exception
	Status GetStatus(void)
	{
		return m_Status;
	}

	/// \brief stop
	///
	/// throws std::exception
	void Stop(void)
	{
	}

	/// \brief abort
	///
	/// throws std::exception
	void Abort(void)
	{
	}

	/// \brief clean up
	///
	/// throws std::exception
	void Cleanup(void)
	{
	}

private:
	IMachine::Ptr m_pISourceMachine;                         ///< source machine

	IConsistencyProvider::Ptr m_pISourceConsistencyProvider; ///< source consistency provider

	ISnapshotProvider::Ptr m_pISourceSnapshotProvider;       ///< source snapshot provider

	ISnapshotProvider::Ptr m_pITargetSnapshotProvider;       ///< target snapshot provider

	ISnapshotDIValidator::Ptr m_pISnapshotDIValidator;       ///< snapshot DI validator

	const std::string m_TestDir;                             ///< test directory

	ILoggerPtr m_pILogger;                                   ///< logger

	Status m_Status;                                         ///< test status
};

#endif /* CEND_TO_END_DI_TEST_H */