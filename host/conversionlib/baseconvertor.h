#ifndef BASECONVERTOR__H_
#define BASECONVERTOR__H_

#include "svtypes.h"
#include <boost/shared_ptr.hpp>

class BaseConvertor
{
public:
    virtual SV_UINT convertUINT( SV_UINT value ) = 0 ;
    virtual SV_ULONGLONG convertULONGLONG( SV_ULONGLONG value ) = 0 ;
    virtual SV_ULONG convertULONG( SV_ULONG value ) = 0 ;
    virtual SV_USHORT convertUSHORT( SV_USHORT value ) = 0 ;
    virtual SV_LONGLONG convertLONGLONG( SV_LONGLONG value )  = 0 ;
} ;

typedef boost::shared_ptr<BaseConvertor> BaseConvertorPtr ;

#endif /* BASECONVERTOR__H_ */

