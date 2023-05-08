#ifndef FILE_CLUSTER_INQUIRER_H
#define FILE_CLUSTER_INQUIRER_H

#include <string>
#include "svtypes.h"
#include "fileclusterinformer.h"

class FileClusterInquirer
{
public:
    /* returns false on failure for now */
    virtual bool InquireRanges(FileClusterInformer::FileClusterRanges_t &ranges) = 0;
    virtual std::string GetErrorMessage(void) = 0;
   
    virtual ~FileClusterInquirer() {}
};

#endif /* FILE_CLUSTER_INQUIRER_H */
