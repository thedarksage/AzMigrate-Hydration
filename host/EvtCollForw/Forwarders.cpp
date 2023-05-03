#include "localconfigurator.h"
#include "Forwarders.h"

extern boost::shared_ptr<LocalConfigurator> lc;

namespace EvtCollForw
{
#ifdef SV_WINDOWS
    MarsComForwarder::MarsComForwarder(unsigned int maxNumOfFiles)
        : m_maxNumOfFiles(maxNumOfFiles)
    {
        //BOOST_ASSERT(volSettings.isAzureDataPlane());
        BOOST_ASSERT(lc);
        std::string emptyStr;

        m_azureagent_ptr = boost::make_shared<AzureAgentInterface::AzureAgent>(emptyStr, // vmName
            emptyStr, // volSettings.GetEndpointHostId(),
            emptyStr, // volSettings.GetEndpointDeviceName(),
            0, // deviceSize
            0, // blockSize
            emptyStr, // resyncJobId
            (AzureAgentInterface::AzureAgentImplTypes)lc->getAzureImplType(), // configurator.getVxAgent().getAzureImplType(),
            // TODO-SanKumar-1702: Although retry params are passed, UploadEvents call doesn't do
            // a retry but it's done at EvtCollForw. So, should we explicitly pass 0?
            lc->getMaxAzureAttempts(), // configurator.getVxAgent().getMaxAzureAttempts(),
            lc->getAzureRetryDelayInSecs()// configurator.getVxAgent().getAzureRetryDelayInSecs()
            );
    }

    bool MarsComForwarder::PutNext(const std::vector<std::string>& filesToUpload, const std::vector<std::string>& newState)
    {
        std::vector<std::string>::const_iterator partStartItr = filesToUpload.cbegin(), partEndItr = filesToUpload.cbegin();
        unsigned int pendingFilesCnt = filesToUpload.size();        

        while (pendingFilesCnt > 0)
        {
            if (pendingFilesCnt > m_maxNumOfFiles)
            {
                partEndItr += m_maxNumOfFiles;
                pendingFilesCnt -= m_maxNumOfFiles;
            }
            else
            {
                partEndItr += pendingFilesCnt;
                pendingFilesCnt -= pendingFilesCnt;
            }

            // TODO-SanKumar-1702: Modify UploadEvents call below to take iterators instead of vector,
            // so we could avoid an unnecessary copy of part range before invoking the method. It's
            // counter intuitive to what we wanna achieve by giving a max num of files limit.
            std::vector<std::string> partFilesToUpload(partStartItr, partEndItr);
            bool hasCompletedAllFiles = filesToUpload.cend() == partEndItr;

            m_azureagent_ptr->UploadEvents(partFilesToUpload);
            Commit(partFilesToUpload, hasCompletedAllFiles);

            // Proceed beyond the completed section.
            partStartItr = partEndItr;
        }

        BOOST_ASSERT(pendingFilesCnt == 0);
        BOOST_ASSERT(partStartItr == partEndItr && partEndItr == filesToUpload.cend());

        return true;
    }

    void MarsComForwarder::Complete()
    {
        // NOP for the forwarder itself.

        Complete(std::vector<std::string>());
    }

    void MarsComForwarder::Complete(const std::vector<std::string>& newState)
    {
        Commit(newState, false);
    }

    EventHubForwarder::EventHubForwarder()
        :m_eventHubRestHelper(EvtCollForw_Env_V2ARcmPS)
    {}

    bool EventHubForwarder::PutNext(const std::vector<std::string>& filesToUpload, const std::vector<std::string>& newState)
    {
        // Todo:sadewang - Build on linux
        std::vector<std::string>::const_iterator partStartItr = filesToUpload.begin(), partEndItr = filesToUpload.end();
        std::vector<std::string> uploaded_files;
        while (partStartItr != partEndItr)
        {
            if (Upload_File(*partStartItr))
            {
                uploaded_files.push_back(*partStartItr);
            }
            else
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to upload file %s", *partStartItr);
                // Do not break here since failed upload of one file should not stop upload of other files
            }
            partStartItr++;
        }
        // FilePollCollector deletes file irrespective of the bool value, so second param is no-op
        Commit(uploaded_files, true);
        return true;
    }

    bool EventHubForwarder::Upload_File(const std::string& filepath)
    {
        try 
        {
            if (!m_eventHubRestHelper.UploadFile(filepath))
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to upload file %s\n", filepath.c_str());
                return false;
            }
            return true;
        }
        GENERIC_CATCH_LOG_ACTION("Failed to upload file", return false);
    }

    void EventHubForwarder::Complete()
    {
        // NOP for the forwarder itself.

        Complete(std::vector<std::string>());
    }

    void EventHubForwarder::Complete(const std::vector<std::string>& newState)
    {
        Commit(newState, false);
    }
#endif // SV_WINDOWS
}