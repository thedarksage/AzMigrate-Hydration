#include "fileclusterinformerimp.h"
#include "inmstrcmp.h"
#include "ntfsfileclusterinquirer.h"
#include "portablehelpers.h"

bool FileClusterInformerImp::InitializeFileClusterInquirer(const FileDetails_t &fd)
{
    bool brval = true;

    if (0 == InmStrCmp<NoCaseCharCmp>(fd.m_FileSystem, NTFS))
    {
        m_pFileClusterInquirer = new (std::nothrow) NtfsFileClusterInquirer(fd);
        brval = m_pFileClusterInquirer;
        if (false == brval)
        {
            m_ErrorMessage = "Could not allocate memory for ntfs file cluster inquirer.";
        }
    }

    return brval;
}
