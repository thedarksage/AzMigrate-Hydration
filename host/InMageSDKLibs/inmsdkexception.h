#ifndef	INMSDK_EXCEPTION
#define INMSDK_EXCEPTION

#include <string>

class APIException : public std::exception
{
	std::string m_str ;
public:
	APIException(const std::string&) ;
	const char* what() ;
	virtual ~APIException() throw(){ } 
} ;
class InvalidArgumentException :  public APIException
{
public:
    InvalidArgumentException(const std::string&) ;
	~InvalidArgumentException()  throw() {}
};
class AttributeNotFoundException : public APIException
{
public:
	AttributeNotFoundException(const std::string&) ;
	~AttributeNotFoundException()  throw() {}
} ;

class GroupNotFoundException : public APIException
{
public:
	GroupNotFoundException(const std::string&) ;
	~GroupNotFoundException()  throw() {}
} ;
class EmptyGroupFoundException : public APIException
{
public:
	EmptyGroupFoundException(const std::string&);
	~EmptyGroupFoundException() throw() {}
};
class PairNotFoundException : public APIException
{
public:
	PairNotFoundException(const std::string&) ;
	~PairNotFoundException()  throw() {}
} ;

class SchemaUpdateFailedException : public APIException
{
public:
	SchemaUpdateFailedException(const std::string&) ;
    ~SchemaUpdateFailedException()throw(){}
} ;

class SchemaReadFailedException : public APIException
{
public:
	SchemaReadFailedException(const std::string&) ;
	~SchemaReadFailedException() throw() {}
} ;

class ParameterGroupNotFoundException : public APIException
{
public:
	ParameterGroupNotFoundException(const std::string&);
	~ParameterGroupNotFoundException() throw() {}
};

class ParameterNotFoundException : public APIException
{
public:
	ParameterNotFoundException(const std::string&);
	~ParameterNotFoundException() throw() {}
};

class InavlidParameterException : public APIException
{
public:
	InavlidParameterException(const std::string&);
	~InavlidParameterException() throw() {}
};

class AttributeAlreadySetException : public APIException
{
public:
	AttributeAlreadySetException(const std::string&);
	~AttributeAlreadySetException() throw() {}
};

class PolicyNotFoundException : public APIException
{
public:
	PolicyNotFoundException(const std::string&);
	~PolicyNotFoundException() throw() {}
};

class ParameterNotMatchedException : public APIException
{
public:
	ParameterNotMatchedException(const std::string&);
	~ParameterNotMatchedException() throw() {}
};

#endif