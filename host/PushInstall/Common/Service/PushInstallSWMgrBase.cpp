///
/// \file PushInstallSWMgrBase.cpp
///
/// \brief
///

#include <string>

#include <boost/thread/mutex.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

#include "PushInstallSWMgrBase.h"
#include "../common/version.h"
#include "errorexception.h"
#include "setpermissions.h"

#include <chrono>
#include <thread>

using namespace PI;

PushInstallSWMgrBase::PushInstallSWMgrBase(PushInstallCommunicationBasePtr & proxy, PushConfigPtr & config)
{
	m_config = config;
	m_proxy = proxy;
	uaFolder = m_config->localRepositoryUAPath();
	pushclientFolder = m_config->localRepositoryPushclientPath();
	patchFolder = m_config->localRepositoryPatchPath();

	InitializeLocalRepositorySoftwares();
}

void PushInstallSWMgrBase::SetFolderPermissions(const std::string & folderPath, int flags)
{
	try
	{
		securitylib::setPermissions(folderPath, flags);
	}
	catch (ErrorException & eex)
	{
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS)
		{
			// When this error is thrown due to race condition, we will wait for 500 ms and retry.
			// Not calling the same function recursively so that, control will not be stuck in case
			// of valid ERROR_ALREADY_EXISTS failure (we retry only once).
			DebugPrintf(
				SV_LOG_ERROR,
				"Failed to create folder %s due to conflict with another process. Will retry after 0.5 seconds.\n",
				folderPath.c_str());
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			securitylib::setPermissions(folderPath, flags);
		}
		else
		{
			throw;
		}
	}
}

void PushInstallSWMgrBase::InitializeLocalRepositorySoftwares()
{
	std::string uaFolderPath = uaFolder;
	std::string pushClientFolderPath = pushclientFolder;
	std::string patchFolderPath = patchFolder;

	SetFolderPermissions(uaFolderPath, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	SetFolderPermissions(pushClientFolderPath, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	if (m_config->stackType() == StackType::CS)
	{
		SetFolderPermissions(patchFolderPath, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	}

	std::string uaFoldertmp = uaFolderPath + remoteApiLib::pathSeperator() + "tmp";
	std::string pushclientFoldertmp = pushClientFolderPath + remoteApiLib::pathSeperator() + "tmp";
	std::string patchFoldertmp = patchFolderPath + remoteApiLib::pathSeperator() + "tmp";

	SetFolderPermissions(uaFoldertmp, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	SetFolderPermissions(pushclientFoldertmp, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	if (m_config->stackType() == StackType::CS)
	{
		SetFolderPermissions(patchFoldertmp, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
	}

	boost::filesystem::directory_iterator uaiter(uaFolderPath);
	boost::filesystem::directory_iterator uaiterEnd;

	for (/* empty */; uaiter != uaiterEnd; ++uaiter) {
		if (boost::filesystem::is_regular_file(uaiter->status())) {
			std::string sw = (*uaiter).path().string();
			boost::replace_all(sw, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
			localRepositorySoftwares.push_back(sw);
		}
	}

	boost::filesystem::directory_iterator pushclientiter(pushClientFolderPath);
	boost::filesystem::directory_iterator pushclientiterEnd;

	for (/* empty */; pushclientiter != pushclientiterEnd; ++pushclientiter) {
		if (boost::filesystem::is_regular_file(pushclientiter->status())) {
			std::string sw = (*pushclientiter).path().string();
			boost::replace_all(sw, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
			localRepositorySoftwares.push_back(sw);
		}
	}

	if (m_config->stackType() == StackType::CS)
	{
		boost::filesystem::directory_iterator patchiter(patchFolderPath);
		boost::filesystem::directory_iterator patchiterEnd;

		for (/* empty */; patchiter != patchiterEnd; ++patchiter) {
			if (boost::filesystem::is_regular_file(patchiter->status())) {
				std::string sw = (*patchiter).path().string();
				boost::replace_all(sw, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
				localRepositorySoftwares.push_back(sw);
			}
		}
	}

	// exception, windows push client is available in install path
	// this needs to be fixed in the installer, then we can remove this below code block
#ifdef WIN32

	// in cs stack, this file would be present and considered as default path for windows push client
	// in rcm stack, push client for windows would be packaged as part of push clients folder
	// so, in rcm stack, this file path will not be present
	std::string pushClientPath = m_config->installPath() + remoteApiLib::pathSeperator() + "pushClient.exe";
	if (boost::filesystem::exists(pushClientPath))
	{
		localRepositorySoftwares.push_back(pushClientPath);
	}
#endif
}

std::string PushInstallSWMgrBase::localUAPath(const std::string &osname, const std::string & downloadUrl)
{
	// algorithm
	// if downloadUrl is empty or contains build notavailable at cs, find the default ua path
	//  else , caculate the ua path from url
	//  return local path

	std::string localPath;

	if (downloadUrl.empty() || (downloadUrl.find("BuildIsNotAvailableAtCX") != std::string::npos)) {
		localPath = defaultUAPath(osname);
	}
	else {
		

		localPath = uaFolder + remoteApiLib::pathSeperator() + getSoftwareNameFromDownloadUrl(downloadUrl);
	}

	if (localPath.empty()) {
		throw ERROR_EXCEPTION << "Mobility agent software for " << osname << " is not available in both remote and local repository";
	}

	boost::replace_all(localPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
	return localPath;
}

std::string PushInstallSWMgrBase::localPushClientPath(const std::string &osname, const std::string & downloadUrl)
{
	// algorithm
	// if downloadUrl is empty or contains build notavailable at cs, find the default ua path
	//  else , caculate the ua path from url
	//  return local path

	std::string localPath;

	if (downloadUrl.empty() || (downloadUrl.find("BuildIsNotAvailableAtCX") != std::string::npos)) {
		localPath = defaultPushClientPath(osname);
	}
	else {
		size_t idx = downloadUrl.find_last_of("/");
		if (idx == std::string::npos) {
			throw ERROR_EXCEPTION << "invalid url " << downloadUrl << " for downloading software";
		}

		localPath = pushclientFolder + "/" + getSoftwareNameFromDownloadUrl(downloadUrl);
	}

	if (localPath.empty()) {
		throw ERROR_EXCEPTION << "push client software for " << osname << " is not available in both remote and local repository";
	}

	boost::replace_all(localPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
	return localPath;
}

std::string PushInstallSWMgrBase::localPatchPath(const std::string & downloadUrl)
{
	// algorithm
	// if downloadUrl is empty or contains build notavailable at cs, throw exception
	//  else , caculate the ua path from url
	//  return local path

	std::string localPath;

	if (downloadUrl.empty() || (downloadUrl.find("BuildIsNotAvailableAtCX") != std::string::npos)) {
		throw ERROR_EXCEPTION << "invalid url " << downloadUrl << " for downloading software";
	}

	size_t idx = downloadUrl.find_last_of("/");
	if (idx == std::string::npos) {
		throw ERROR_EXCEPTION << "invalid url " << downloadUrl << " for downloading software";
	}

	localPath = patchFolder + "/" + getSoftwareNameFromDownloadUrl(downloadUrl);

	boost::replace_all(localPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
	return localPath;
}

void PushInstallSWMgrBase::downloadSW(const std::string & downloadUrl, const std::string & localPath)
{
	// download software
	// acquire mutex
	// add to local repository
	// release mutex

	std::string exceptionduringDownload;
	bool downloadCompleted = false;

	std::string tmpPath;

	try {

		std::string::size_type idx = localPath.find_last_of(remoteApiLib::pathSeperator());
		if (std::string::npos != idx) {
			tmpPath = localPath.substr(0, idx);
		}
		tmpPath += remoteApiLib::pathSeperator() + "tmp" + remoteApiLib::pathSeperator() + localPath.substr(idx + 1);

		m_proxy->DownloadInstallerFile(downloadUrl, tmpPath);
		boost::filesystem::remove(localPath);
		boost::filesystem::rename(tmpPath, localPath);
		downloadCompleted = true;
	}
	catch (ErrorException & e)
	{
		exceptionduringDownload = e.what();
		boost::filesystem::remove(tmpPath);
	}
	catch (...) {
		exceptionduringDownload = "unhandled exception";
		boost::filesystem::remove(tmpPath);
	}

	DebugPrintf(SV_LOG_DEBUG, "downloadSW %s completed acquiring mutex", localPath.c_str());
	boost::unique_lock<boost::mutex> lock(m_monitordownloadMutex);
	DebugPrintf(SV_LOG_DEBUG, "downloadSW %s completed acquired mutex", localPath.c_str());

	std::vector<std::string>::iterator iter = std::find(downloadsinProgress.begin(), downloadsinProgress.end(), downloadUrl);
	if (iter == downloadsinProgress.end()) {
		throw ERROR_EXCEPTION << "internal error, downloading sofware " << downloadUrl << " without proper synchronization ";
	}

	downloadsinProgress.erase(iter);

	if (downloadCompleted) {

		if (swAvailable(localPath)) {
			throw ERROR_EXCEPTION << "internal error , trying to download an already downloaded software : " << localPath;
		}

		localRepositorySoftwares.push_back(localPath);
	}

	m_monitordownloadCv.notify_all();

	if (!downloadCompleted) {

		std::string protocol = "https://";
		if (m_config->secure()) {
			protocol = "http://";
		}
		throw ERROR_EXCEPTION << "download of " << protocol << m_proxy->csip() << ":" << m_proxy->csport() + "/" + downloadUrl + " to "
			+ localPath << " failed with error " << exceptionduringDownload;
	}

	DebugPrintf(SV_LOG_DEBUG, "downloadSW %s completed", localPath.c_str());
}

void PushInstallSWMgrBase::getCommonDefaultUAandPushClientPath(const std::string & osname, std::string & pushClientPath, std::string & uaPath)
{
	pushClientPath = "";
	uaPath = "";
	std::string SWMatch = "UA_";
	std::vector<std::string>::iterator iter(localRepositorySoftwares.begin());
	std::vector<std::string>::iterator iterEnd(localRepositorySoftwares.end());

	for (; iter != iterEnd; ++iter) {
		if (((*iter).find(osname) != std::string::npos) &&
			((*iter).find(SWMatch) != std::string::npos)) {
			if (uaPath.compare(*iter) < 0)
			{
				// file name would be of form Microsoft-ASR_UA_9.35.0.0_DEBIAN8-64_GA_30Jun2020_release.tar.gz or Microsoft-ASR_UA_9.35.0.0_Windows_GA_30Jun2020_release.exe
				size_t index = iter->find(SWMatch);
				// remaniningStr would be of form 9.35.0.0_DEBIAN8-64_GA_30Jun2020_release.tar.gz or 9.35.0.0_Windows_GA_30Jun2020_release.exe
				std::string remainingStr = iter->substr(index + SWMatch.length());
				index = remainingStr.find('_');
				std::string versionStr = remainingStr.substr(0, index); // version would be of form 9.35.0.0

				// check for push client with the same version in the repository
				std::vector<std::string>::iterator pushClientIter(localRepositorySoftwares.begin());
				std::vector<std::string>::iterator pushClientEnd(localRepositorySoftwares.end());
				for (; pushClientIter != pushClientEnd; ++pushClientIter) {
					if (pushClientIter->find(versionStr) != std::string::npos &&
						pushClientIter->find("pushinstallclient") != std::string::npos &&
						pushClientIter->find(osname) != std::string::npos)
					{
						uaPath = *iter;
						pushClientPath = *pushClientIter;
						break;
					}
				}
			}
		}
	}
}

std::string PushInstallSWMgrBase::defaultUAPath(const std::string & osname)
{
	// sw match here is "UA_" and the file name should contain os name.
	// in cs stack, only one file would be present which satisfy these conditions
	// in rcm stack, the latest unified agent matching this os will be picked up

	std::string localpath = "";

	if (m_config->stackType() == StackType::CS)
	{
		std::string SWMatch = "UA_";

		std::vector<std::string>::iterator iter(localRepositorySoftwares.begin());
		std::vector<std::string>::iterator iterEnd(localRepositorySoftwares.end());

		for (; iter != iterEnd; ++iter) {
			if (((*iter).find(osname) != std::string::npos) &&
				((*iter).find(SWMatch) != std::string::npos)) {
				if (localpath.compare(*iter) < 0)
				{
					localpath = *iter;
				}
			}
		}
	}
	else
	{
		std::string pushClientPath = "";
		getCommonDefaultUAandPushClientPath(osname, pushClientPath, localpath);
	}

	return localpath;
}

std::string PushInstallSWMgrBase::defaultPushClientPath(const std::string &osname)
{
	// sw match here is "pushinstallclient" and the file name should contain os name.
	// in cs stack, only one file would be present which satisfy these conditions
	// in rcm stack, the latest push client matching this os will be picked up
	
	std::string localpath = "";
	
	if (m_config->stackType() == StackType::CS)
	{
		std::string SWMatch = "pushinstallclient";

		std::vector<std::string>::iterator iter(localRepositorySoftwares.begin());
		std::vector<std::string>::iterator iterEnd(localRepositorySoftwares.end());

		for (; iter != iterEnd; ++iter) {

			// exception, windows push client is available in install path and it does not have osname in it
			// this needs to be fixed in the installer, then we can remove this below #ifdef code block
#ifdef WIN32
			// in cs stack, push client is packaged as "pushClient.exe", hence this condition
			if (osname == "Win") {
				if ((*iter).find("pushClient.exe") != std::string::npos) {
					localpath = *iter;
					break;
				}
			}
#endif

			if (((*iter).find(osname) != std::string::npos) &&
				((*iter).find(SWMatch) != std::string::npos)) {
				if (localpath.compare(*iter) < 0)
				{
					localpath = *iter;
				}
			}
		}
	}
	else
	{
		std::string uaPath = "";
		getCommonDefaultUAandPushClientPath(osname, localpath, uaPath);
	}

	return localpath;
}

bool PushInstallSWMgrBase::swAvailable(const std::string & swPath)
{
	std::vector<std::string>::iterator iter = localRepositorySoftwares.begin();
	DebugPrintf(SV_LOG_DEBUG, "checking for software %s in local repository\n", swPath.c_str());
	for (; iter != localRepositorySoftwares.end(); ++iter) {
		DebugPrintf(SV_LOG_DEBUG, "%s\n", (*iter).c_str());
	}
	return (std::find(localRepositorySoftwares.begin(), localRepositorySoftwares.end(), swPath) != localRepositorySoftwares.end());
}

bool PushInstallSWMgrBase::downloadInProgress(const std::string & downloadUrl)
{
	std::vector<std::string>::iterator iter = downloadsinProgress.begin();
	DebugPrintf(SV_LOG_DEBUG, "checking fo in-progress downloads %s\n", downloadUrl.c_str());
	for (; iter != downloadsinProgress.end(); ++iter) {
		DebugPrintf(SV_LOG_DEBUG, "%s\n", (*iter).c_str());
	}

	return (std::find(downloadsinProgress.begin(), downloadsinProgress.end(), downloadUrl) != downloadsinProgress.end());
}

void  PushInstallSWMgrBase::CheckAndDownloadSW(const std::string & downloadUrl, const std::string & localPath)
{
	// acquire mutex
	//  while !swAvailable
	//    if downloadinprogress
	//       waiton cv
	//    else
	//       download
	//   

	if (swAvailable(localPath))
		return;

	while (1) {

		boost::unique_lock<boost::mutex> lock(m_monitordownloadMutex);

		if (boost::filesystem::exists(localPath)) {
			localRepositorySoftwares.push_back(localPath);
			break;
		}

		if (swAvailable(localPath)) {
			break;
		}

		if (downloadInProgress(downloadUrl)) {
			m_monitordownloadCv.wait(lock);
		}
		else {
			downloadsinProgress.push_back(downloadUrl);
			lock.unlock();
			downloadSW(downloadUrl, localPath);
		}
	}
}

bool PushInstallSWMgrBase::SWAlreadyVerified(const std::string & sw)
{
	boost::mutex::scoped_lock guard(m_swVerificationMutex);
	return (std::find(verifiedSofwares.begin(), verifiedSofwares.end(), sw) != verifiedSofwares.end());
}

void PushInstallSWMgrBase::RemoveFromAvailableSwList(const std::string &swPath)
{
	boost::mutex::scoped_lock guard(m_monitordownloadMutex);
	std::vector<std::string>::iterator iter = std::find(localRepositorySoftwares.begin(), localRepositorySoftwares.end(), swPath);
	if (iter == localRepositorySoftwares.end()) {
		throw ERROR_EXCEPTION << "internal error, trying to remove a non existent entry  " << swPath << " from in memory list of available softwares";
	}

	localRepositorySoftwares.erase(iter);
}

std::string PushInstallSWMgrBase::downloadUA(const std::string & osname, const std::string & downloadUrl, bool verifysignature)
{
	// algorithm
	// find the local repository path
	// if sw is  notavailable in local repository, download the sw 	
	//  if verifysignature, verify signature 
	// return local sw path


	std::string localPath = localUAPath(osname, downloadUrl);
	CheckAndDownloadSW(downloadUrl, localPath);

	if (verifysignature) {
		verifyUA(osname, downloadUrl);
	}

	return localPath;

}

std::string PushInstallSWMgrBase::downloadPushClient(const std::string & osname, const std::string & downloadUrl, bool verifysignature)
{
	// algorithm
	// find the local repository path
	// if sw is  notavailable in local repository, download the sw 	
	//  if verifysignature, verify signature 
	// return local sw path


	std::string localPath = localPushClientPath(osname, downloadUrl);
	CheckAndDownloadSW(downloadUrl, localPath);

	if (verifysignature) {
		verifyPushClient(osname, downloadUrl);
	}

	return localPath;

}


std::string PushInstallSWMgrBase::downloadPatch(const std::string & osname, const std::string & downloadUrl, bool verifysignature)
{
	// algorithm
	// find the local repository path
	// if sw is  notavailable in local repository, download the sw 	
	//  if verifysignature, verify signature 
	// return local sw path


	std::string localPath = localPatchPath(downloadUrl);
	CheckAndDownloadSW(downloadUrl, localPath);

	if (verifysignature) {
		verifyPatch(osname, downloadUrl);
	}

	return localPath;
}

void PushInstallSWMgrBase::verifyUA(const std::string & osname, const std::string & downloadUrl)
{

	std::string localPath = localUAPath(osname, downloadUrl);


	try {
		verifySW(osname, localPath);

		boost::mutex::scoped_lock guard(m_swVerificationMutex);
		if (std::find(verifiedSofwares.begin(), verifiedSofwares.end(), localPath) != verifiedSofwares.end())
			return;

		verifiedSofwares.push_back(localPath);
	}
	catch (ErrorException & e)
	{
		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed with error %s, removing software from available list\n", localPath.c_str(), e.what());
		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}
	catch (...) {

		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed, removing software from available list\n", localPath.c_str());

		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}
}

void PushInstallSWMgrBase::verifyPushClient(const std::string & osname, const std::string & downloadUrl)
{

	std::string localPath = localPushClientPath(osname, downloadUrl);

	try {
		verifySW(osname, localPath);

		boost::mutex::scoped_lock guard(m_swVerificationMutex);
		if (std::find(verifiedSofwares.begin(), verifiedSofwares.end(), localPath) != verifiedSofwares.end())
			return;

		verifiedSofwares.push_back(localPath);
	}
	catch (ErrorException & e)
	{
		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed with error %s, removing software from available list\n", localPath.c_str(), e.what());
		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}
	catch (...) {

		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed, removing software from available list\n", localPath.c_str());

		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}

}

void PushInstallSWMgrBase::verifyPatch(const std::string & osname, const std::string & downloadUrl)
{

	std::string localPath = localPatchPath(downloadUrl);

	try {
		verifySW(osname, localPath);

		boost::mutex::scoped_lock guard(m_swVerificationMutex);
		if (std::find(verifiedSofwares.begin(), verifiedSofwares.end(), localPath) != verifiedSofwares.end())
			return;

		verifiedSofwares.push_back(localPath);
	}
	catch (ErrorException & e)
	{
		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed with error %s, removing software from available list\n", localPath.c_str(), e.what());
		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}
	catch (...) {

		DebugPrintf(SV_LOG_ERROR, "software verification of %s failed, removing software from available list\n", localPath.c_str());

		//remove corrupt software
		boost::filesystem::remove(localPath);
		// remove from in memory list
		RemoveFromAvailableSwList(localPath);

		throw;
	}

}
