#include "conversionexception.h"

NoConversionRequired::NoConversionRequired( const char* errString ) 
{
    m_errString = errString ;
}

const char *NoConversionRequired::what( ) const throw( )
{
    return m_errString.c_str() ;
}

