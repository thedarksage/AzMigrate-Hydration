#include <stdio.h>
#include "svdconvertor.h"
#include "svdparse.h"
void SVDConvertor::convertGUID( SV_GUID& guid ) 
{
    guid.Data1 = m_convertor->convertULONG( guid.Data1 ) ;
    guid.Data2 = m_convertor->convertUSHORT( guid.Data2 ) ;
    guid.Data3 = m_convertor->convertUSHORT( guid.Data3 ) ;
    return ;
}
