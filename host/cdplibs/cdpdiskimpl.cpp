///
///  \file  cdpdiskimpl.cpp
///
///  \brief wrapper over disk class
///

#include <iostream>
#include <string>
#include <new>
#include "cdpdiskimpl.h"
#include "logger.h"
#include "portablehelpers.h"
#include "inmageex.h"


cdp_diskImpl_t::cdp_diskImpl_t(const std::string& name, const VolumeSummaries_t *pVolumeSummariesCache)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	try {
		Disk *p = new Disk(name, pVolumeSummariesCache);
		m_disk.reset(p);
	} catch (std::bad_alloc &e) {
		throw INMAGE_EX("Disk class allocation failed with error")(e.what())(name);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


cdp_diskImpl_t::~cdp_diskImpl_t()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}