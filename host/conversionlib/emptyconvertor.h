#ifndef EMPTYCONVERTOR__H_
#define EMPTYCONVERTOR__H_

#include "baseconvertor.h"

class EmptyConvertor :   public BaseConvertor
{
public:
    SV_UINT convertUINT( SV_UINT value ); 
    SV_ULONGLONG convertULONGLONG( SV_ULONGLONG value );
    SV_ULONG convertULONG( SV_ULONG value );
    SV_USHORT convertUSHORT( SV_USHORT value );
    SV_LONGLONG convertLONGLONG( SV_LONGLONG value );

    EmptyConvertor(void);
    virtual ~EmptyConvertor(void);
};

#endif /* EMPTYCONVERTOR__H_ */
