#ifndef STANDARDTYPECONVERTOR__H_ 
#define STANDARDTYPECONVERTOR__H_ 

#include "baseconvertor.h"

class StandardTypeConvertor : public BaseConvertor
{

public:
    StandardTypeConvertor(void);
    virtual ~StandardTypeConvertor(void);

    SV_UINT convertUINT( SV_UINT value ) ;
    SV_ULONGLONG convertULONGLONG( SV_ULONGLONG value ) ;
    SV_ULONG convertULONG( SV_ULONG value ) ;
    SV_USHORT convertUSHORT( SV_USHORT value ) ;

    SV_LONGLONG convertLONGLONG( SV_LONGLONG value ) ;
};

#endif /* STANDARDTYPECONVERTOR__H_ */
