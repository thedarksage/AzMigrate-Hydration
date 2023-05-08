#include "fileclusterinformerimp.h"


FileClusterInformerImp::FileClusterInformerImp()
 : m_pFileClusterInquirer(0)
{
}


FileClusterInformerImp::~FileClusterInformerImp()
{
    if (m_pFileClusterInquirer)
    {
        delete m_pFileClusterInquirer;
        m_pFileClusterInquirer = 0;
    }
}


bool FileClusterInformerImp::Init(const FileDetails_t &fd)
{
    bool brval = (ACE_INVALID_HANDLE != fd.m_Handle);
  
    if (brval)
    {
        brval = InitializeFileClusterInquirer(fd);
    }
    else
    {
        m_ErrorMessage = "invalid file handle supplied.";
    }

    return brval;
}


bool FileClusterInformerImp::GetRanges(FileClusterRanges_t &ranges)
{
    bool brval = true;

    if (m_pFileClusterInquirer)
    {
        brval = m_pFileClusterInquirer->InquireRanges(ranges);
        if (!brval)
        {
            m_ErrorMessage = std::string("Failed to inquire ranges with error ") + m_pFileClusterInquirer->GetErrorMessage();
        }
    } 
    else
    {
        ranges.m_FeatureSupportState = E_FEATURENOTSUPPORTED;
    }

    return brval;
}


std::string FileClusterInformerImp::GetErrorMessage(void)
{
    return m_ErrorMessage;
}
