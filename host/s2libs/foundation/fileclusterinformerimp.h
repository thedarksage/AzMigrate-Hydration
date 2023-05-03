#ifndef FILE_CLUSTER_INFORMERIMP_H
#define FILE_CLUSTER_INFORMERIMP_H

#include "fileclusterinformer.h"
#include "fileclusterinquirer.h"

class FileClusterInformerImp : public FileClusterInformer
{
public:
    FileClusterInformerImp();
    ~FileClusterInformerImp();
    bool Init(const FileDetails_t &fd);
    bool GetRanges(FileClusterRanges_t &ranges);
    std::string GetErrorMessage(void);

private:
    bool InitializeFileClusterInquirer(const FileDetails_t &fd);

private:
    std::string m_ErrorMessage;
    FileClusterInquirer *m_pFileClusterInquirer;
};

#endif /* FILE_CLUSTER_INFORMERIMP_H */
