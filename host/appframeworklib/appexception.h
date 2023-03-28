#ifndef APP_EXCEPTION_H
#define APP_EXCEPTION_H
#include <string>
class AppException
{
    std::string m_str ;
public:
    AppException() ;
    AppException(const std::string& str): m_str(str){}
    std::string to_string()
    {
        return m_str ;
    }
    const char* what()
    {
        return m_str.c_str() ;
    }
} ;
class ParseException :public AppException
{
    std::string m_errorstring ;
    
public:
    std::string inputType;
	ParseException() ;
	//ParseException(const std::string&) ;
    ParseException(const std::string& str): AppException(str)
    {
    }

~ParseException() throw() {}
};

class InvalidInput: public ParseException
{
    std::string m_inval;
public:    
    InvalidInput(const std::string& str)
    :ParseException( str )
    {
    }
   std::string getInvalidInput()
   {
         return what();
   }
	~InvalidInput() throw(){} 
};
class UnknownKeyException : public AppException
{
     std::string m_errorstring ;

    public:
        std::string inputType;
	    UnknownKeyException() ;
	    //ParseException(const std::string&) ;
        UnknownKeyException(const std::string& str): AppException(str)
        {
        }
    ~UnknownKeyException() throw() {}
};

class FileNotFoundException : public AppException
{
	std::string m_fileName ;
public:
    FileNotFoundException(const std::string& str): AppException(str)
    {
    }
	~FileNotFoundException() throw() {}
} ;

class SettingsNotFound : public AppException
{
     std::string m_errorstring ;
        
    public:
        std::string inputType;
	    SettingsNotFound() ;
	    //ParseException(const std::string&) ;
        SettingsNotFound(const std::string& str): AppException(str)
        {
        }
    ~SettingsNotFound() throw() {}
};
#endif