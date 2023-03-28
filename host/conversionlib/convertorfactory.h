#ifndef CONVERTORFACTORY__H_
#define CONVERTORFACTORY__H_

#include "svtypes.h"
#include <boost/shared_ptr.hpp>

class SVDConvertor ;
typedef boost::shared_ptr<SVDConvertor> SVDConvertorPtr ;

class HashConvertor ;
typedef boost::shared_ptr<HashConvertor> HashConvertorPtr ;

class BaseConvertor ;
typedef boost::shared_ptr<BaseConvertor> BaseConvertorPtr ;

enum MACHINETYPE { SV_LITTLE_ENDIAN, SV_BIG_ENDIAN } ;

class ConvertorFactory
{
private: 

	static bool isConversionReqd( SV_UINT& dataFormatFlags ) ;
    static BaseConvertor* getBaseConvertor( SV_UINT& dataFormatFlags ) ;
    ConvertorFactory(void);
    ConvertorFactory( ConvertorFactory& ) ;
    ConvertorFactory& operator=( ConvertorFactory& ) ;

public:
   
    ~ConvertorFactory(void);

    static void getSVDConvertor( SV_UINT& dataFormatFlags, SVDConvertorPtr& svdConvertor ) ;
    static void getHashConvertor( SV_UINT& dataFormatFlags, HashConvertorPtr& hashConvertor ) ;
    static void getBaseConvertor( SV_UINT& dataFormatFlags, BaseConvertorPtr& baseConvertor ) ;

    static MACHINETYPE machineType() ;
    static void setDataFormatFlags( SV_UINT& dataFormatFlags ) ;
    
    static bool isOldSVDFile( SV_UINT& dataFormatFlags ) ;

};

#endif /* CONVERTORFACTORY__H_ */
