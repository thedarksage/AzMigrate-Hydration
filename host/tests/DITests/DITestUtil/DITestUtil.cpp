// DIUtil.cpp : Defines the entry point for the console application.
//

#include <string>
#include <iostream>
#include <map>

#include "boost/tokenizer.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread.hpp"
#include "boost/algorithm/string/replace.hpp"
#include "boost/algorithm/string/erase.hpp"

#include "ILogger.h"
#include "Logger.h"
#include "CLocalMachineWithDisks.h"
#include "CVacpCrashMachineConsistencyProvider.h"
#include "CVacpAppMachineConsistencyProvider.h"
#include "CStorageOfflineMachineSnapshotProvider.h"
#include "CIStorageMD5ChecksumProvider.h"

#pragma comment(lib, "wbemuuid.lib")

/// \brief operation for which this util is invoked.
enum eDIUtilOp
{
	E_DIUTIL_OP_CC,     ///< crash consistency
	E_DIUTIL_OP_MD5,    ///< checksum
	E_DIUTIL_OP_ONLINE, ///< online
	E_DIUTIL_OP_APP,    ///< application consistency

	E_DIUTIL_OP_UNKNOWN ///< 'unknown': Must always be last
};

const char * const StrDIUtilOp[] = { "cc", "md5sum", "online", "app", "UNKNOWN" };

std::string GetStrDIUtilOp(const eDIUtilOp &op);

void DispUsage(const std::string &progname);
bool GetArgs(std::string &runDir, eDIUtilOp &op, CLocalMachineWithDisks::Disks_t &disks, unsigned &checksumBlockSize, bool &formatHashAsText, bool &separateHashesByNewline,
	int argc, char **argv);
void GetDiskNames(const CLocalMachineWithDisks::Disks_t &diskIds, ILoggerPtr pILogger, CLocalMachineWithDisks::Disks_t &diskNames);
int IssueCrashConsistency(IMachine::Ptr pIMachine, const std::string &dir, ILoggerPtr pILogger);
int IssueAppConsistency(IMachine::Ptr pIMachine, const std::string &dir, ILoggerPtr pILogger);
int GetMD5Sum(IStorageObjects &storageObjs, const unsigned &blockSize, const bool &formatHashAsText, const bool &separateHashesByNewline, const std::string &dir, ILoggerPtr pILogger);
int OnlineDisks(IStorageObjects &storageObjs);
void GetIStorageMD5Checksum(IStoragePtr pIStorage, const unsigned &blockSize, const bool &formatHashAsText, const bool &separateHashesByNewline, const std::string &dir, int &status, std::string &errMsg, ILoggerPtr pILogger);
boost::shared_ptr<IPlatformAPIs> GetPlatformAPIs(void);

int main(int argc, char **argv)
{
	std::string progname = argv[0];
	// Verify arguments
	if (argc == 1) {
		DispUsage(progname);
		return -1;
	}

	int status = -1;

	// Get args
	eDIUtilOp op;
	unsigned checksumBs;
	std::string runDir;
	bool formatHashAsText;
	bool separateHashesByNewline;
	CLocalMachineWithDisks::Disks_t diskIds;
	if (!GetArgs(runDir, op, diskIds, checksumBs, formatHashAsText, separateHashesByNewline, argc, argv)) {
		DispUsage(progname);
		return -1;
	}

	// Form the log name
	std::string logName = runDir;
	logName += '/';
	boost::filesystem::path p(progname);
	logName += boost::filesystem::basename(p);
	logName += '_';
	logName += GetStrDIUtilOp(op);
	for (CLocalMachineWithDisks::ConstDisksIter_t cit = diskIds.begin(); cit != diskIds.end(); cit++) {
		logName += '_';
		std::string d = *cit;
		boost::algorithm::replace_all(d, "/", "_");
		logName += d;
	}
	logName += ".log";

	std::cout << "run directory = " << runDir 
		<< ", operation = " << GetStrDIUtilOp(op) 
		<< ", checksum block size = " << checksumBs 
		<< ", formatHashAsText = " << formatHashAsText
		<< ", separateHashesByNewline = " << separateHashesByNewline
		<< ", log name = " << logName << '\n';

	try {
		// Initialize logger
		boost::filesystem::path file_path(logName);
		boost::filesystem::path dir_path(file_path.parent_path());
		if (!boost::filesystem::exists(dir_path))
			boost::filesystem::create_directories(dir_path);
		bool appendLog = true;
		ILoggerPtr pILogger(new CLogger(logName, appendLog));

		CLocalMachineWithDisks::Disks_t diskNames;
		GetDiskNames(diskIds, pILogger, diskNames);

		// Create the machine: Empty machine ID is fine for now.
		boost::shared_ptr<IPlatformAPIs> pPlatformApis = GetPlatformAPIs();
		IMachine::Ptr pIMachine(new CLocalMachineWithDisks(std::string(), diskNames, pPlatformApis.get(), pILogger));
        IStorageObjects o = pIMachine->GetStorageObjects();

		switch (op) {
		case E_DIUTIL_OP_CC:
			status = IssueCrashConsistency(pIMachine, runDir, pILogger);
			break;
		case E_DIUTIL_OP_MD5:
			status = GetMD5Sum(o, checksumBs, formatHashAsText, separateHashesByNewline, runDir, pILogger);
			break;
		case E_DIUTIL_OP_ONLINE:
			status = OnlineDisks(o);
			break;
		case E_DIUTIL_OP_APP:
			status = IssueAppConsistency(pIMachine, runDir, pILogger);
			break;
		}
	}
	catch (std::exception &e) {
		std::cerr << progname << " failed with exception message " << e.what() << '\n';
	}

	return status;
}

std::string GetStrDIUtilOp(const eDIUtilOp &op)
{
	return ((op < E_DIUTIL_OP_UNKNOWN) && (op >= E_DIUTIL_OP_CC) ? StrDIUtilOp[op] : "unknown");
}

void DispUsage(const std::string &progname)
{
	size_t dotPos = progname.find('.');
	std::string progNameInLog = (std::string::npos == dotPos) ? progname : progname.substr(0, dotPos);

	std::cerr << "Usage:\n\n";

	// Crash consistency option
	std::cerr << "Crash consistency option:\n";
	std::cerr << progname << " -d <run dir> -cc -disks <disk_id1[,disk_id2,...]>\n";
	std::cerr << "cc: crash consistency\n";
	std::cerr << "Marker id is printed in file cc_marker_id.txt under <run dir>\n";
	std::cerr << "Log file for this operation is " << progNameInLog << "_cc_<disk_id1[_disk_id2_...]>.log under <run dir>\n\n";

	// MD5 sum option
	std::cerr << "MD5sum option:\n";
	std::cerr << progname << " -d <run dir> -md5sum -disks <disk_id1[,disk_id2,...]> [-b <blocksize in bytes>] [-t] [-n]\n";
	std::cerr << "Default block size is 1048576 bytes\n";
	std::cerr << "-t: Writes checksums in text form\n";
	std::cerr << "-n: Applies only when -t is specified. Writes checksums in text form with each block's checksum separated by a newline\n";
	std::cerr << "The checksums are created in <disk_id (without {})>_md5sum_bs_<blocksize in bytes> files under <run dir>. Checksums are recorded consecutively for all blocks.\n";
	std::cerr << "Log file for this operation is " << progNameInLog << "_md5sum_<disk_id1[_disk_id2_...]>.log under <run dir>\n\n";

	// online disks option
	std::cerr << "Online disks option:\n";
	std::cerr << progname << " -d <run dir> -online -disks <disk_id1[,disk_id2,...]>\n";
	std::cerr << "Log file for this operation is " << progNameInLog << "_online_<disk_id1[_disk_id2_...]>.log under <run dir>\n\n";

	// app consistency option
	std::cerr << "App consistency option:\n";
	std::cerr << progname << " -d <run dir> -app\n";
	std::cerr << "app: application consistency\n";
	std::cerr << "Marker id is printed in file app_marker_id.txt under <run dir>\n";
	std::cerr << "Log file for this operation is " << progNameInLog << "_app.log under <run dir>\n";
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

bool GetArgs(std::string &runDir, eDIUtilOp &op, CLocalMachineWithDisks::Disks_t &disks, unsigned &checksumBlockSize, bool &formatHashAsText, bool &separateHashesByNewline,
	int argc, char **argv)
{
	const std::string RUN_DIR_OPT = "d";
	const std::string CC_OPT = "cc";
	const std::string DISKS_OPT = "disks";

	const std::string MD5_OPT = "md5sum";
	const std::string BS_OPT = "b";
	const std::string FORMAT_HASH_OPT = "t";
	const std::string NEWLINE_HASH_SEP_OPT = "n";

	const std::string ONLINE_OPT = "online";

	const std::string APP_OPT = "app";

	op = E_DIUTIL_OP_UNKNOWN;
	checksumBlockSize = 1024 * 1024; // 1MB default
	formatHashAsText = false;
	separateHashesByNewline = false;

	bool areArgsProper = true;
	while ((areArgsProper) && --argc > 0 && (*++argv)[0] == '-') {
		if (*++argv[0]) {
			if (RUN_DIR_OPT == *argv) {
				++argv;
				--argc;
				runDir = *argv ? *argv : std::string();
				if (runDir.empty()) {
					std::cerr << "Run Directory not specified.\n";
					areArgsProper = false;
				}
			} 
			else if (CC_OPT == *argv)
				op = E_DIUTIL_OP_CC;
			else if (DISKS_OPT == *argv) {
				++argv;
				--argc;
				FillDisks(*argv, disks);
				if (disks.empty()) {
					std::cerr << "Disks not specified.\n";
					areArgsProper = false;
				}
			}
			else if (MD5_OPT == *argv)
				op = E_DIUTIL_OP_MD5;
			else if (BS_OPT == *argv) {
				++argv;
				--argc;
				std::string strBsize = *argv ? *argv : std::string();
				checksumBlockSize = 0;
				checksumBlockSize = boost::lexical_cast<unsigned>(strBsize);
				if (0 == checksumBlockSize) {
					std::cerr << "Invalid block size specified.\n";
					areArgsProper = false;
				}
			}
			else if (FORMAT_HASH_OPT == *argv)
				formatHashAsText = true;
			else if (NEWLINE_HASH_SEP_OPT == *argv)
				separateHashesByNewline = true;
			else if (ONLINE_OPT == *argv)
				op = E_DIUTIL_OP_ONLINE;
			else if (APP_OPT == *argv)
				op = E_DIUTIL_OP_APP;
			else {
				std::cerr << "Invalid options specified.\n";
				areArgsProper = false;
			}
		}
	}
	
	// Run dir has to be present for all options.
	if (runDir.empty()) {
		std::cerr << "Run Directory not specified.\n";
		areArgsProper = false;
	}

	switch (op) {
	case E_DIUTIL_OP_CC:
	case E_DIUTIL_OP_MD5:    //fallthrough
	case E_DIUTIL_OP_ONLINE:
		if (disks.empty()) {
			// Disks have to be present for above options.
			std::cerr << "Disks not specified.\n";
			areArgsProper = false;
		}
		break;
	case E_DIUTIL_OP_APP:
		// no other check
		break;
	default:
		std::cerr << "Operation invalid.\n";
		areArgsProper = false;
		break;
	}

	return areArgsProper;
}

int IssueCrashConsistency(IMachine::Ptr pIMachine, const std::string &dir, ILoggerPtr pILogger)
{
	// Create the consistency provider
	IConsistencyProvider::Ptr pIConsistencyProvider(new CVacpCrashMachineConsistencyProvider(pIMachine, pILogger));

	// Create snapshot provider
	ISnapshotProvider::Ptr pISnapshotProvider(new CStorageOfflineMachineSnapshotProvider(pIMachine, pILogger));

	// Take snapshot
	//
	// Note: Dummy consistency point is needed here because taking consistency point is next step.
	pILogger->LogInfo("Taking source snapshot.\n");
	IConsistencyPoint::Ptr pDummyConsistentPoint;
	ISnapshot::Ptr pISourceSnapshot = pISnapshotProvider->CreateSnapshot(pDummyConsistentPoint);
	if (!pISourceSnapshot)
		throw ERROR_EXCEPTION << "Failed to take source snapshot point";

	// Create source consistency point
	pILogger->LogInfo("Taking consistency point.\n");
	IConsistencyPoint::Ptr pIConsistencyPoint = pIConsistencyProvider->CreateConsistencyPoint();
	if (!pIConsistencyPoint)
		throw ERROR_EXCEPTION << "Failed to take consistency point";

	std::string outFile = dir;
	outFile += '/';
	outFile += "cc_marker_id.txt";

	// Write the consistency marker GUID
	std::ofstream os;
	os.open(outFile.c_str(), ios::trunc);
	if (!os)
		throw ERROR_EXCEPTION << "Failed to open out file " << outFile;

	os << pIConsistencyPoint->GetID();

	if (!os)
		throw ERROR_EXCEPTION << "Failed to write cc marker to out file " << outFile;
	
	return 0;
}

int GetMD5Sum(IStorageObjects &storageObjs, const unsigned &blockSize, const bool &formatHashAsText, const bool &separateHashesByNewline, const std::string &dir, ILoggerPtr pILogger)
{
	int status = -1;

	// Fork checksum threads
	std::vector<int> statuses;
	std::vector<std::string> errorMessages;
	std::vector <boost::shared_ptr<boost::thread> > ts;
	for (IStorageObjectsIter it = storageObjs.begin() + 1; it != storageObjs.end(); it++) {
		std::vector<int>::iterator iSt = statuses.insert(statuses.end(), -1);
		std::vector<std::string>::iterator sErr = errorMessages.insert(errorMessages.end(), std::string());
		int &st = *iSt;
		std::string &errMsg = *sErr;
		boost::shared_ptr<boost::thread> pT(new boost::thread(&GetIStorageMD5Checksum, 
			*it, boost::cref(blockSize), boost::cref(formatHashAsText), boost::cref(separateHashesByNewline), boost::cref(dir), boost::ref(st), boost::ref(errMsg), pILogger)); //arguments
		ts.push_back(pT);
	}

	// Handle first one ourself
	IStoragePtr pIStorageFirst = *(storageObjs.begin());
	int statusFirst = -1;
	std::string errMsgFirst;
	GetIStorageMD5Checksum(pIStorageFirst, blockSize, formatHashAsText, boost::cref(separateHashesByNewline), dir, statusFirst, errMsgFirst, pILogger);

	// Join threads
	std::vector <boost::shared_ptr<boost::thread> >::iterator it = ts.begin();
	for ( /* empty */; it != ts.end(); it++) {
		boost::shared_ptr<boost::thread> &t = *it;
		t->join();
	}

	// Get statuses
	status = statusFirst;
	if (0 == status){
		// Look at other statuses as first status was successful
		std::vector<int>::iterator sIt = statuses.begin();
		std::vector<std::string>::iterator eIt = errorMessages.begin();
		for ( /* empty */; sIt != statuses.end(); sIt++, eIt++) {
			const int &s = *sIt;
			if (0 != s) {
				pILogger->LogError("%s\n", eIt->c_str());
				status = s;
				break;
			}
		}
	}

	return status;
}

void GetIStorageMD5Checksum(IStoragePtr pIStorage, const unsigned &blockSize, const bool &formatHashAsText, const bool &separateHashesByNewline, const std::string &dir, int &status, std::string &errMsg, ILoggerPtr pILogger)
{
	// Form the checksum file name
	std::string csFile = dir;
	csFile += '/';
    std::string sDeviceID(pIStorage->GetDeviceId());
	boost::algorithm::erase_all(sDeviceID, "{");
	boost::algorithm::erase_all(sDeviceID, "}");
	boost::algorithm::replace_all(sDeviceID, "/", "_");
	csFile += sDeviceID;
	csFile += "_md5sum_bs_";
	csFile += boost::lexical_cast<std::string>(blockSize);

	status = -1;
	try {
		IChecksumProvider::Ptr pChecksumProvider(new CIStorageMD5ChecksumProvider(pIStorage));
		pChecksumProvider->GetChecksumsInFile(blockSize, csFile, formatHashAsText, separateHashesByNewline);
		status = 0;
	}
	catch (std::exception &e) {
		errMsg = e.what();
	}
}

int OnlineDisks(IStorageObjects &storageObjs)
{
	const bool READONLY = false;
	for (IStorageObjectsIter it = storageObjs.begin(); it != storageObjs.end(); it++) {
		IStoragePtr pIStorage = *it;
		pIStorage->Online(READONLY);
	}

	return 0;
}

int IssueAppConsistency(IMachine::Ptr pIMachine, const std::string &dir, ILoggerPtr pILogger)
{
	// Create the consistency provider
	IConsistencyProvider::Ptr pIConsistencyProvider(new CVacpAppMachineConsistencyProvider(pIMachine, pILogger));

	// Create consistency point
	pILogger->LogInfo("Taking consistency point.\n");
	IConsistencyPoint::Ptr pIConsistencyPoint = pIConsistencyProvider->CreateConsistencyPoint();
	if (!pIConsistencyPoint)
		throw ERROR_EXCEPTION << "Failed to take consistency point";

	std::string outFile = dir;
	outFile += '/';
	outFile += "app_marker_id.txt";

	// Write the consistency marker GUID
	std::ofstream os;
	os.open(outFile.c_str(), ios::trunc);
	if (!os)
		throw ERROR_EXCEPTION << "Failed to open out file " << outFile;

	os << pIConsistencyPoint->GetID();

	if (!os)
		throw ERROR_EXCEPTION << "Failed to write cc marker to out file " << outFile;

	return 0;
}
