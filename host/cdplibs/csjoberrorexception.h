//---------------------------------------------------------------
//  <copyright file="csjoberrorexception.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Exception class to represent all the errors originated from the MasterTarget.
//  This exception is currently being used only for exceptions in job processing.
//  </summary>
//
//  History:     06-June-2017   Jaysha   Created
//----------------------------------------------------------------

#ifndef CSJOBERROREXCEPTION_H
#define CSJOBERROREXCEPTION_H

#include <exception>
#include <string>
#include <sstream>
#include <map>
#include "initialsettings.h"

/// <summary>
/// Exception class to represent all the errors originated from the MasterTarget.
/// </summary>
class CsJobErrorException : public std::exception
{
public:

    /// <summary>
    /// Initializes a new instance of the <see cref="CsJobErrorException"/> class.
    /// </summary>
    /// <param name="errorCode">The error code to be associated with the exception.</param>
    /// <param name="message">The error message associated with the exception.</param>
    /// <param name="possibleCauses">The possible causes for the error.</param>
    /// <param name="recommendedActions">Recommended action for the error.</param>
    /// <param name="messageParams">Parameters required to construct the error message.
    /// </param>
    CsJobErrorException(
        const std::string & errorCode,
        const std::string & message,
        const std::string & possibleCauses,
        const std::string & recommendedActions,
        const std::map<std::string, std::string> & messageParams)
        : m_ErrorCode(errorCode),
        m_Message(message),
        m_PossibleCauses(possibleCauses),
        m_RecommendedActions(recommendedActions),
        m_Params(messageParams)
    {

    }


    /// <summary>
    /// Gets the description of the exception.
    /// </summary>
    const char* what() const throw()
    {
        std::stringstream displayString;
        displayString << "Error code: " << m_ErrorCode << std::endl;
        displayString << "Message: " << m_Message << std::endl;
        displayString << "Possible causes: " << m_PossibleCauses << std::endl;
        displayString << "Recommended Actions: " << m_RecommendedActions << std::endl;
        displayString << "Message Params: " << std::endl;
        for (std::map<std::string, std::string>::const_iterator it = m_Params.begin();
            it != m_Params.end(); ++it)
        {
            displayString << it->first << ":" << it->second << std::endl;
        }

        return displayString.str().c_str();
    }

    /// <summary>
    /// Helper routine to map exception to corresponding <see cref="CsJobError"> class.
    /// </summary>
    void ToCsJobError(CsJobError & error)
    {
        error.ErrorCode = m_ErrorCode;
        error.Message = m_Message;
        error.PossibleCauses = m_PossibleCauses;
        error.RecommendedAction = m_RecommendedActions;
        error.MessageParams = m_Params;
    }

protected:

private:

    std::string m_ErrorCode;
    std::string m_Message;
    std::string m_PossibleCauses;
    std::string m_RecommendedActions;
    std::map<std::string, std::string> m_Params;
};

#endif