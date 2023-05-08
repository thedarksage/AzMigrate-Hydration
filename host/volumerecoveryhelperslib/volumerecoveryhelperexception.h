#pragma once

#ifndef VOLUMERECOVERYHELPEREXCEPTION_H
#define VOLUMERECOVERYHELPEREXCEPTION_H

#include "InmXmlParser.h"

class VolumeRecoveryHelperException :
    public std::exception
    {
    private:
        FunctionInfo m_functInfo ;
        std::string m_strMessage ;
    public:
        VolumeRecoveryHelperException( const FunctionInfo& functInfo ) ;
        VolumeRecoveryHelperException( const std::string& exceptionMessage ) ;
        ~VolumeRecoveryHelperException(void) throw();

        const char* what() const throw();

    };

#endif // VOLUMERECOVERYHELPEREXCEPTION_H
