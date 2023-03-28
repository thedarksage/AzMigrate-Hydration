//---------------------------------------------------------------
//  <copyright file="csjobprocessor.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines class for interacting with CS and process all available jobs.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------

#include "csjobprocessor.h"
#include "csjobhandler.h"
#include "mtprepareforaadmigrationjobhandler.h"
#include "csjoberror.h"
#include "cdputil.h"
#include "logger.h"
#include "portablehelpers.h"

/// <summary>
/// Constructor for <c>CsJobProcessor</c> class.
/// </summary>
CsJobProcessor::CsJobProcessor() :m_Configurator(0)
{
}

/// <summary>
/// Desstructor for <c>CsJobProcessor</c> class.
/// </summary>
CsJobProcessor::~CsJobProcessor()
{
}

/// <summary>
/// Fetches jobs from CS and processes them until there are none.
/// </summary>
/// <param name="waitTimeInSecs">The time delay in seconds between sucessive polling to CS.</param>
bool CsJobProcessor::run(unsigned int waitTimeInSecs)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    try
    {
        GetConfigurator();
        if (m_Configurator == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "%s hit internal error (unable to communicate with CS).\n");
            return false;
        }

        while (!CDPUtil::QuitRequested(waitTimeInSecs))
        {
            if (m_Configurator->getInitialSettings().AnyJobsAvailableForProcessing())
            {
                ProcessCsJobs();
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "No jobs to process.\n");
                break;
            }

        }
    }
    catch (std::exception & e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s hit an unhandled exception %s.\n", __FUNCTION__, e.what());
        return false;
    }

    if (CDPUtil::Quit())
    {
        DebugPrintf(
            SV_LOG_ERROR,
            "%s Recieved a shutdown request.\n",
            __FUNCTION__);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "CSJob processing completed.\n");

    return true;
}

void CsJobProcessor::GetConfigurator()
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    if (m_Configurator == NULL && !::GetConfigurator(&m_Configurator))
    {
        DebugPrintf(
            SV_LOG_ERROR,
            "@%s %d: Unable to obtain configurator ",
            __FUNCTION__,
            LINE_NO);
    }
}

void CsJobProcessor::ProcessCsJobs()
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

        std::list<CsJob> csJobs = m_Configurator->getJobsToProcess();
        if (!csJobs.empty())
        {
            std::list<CsJob>::iterator iter = csJobs.begin();
            for (; iter != csJobs.end(); iter++)
            {
                if (CDPUtil::Quit())
                {
                    DebugPrintf(
                        SV_LOG_ERROR,
                        "%s recieved a shutdown request.\n",
                        __FUNCTION__);
                    break;
                }

                CsJob job = *iter;
                ProcessCsJob(job);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "CS did not returns any jobs to process.\n");
        }
}

void CsJobProcessor::ProcessCsJob(CsJob job)
{
    try
    {
        std::string displayString = ToString(job);
        DebugPrintf(SV_LOG_DEBUG, "Processing Job:\n%s\n", displayString.c_str());

        if (IsJobReadyForProcesing(job))
        {
            if (strcmpi(job.JobType.c_str(), MTPrepareForAadMigrationJob.c_str()) == 0)
            {
                ProcessMTPrepareForAadMigrationJob(job);
            }
            else
            {
                DebugPrintf(
                    SV_LOG_ERROR,
                    "Recieved an unsupported job. Job details:\n%s\n",
                    displayString.c_str());
            }
        }
        else
        {
            DebugPrintf(
                SV_LOG_ERROR,
                "Recieved a job which is not ready for processing. Job details:\n%s\n",
                displayString.c_str());
        }
    }
    catch (std::exception & e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s hit an unhandled exception %s.\n", __FUNCTION__, e.what());
    }
}

bool CsJobProcessor::IsJobReadyForProcesing(CsJob job)
{
    return (strcmpi(job.JobStatus.c_str(), CsJobStatusPending.c_str()) == 0 ||
        strcmpi(job.JobStatus.c_str(), CsJobStatusInProgress.c_str()) == 0);
}

std::string CsJobProcessor::ToString(CsJob job)
{
    std::stringstream displayString;
    displayString << job << std::endl;
    return displayString.str();
}

void CsJobProcessor::ProcessMTPrepareForAadMigrationJob(CsJob job)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s %s line: %d.\n", __FILE__, __FUNCTION__, __LINE__);

    MTPrepareForAadMigrationJobHandler jobhandler(m_Configurator);
    jobhandler.ProcessCsJob(job);
}