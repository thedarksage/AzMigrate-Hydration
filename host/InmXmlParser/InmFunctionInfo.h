#ifndef INM__FUNCTION__INFO__H_
#define INM__FUNCTION__INFO__H_

#include <string>
#include <map>
#include <boost/lexical_cast.hpp>

class compare_1 { // simple comparison function
public:
    bool operator()(const std::string& x, const std::string y) const
    { 
        int intx, inty ;
        //return ( (x.compare(y) < 0) ? true : false ) ;
        std::string::size_type xIndex = x.find_first_of("1234567890") ;
        std::string::size_type yIndex = y.find_first_of("1234567890") ;
        if( xIndex !=  std::string::npos && yIndex !=  std::string::npos )
        {
            std::string::size_type xIndex2 = std::string::npos, yIndex2  = std::string::npos ;
             bool bNumberInSquare = ( x.find( '[' ) != std::string::npos && 
                 y.find( '[' ) != std::string::npos && 
                 ( xIndex2 = x.find( "]", xIndex ) ) != std::string::npos && 
                 ( yIndex2 = y.find("]", yIndex ) ) != std::string::npos )  ;

            if( bNumberInSquare ||
                ( xIndex == 0 && yIndex == 0 ) )
            {
                try
                {
                    if( xIndex2 != std::string::npos &&  yIndex2 != std::string::npos )
                    {
                        xIndex = x.find( "[" ) + 1 ;
                        yIndex = y.find( "[" ) + 1 ;
                        intx = boost::lexical_cast<int>(x.substr(xIndex, xIndex2 - xIndex)) ;
                        inty = boost::lexical_cast<int>(y.substr(yIndex, yIndex2 - yIndex)) ;
                    }
                    else
                    {
                         intx = boost::lexical_cast<int>(x.substr(xIndex)) ;
                         inty = boost::lexical_cast<int>(y.substr(yIndex)) ;
                    }                
                    return (intx - inty) < 0 ? true : false ; 
                }
                catch(...)
                {
                }
            }
        }
        return x.compare(y) < 0 ? true : false ; 
    }
};

struct ValueType
{
    std::string type;
    std::string value;
    ValueType(const std::string &t, const std::string &v) :
              type(t), value(v)
    {
    }
	ValueType()
    {
    }
};

//map from name to value
typedef std::map<std::string,ValueType> Parameters_t;
typedef Parameters_t::iterator ParametersIter_t;
typedef Parameters_t::const_iterator ConstParametersIter_t;

struct ParameterGroup;
typedef std::map<std::string, ParameterGroup,compare_1> ParameterGroups_t;
//Key :Parametergroup ID
typedef ParameterGroups_t::const_iterator ConstParameterGroupsIter_t;
typedef ParameterGroups_t::iterator ParameterGroupsIter_t;



struct InmAuthentication
{
  std::string m_AccessKeyID;
  std::string m_AccessSignature;
  Parameters_t  m_AuthenticationParamsPair;
};

struct ParameterGroup
{
  Parameters_t m_Params;
  ParameterGroups_t m_ParamGroups;
};

struct Header
{
  InmAuthentication m_AuthenticationObj;
  ParameterGroup m_HeaderParmGrp;
};

//FunctionInfo will contain request and response(In and out param)
struct FunctionInfo
{
 //attributes for request
  std::string m_RequestFunctionName;
  //std::string m_ReqFunId;
  std::string m_ReqstInclude;
  
  //attributes for Response
  std::string m_FuntionId;
  std::string m_ReturnCode;
  std::string m_Message;
  
  //request and response pgs
  ParameterGroup m_RequestPgs;
  ParameterGroup m_ResponsePgs;
};

#endif /* INM__FUNCTION__INFO__H_*/
