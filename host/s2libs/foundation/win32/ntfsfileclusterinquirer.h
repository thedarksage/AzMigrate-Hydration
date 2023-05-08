#ifndef NTFS_FILE_CLUSTER_INQUIRER_H
#define NTFS_FILE_CLUSTER_INQUIRER_H

#include <windows.h>
#include <WinIoCtl.h>
#include "fileclusterinformer.h"
#include "fileclusterinquirer.h"

class NtfsFileClusterInquirer : public FileClusterInquirer
{
public:
	NtfsFileClusterInquirer(const FileClusterInformer::FileDetails_t &fd);
    ~NtfsFileClusterInquirer();
    bool InquireRanges(FileClusterInformer::FileClusterRanges_t &ranges);
    std::string GetErrorMessage(void);

private:
    void RecordRanges(STARTING_VCN_INPUT_BUFFER &s, RETRIEVAL_POINTERS_BUFFER &r, FileClusterInformer::FileClusterRanges_t &ranges);

private:
    FileClusterInformer::FileDetails_t m_Fd;
    std::string m_ErrorMessage;
};

#endif /* NTFS_FILE_CLUSTER_INQUIRER_H */
