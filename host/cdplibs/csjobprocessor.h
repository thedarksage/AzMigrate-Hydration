//---------------------------------------------------------------
//  <copyright file="csjobprocessor.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines class for interacting with CS and process all available jobs.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------


#ifndef CSJOBPROCESSOR_H
#define CSJOBPROCESSOR_H

#include "configurator2.h"

/// <summary>
/// Defines class for interacting with CS and process all available jobs.
/// </summary>
class CsJobProcessor
{
public:

    /// <summary>
    /// Constructor for <c>CsJobProcessor</c> class.
    /// </summary>
    CsJobProcessor();

    /// <summary>
    /// Desstructor for <c>CsJobProcessor</c> class.
    /// </summary>
    ~CsJobProcessor();

    /// <summary>
    /// Fetches jobs from CS and processes them until there are none.
    /// </summary>
    /// <param name="waitTimeInSecs">The time delay in seconds between sucessive polling to CS.</param>
    bool run(unsigned int waitTimeInSecs);

protected:


private:

    void GetConfigurator();
    void ProcessCsJobs();

    bool IsJobReadyForProcesing(CsJob job);
    void ProcessCsJob(CsJob job);
    void ProcessMTPrepareForAadMigrationJob(CsJob job);

    std::string ToString(CsJob job);

    Configurator* m_Configurator;
};

#endif /* REGISTERHOSTTHREAD_H */
