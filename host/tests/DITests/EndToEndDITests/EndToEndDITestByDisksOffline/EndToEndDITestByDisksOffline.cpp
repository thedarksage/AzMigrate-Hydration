// EndToEndDITestByDisksOffline.cpp : Defines the entry point for the console application.
//

#include <string>
#include <iostream>
#include <ctime>

#include "boost/tokenizer.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/algorithm/string/erase.hpp"
#include "boost/algorithm/string/replace.hpp"

#include "ILogger.h"
#include "Logger.h"
#include "CEndToEndDITest.h"
#include "CLocalMachineWithDisks.h"
#include "CVacpCrashMachineConsistencyProvider.h"
#include "CStorageOfflineMachineSnapshotProvider.h"
#include "CTargetSnapshotProvider.h"
#include "CWinToWinRemoteProcessProvider.h"
#include "CLocalProcessProvider.h"
#include "CISnapshotDIValidator.h"
#include "CIStorageDIValidatorChecksumProviderFactory.h"

void DispUsage(const std::string &progname);
void FillDisks(const std::string &disknames, CLocalMachineWithDisks::Disks_t &disks);
void FillPSDetails(const std::string &psDetails, std::string &ip, std::string &username, std::string &passwd);
bool GetArgs(std::string &vmId, CLocalMachineWithDisks::Disks_t &disks, std::string &psIp, std::string &psUser, std::string &psPasswd,
	std::string &testDir, int argc, char **argv);

int main(int argc, char **argv)
{
	std::string progname = argv[0];
	// Verify arguments
	if (argc == 1) {
		DispUsage(progname);
		return -1;
	}

	// Get args
	std::string vmId;
	CLocalMachineWithDisks::Disks_t disks;
	std::string psIp, psUser, psPasswd;
	std::string testDir;
	if (!GetArgs(vmId, disks, psIp, psUser, psPasswd, testDir, argc, argv)) {
		DispUsage(progname);
		return -1;
	}

	int status = -1;
	try {
		// Initialize logger
		std::string logName = testDir;
		logName += "/test.log";
		ILoggerPtr pILogger(new CLogger(logName));

		// Create the source machine 
		CWin32APIs apis;
		IMachine::Ptr pISourceMachine(new CLocalMachineWithDisks(vmId, disks, &apis, pILogger));

		// Create the consistency provider
		IConsistencyProvider::Ptr pISourceConsistencyProvider(new CVacpCrashMachineConsistencyProvider(pISourceMachine, pILogger));

		// Create source snapshot provider
		ISnapshotProvider::Ptr pISourceSnapshotProvider(new CStorageOfflineMachineSnapshotProvider(pISourceMachine, pILogger));

		// Create local process provider
		CLocalProcessProvider::Ptr pCLocalProcessProvider(new CLocalProcessProvider(pILogger));

		// Create remote process provider
		IProcessProvider::Ptr pIRemoteProcessProvider(new CWinToWinRemoteProcessProvider(psIp, psUser, psPasswd, pCLocalProcessProvider, pILogger));

		// Create target snapshot provider
		ISnapshotProvider::Ptr pITargetSnapshotProvider(new CTargetSnapshotProvider(pISourceMachine, pIRemoteProcessProvider, testDir, pILogger));

		// Create the checksums factory
		IStorageDIValidatorChecksumProviderFactory::Ptr pIChecksumFactory(new CIStorageDIValidatorChecksumProviderFactory(pIRemoteProcessProvider));

		// Create snapshot validator
		ISnapshotDIValidator::Ptr pISnapshotDIValidator(new CISnapshotDIValidator(pIChecksumFactory, testDir, 1024 * 1024, pILogger));

		// Create the test
		ITest::Ptr pITest(new CEndToEndDITest(pISourceMachine, pISourceConsistencyProvider, pISourceSnapshotProvider, pITargetSnapshotProvider, pISnapshotDIValidator, testDir, pILogger));

		pITest->run();

		status = (ITest::PASS == pITest->GetStatus()) ? 0 : -1;
		if (-1 == status)
			std::cerr << "DI did not match. Check logs in " << testDir << '\n';
	}
	catch (std::exception &e) {
		std::cerr << "Test failed with error message " << e.what() << '\n';
	}

	return status;
}

void DispUsage(const std::string &progname)
{
	std::cerr << "Usage:\n";
	std::cerr << progname << " -i <vmid> -d <disk1[,disk2,...]> -p <PS ip, PS username, PS password> -f <folder shared with PS>\n";
}

void FillDisks(const std::string &disknames, CLocalMachineWithDisks::Disks_t &disks)
{
	boost::char_separator<char> sep(",");
	boost::tokenizer<boost::char_separator<char> > tokens(disknames, sep);
	boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
	std::cout << "Disks:\n";
	for (/* empty */; it != tokens.end(); it++) {
		std::cout << *it << '\n';
		disks.push_back(*it);
	}
}

void FillPSDetails(const std::string &psDetails, std::string &ip, std::string &username, std::string &passwd)
{
	boost::char_separator<char> sep(",");
	boost::tokenizer<boost::char_separator<char> > tokens(psDetails, sep);
	boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
	unsigned nTokens = 0;
	for (/* empty */; it != tokens.end(); it++)
		nTokens++;
	if (nTokens == 3) {
		it = tokens.begin();

		ip = *it;
		std::cout << "PS ip = " << ip << '\n';
		it++;

		username = *it;
		std::cout << "PS Username = " << username << '\n';
		it++;

		passwd = *it;
		std::cout << "PS Password = " << passwd << '\n';
	}
}

bool GetArgs(std::string &vmId, CLocalMachineWithDisks::Disks_t &disks, std::string &psIp, std::string &psUser, std::string &psPasswd,
	std::string &testDir, int argc, char **argv)
{
	char c;
	bool areArgsProper = true;
	while ((areArgsProper) && --argc > 0 && (*++argv)[0] == '-') {
		bool breakout = false;
		while (!breakout && (c = *++argv[0])) {
			switch (c) {
			case 'i':
				++argv;
				--argc;
				vmId = *argv ? *argv : std::string();
				breakout = true;
				break;
			case 'd':
				++argv;
				--argc;
				FillDisks(*argv, disks);
				breakout = true;
				break;
			case 'p':
				++argv;
				--argc;
				FillPSDetails(*argv, psIp, psUser, psPasswd);
				breakout = true;
				break;
			case 'f':
				++argv;
				--argc;
				testDir = *argv ? *argv : std::string();
				breakout = true;
				break;
			default:
				std::cerr << "Invalid options specified.\n";
				areArgsProper = false;
				breakout = true;
				break;
			}
		}
	}

	std::cout << "VmId: " << vmId << '\n';
	std::cout << "Folder shared with PS: " << testDir << '\n';

	if (vmId.empty()) {
		std::cerr << "VmId not specified.\n";
		areArgsProper = false;
	}

	if (disks.empty()) {
		std::cerr << "Disks not specified.\n";
		areArgsProper = false;
	}

	if (psIp.empty() || psUser.empty() || psPasswd.empty()) {
		std::cerr << "PS details not specified.\n";
		areArgsProper = false;
	}

	if (testDir.empty()) {
		std::cerr << "Folder shared with PS not specified.\n";
		areArgsProper = false;
	}
	else {
		testDir += "/EndToEndDITestByDisksOffline/";

		// Put time
		time_t t = time(NULL);
		std::string st = ctime(&t);
		boost::algorithm::erase_all(st, "\n");
		boost::algorithm::replace_all(st, " ", "_");
		boost::algorithm::replace_all(st, "\t", "_");
		boost::algorithm::replace_all(st, ":", "_");
		std::cout << "time string = " << st << '\n';

		testDir += st;

		boost::filesystem::path dir_path(testDir);
		if (!boost::filesystem::exists(dir_path))
			boost::filesystem::create_directories(dir_path);
		std::cout << "Test folder: " << testDir << '\n';
	}

	return areArgsProper;
}