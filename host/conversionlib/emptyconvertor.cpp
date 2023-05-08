#include "emptyconvertor.h"

EmptyConvertor::EmptyConvertor(void)
{
}

EmptyConvertor::~EmptyConvertor(void)
{
}

SV_UINT EmptyConvertor::convertUINT( SV_UINT value ) 
{
    return value;
}

SV_ULONGLONG EmptyConvertor::convertULONGLONG( SV_ULONGLONG value ) 
{
    return value;
}

SV_ULONG EmptyConvertor::convertULONG( SV_ULONG value ) 
{
    return value;
}

SV_USHORT EmptyConvertor::convertUSHORT( SV_USHORT value ) 
{
    return value;
}

SV_LONGLONG EmptyConvertor::convertLONGLONG( SV_LONGLONG value )
{
    return value;
}
