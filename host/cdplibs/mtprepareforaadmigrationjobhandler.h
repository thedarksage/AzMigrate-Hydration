//---------------------------------------------------------------
//  <copyright file="mtprepareforaadmigrationjobhandler.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Define class to handle MTPrepareForAadMigrationJob job.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------
#ifndef MTPrepareForAadMigrationJobHandler_H
#define MTPrepareForAadMigrationJobHandler_H

#include "configurator2.h"
#include "csjobhandler.h"

///  <summary>
///  Define class to handle MTPrepareForAadMigrationJob job.
///  </summary>
class MTPrepareForAadMigrationJobHandler : CsJobHandler
{
public:
    ///  <summary>
    ///  Constructor for <see cref="MTPrepareForAadMigrationJobHandler"> class.
    ///  </summary>
    MTPrepareForAadMigrationJobHandler(Configurator * configurator) :
        CsJobHandler(configurator)
    {

    }

    ///  <summary>
    ///  Destructor for <see cref="MTPrepareForAadMigrationJobHandler"> class.
    ///  </summary>
    ~MTPrepareForAadMigrationJobHandler()
    {

    }

    ///  <summary>
    ///  Job specific handler routine.
    ///  </summary>
    virtual void ProcessCsJob(CsJob & job);

protected:


private:
    MTPrepareForAadMigrationInput DeSerializeInput(const std::string & serializedInput);
    std::string DecodeInput(const std::string & inputType, const std::string & base64EncodedInput);
    void CopyContent(const std::string contentDetail, std::string & content, const std::string & location);
    void VerifyContent(const std::string contentDetail, const std::string & content, const std::string & location);
    void ImportHive(const std::string & fileLocation, const std::string & registryPath);
    void ImportCert(const std::string & fileLocation, const std::string & passphrase);
    void SetPrivilege(LPCSTR privilege, bool set);
};

#endif
