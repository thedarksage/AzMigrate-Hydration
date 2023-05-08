#ifndef SVDCONVERTOR__H_  
#define SVDCONVERTOR__H_  

#include "baseconvertor.h"
#include "svdparse.h"
#include <boost/shared_ptr.hpp>

class SVDConvertor
{
private:
    BaseConvertor* m_convertor ;
    SVDConvertor(void);

    void convertGUID( SV_GUID& guid ) ;
    void convertVersionInfo( SVD_VERSION_INFO& verInfo ) ;
    void convertTimeStampHeader( SVD_TIME_STAMP_HEADER& tsHeader ) ;

public:
    SVDConvertor( BaseConvertor* convertor ) ;
    ~SVDConvertor(void);

    void convertPrefix( SVD_PREFIX& prefix)  ;
    
    void convertHeader( SVD_HEADER1& header ) ;
    void convertHeaderV2( SVD_HEADER_V2& header ) ;

    void convertTimeStamp( SVD_TIME_STAMP& ts ) ;
    void convertTimeStampV2( SVD_TIME_STAMP_V2& ts ) ;

    void convertDBHeader( SVD_DIRTY_BLOCK& drtdHeader ) ;
    void convertDBHeaderV2( SVD_DIRTY_BLOCK_V2& drtHeader ) ;
    void convertSVDDataInfo( SVD_DATA_INFO& dataInfo ) ;
	void convertFostInfo( SVD_FOST_INFO & info ) ;
	void convertOthrInfo( SVD_OTHR_INFO & info ) ;
	void convertCdpDrtd( SVD_CDPDRTD & drtd ) ;
	void convertLONGLONG( SV_LONGLONG & value );
    BaseConvertor* getBaseConvertor() const { return m_convertor ; }    
};

typedef boost::shared_ptr<SVDConvertor>  SVDConvertorPtr ;

#endif /* SVDCONVERTOR__H_ */
