#ifndef CONVERSIONEXCEPTION__H_
#define CONVERSIONEXCEPTION__H_

#include <string>
#include <exception>

class NoConversionRequired: public std::exception
{
    std::string m_errString ;
  public:
    NoConversionRequired(const char* errString ) ;
   const char *what( ) const throw( );
    virtual ~NoConversionRequired() throw(){ } 
} ;

#endif /* CONVERSIONEXCEPTION__H_ */
