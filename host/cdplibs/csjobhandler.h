//---------------------------------------------------------------
//  <copyright file="csjobhandler.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Abstract class to represent handler for CS jobs.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------

#ifndef CSJOBHANDLER_H
#define CSJOBHANDLER_H

#include "configurator2.h"


/// <summary>
/// Abstract class to represent handler for CS jobs.
/// </summary>
class CsJobHandler
{
public:
    /// <summary>
    /// Initializes a new instance of the <see cref="CsJobHandler"/> class.
    /// </summary>
    /// <param name="configurator">The configurator instance.</param>
    CsJobHandler(Configurator * configurator) :
        m_Configurator(configurator)
    {

    }

    /// <summary>
    /// Destructor for the <see cref="CsJobHandler"/> class.
    /// </summary>
    ~CsJobHandler()
    {

    }

    /// <summary>
    /// Pure virtual routine for processing a job to be implemented in the
    /// derived class.
    /// </summary>
    virtual void ProcessCsJob(CsJob & job) = 0;

protected:
    Configurator* m_Configurator;

private:

};

#endif
