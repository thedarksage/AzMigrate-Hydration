#include "standardtypeconvertor.h"

StandardTypeConvertor::StandardTypeConvertor(void)
{
}

StandardTypeConvertor::~StandardTypeConvertor(void)
{
}

SV_UINT StandardTypeConvertor::convertUINT( SV_UINT value ) 
{
    return ( ( value >> 24 ) | 
                    ( ( value << 8) & 0x00FF0000 ) | 
                    ( ( value >> 8 ) & 0x0000FF00 ) | 
                    ( value<<24 ) ) ;
}

SV_ULONGLONG StandardTypeConvertor::convertULONGLONG( SV_ULONGLONG value ) 
{
    return ( ( value >> 56) | 
                   ( ( value << 40 ) & 0x00FF000000000000ULL ) | 
                   ( ( value << 24 ) & 0x0000FF0000000000ULL )	| 
                   ( ( value << 8 )  & 0x000000FF00000000ULL ) | 
                   ( ( value >> 8)  & 0x00000000FF000000 ) | 
                   ( ( value >> 24 ) & 0x0000000000FF0000 ) | 
                   ( ( value>>40 ) & 0x000000000000FF00 ) | 
                   ( value << 56 ) ) ;
}

SV_LONGLONG StandardTypeConvertor::convertLONGLONG( SV_LONGLONG value )
{
    return convertULONGLONG( value ) ;
}

SV_USHORT StandardTypeConvertor::convertUSHORT( SV_USHORT value ) 
{
    return ( ( ( ( value ) >> 8) & 0xFF ) | 
                     ( ( ( value ) & 0xFF) << 8 ) ) ;
}


SV_ULONG StandardTypeConvertor::convertULONG( SV_ULONG value ) 
{
#ifndef SV_WINDOWS
	//SV_ULONG can never be 8 byte length in Windows
    if (8 == sizeof (SV_ULONG))
    {
        return ( ( value >> 56) | 
                       ( ( value << 40 ) & 0x00FF000000000000ULL ) | 
                       ( ( value << 24 ) & 0x0000FF0000000000ULL )	| 
                       ( ( value << 8 )  & 0x000000FF00000000ULL ) | 
                       ( ( value >> 8)  & 0x00000000FF000000 ) | 
                       ( ( value >> 24 ) & 0x0000000000FF0000 ) | 
                       ( ( value>>40 ) & 0x000000000000FF00 ) | 
                       ( value << 56 ) ) ;
    }
	else if (4 == sizeof (SV_ULONG))
#endif
    {
        return ( ( value >> 24 ) | 
                    ( ( value << 8) & 0x00FF0000 ) | 
                    ( ( value >> 8 ) & 0x0000FF00 ) | 
                    ( value<<24 ) ) ;
    } 
}
