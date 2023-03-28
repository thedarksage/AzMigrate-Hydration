#include "volumerecoveryhelperexception.h"

VolumeRecoveryHelperException::VolumeRecoveryHelperException(const FunctionInfo& functInfo)
{
    this->m_functInfo = functInfo ;
    m_strMessage = functInfo.m_Message ;
}

VolumeRecoveryHelperException::VolumeRecoveryHelperException(const std::string& excpetionMessage ) 
{
   this->m_strMessage = excpetionMessage ;
}
VolumeRecoveryHelperException::~VolumeRecoveryHelperException(void) throw()
{

}

const char* VolumeRecoveryHelperException::what() const throw()
{
    std::stringstream exceptionMessage ; 
    exceptionMessage <<  "The API " << m_functInfo.m_RequestFunctionName << 
        "failed with Message: " << m_strMessage  ;

    return exceptionMessage.str().c_str() ;
}
