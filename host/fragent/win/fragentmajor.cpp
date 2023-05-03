
///
/// \file fragentmajor.cpp<2>
///
/// \brief
///

#include <boost/filesystem/path.hpp>

#include "fragent.h"

/*
 * CFileReplicationAgent::getFxAllowedDirsPath
 *
 * creates a FX allowed dirs path
 */
void CFileReplicationAgent::getFxAllowedDirsPath(boost::filesystem::path& fxPath)
{
    fxPath = m_sv_host_agent_params.AgentInstallPath;
    fxPath /= "transport";
    fxPath /= "fxallowed";
}
