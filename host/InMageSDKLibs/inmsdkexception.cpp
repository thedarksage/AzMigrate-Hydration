#include "inmsdkexception.h"


APIException::APIException(const std::string& str)
{
	m_str = str ;
}
const char* APIException::what()
{
    return m_str.c_str() ;
}


AttributeNotFoundException :: AttributeNotFoundException(const std::string& str) 
: APIException(str)
{

}

GroupNotFoundException ::GroupNotFoundException (const std::string& str)
:APIException(str)
{
}
EmptyGroupFoundException ::EmptyGroupFoundException(const std::string& str)
:APIException(str)
{
}
PairNotFoundException::PairNotFoundException(const std::string& str)
:APIException(str)
{
}

SchemaUpdateFailedException::SchemaUpdateFailedException(const std::string& str)
: APIException(str) 
{
}

SchemaReadFailedException::SchemaReadFailedException(const std::string& str)
: APIException(str)
{

}

InvalidArgumentException::InvalidArgumentException(const std::string& str)
: APIException(str)
{

}

ParameterGroupNotFoundException::ParameterGroupNotFoundException(const std::string& str)
: APIException(str)
{

}

ParameterNotFoundException::ParameterNotFoundException(const std::string& str)
: APIException(str)
{

}

InavlidParameterException::InavlidParameterException(const std::string& str)
: APIException(str)
{

}

AttributeAlreadySetException::AttributeAlreadySetException(const std::string& str)
: APIException(str)
{

}

PolicyNotFoundException::PolicyNotFoundException(const std::string& str)
: APIException(str)
{

}

ParameterNotMatchedException::ParameterNotMatchedException(const std::string& str)
: APIException(str)
{

}