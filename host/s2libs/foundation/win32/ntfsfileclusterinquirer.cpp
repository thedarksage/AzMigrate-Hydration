#include <sstream>
#include "ntfsfileclusterinquirer.h"


NtfsFileClusterInquirer::NtfsFileClusterInquirer(const FileClusterInformer::FileDetails_t &fd)
: m_Fd(fd)
{
}


NtfsFileClusterInquirer::~NtfsFileClusterInquirer()
{
}


bool NtfsFileClusterInquirer::InquireRanges(FileClusterInformer::FileClusterRanges_t &ranges)
{
    STARTING_VCN_INPUT_BUFFER s;
	const int OUTSIZE = 1024;
    char br[OUTSIZE];
    bool bexit = false;
    DWORD bytesreturned = 0;
    bool brval = true;

    s.StartingVcn.QuadPart = 0;
    do
    {
        BOOL b = DeviceIoControl(m_Fd.m_Handle, FSCTL_GET_RETRIEVAL_POINTERS, &s,
            sizeof s, br, sizeof br,
            &bytesreturned, NULL);
        bool bfill = false;

        if (b)
        {
            bfill = true;
            bexit = true;
        }
        else
        {
            DWORD err = GetLastError();
            if (ERROR_MORE_DATA == err)
            {
                bfill = true;
            }
            else
            {
                std::stringstream msg;
                msg << "FSCTL_GET_VOLUME_BITMAP failed with " <<  err;
                m_ErrorMessage = msg.str();
                bexit = true;
                brval = false;
            }
        }

        if (bfill)
        {
			RETRIEVAL_POINTERS_BUFFER *pr = (RETRIEVAL_POINTERS_BUFFER *)br;
            RecordRanges(s, *pr, ranges);
			s.StartingVcn.QuadPart = pr->Extents[pr->ExtentCount-1].NextVcn.QuadPart;
        }

    } while (!bexit);

    return brval;
}


void NtfsFileClusterInquirer::RecordRanges(STARTING_VCN_INPUT_BUFFER &s, RETRIEVAL_POINTERS_BUFFER &r, FileClusterInformer::FileClusterRanges_t &ranges)
{
    ranges.m_FeatureSupportState = E_FEATURESUPPORTED;

    SV_ULONGLONG start, count;
	for (DWORD i = 0; i < r.ExtentCount; i++)
	{
        start = r.Extents[i].Lcn.QuadPart;
        count = ((i>0) ? (r.Extents[i].NextVcn.QuadPart-r.Extents[i-1].NextVcn.QuadPart) : (r.Extents[i].NextVcn.QuadPart-r.StartingVcn.QuadPart));
        ranges.m_ClusterRanges.push_back(FileSystem::ClusterRange_t(start, count));
	}
}


std::string NtfsFileClusterInquirer::GetErrorMessage(void)
{
    return m_ErrorMessage;
}
