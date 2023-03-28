#include "hashconvertor.h"
#include "hashcomparedata.h"

HashConvertor::HashConvertor(void)
{
    m_convertor = NULL ;
}

HashConvertor::~HashConvertor(void)
{
    if( m_convertor != NULL ) 
        delete m_convertor ;
    m_convertor = NULL ;
}

HashConvertor::HashConvertor( BaseConvertor* convertor )
{
    m_convertor = convertor ;
}
void HashConvertor::convertNode( HashCompareNode& node ) 
{
    node.m_Offset = m_convertor->convertLONGLONG( node.m_Offset ) ;
    node.m_Length = m_convertor->convertUINT( node.m_Length ) ;
    return ;
}

void HashConvertor::convertInfo( HashCompareInfo& info ) 
{
    info.m_Algo = m_convertor->convertUINT( info.m_Algo ) ;
    info.m_ChunkSize = m_convertor->convertUINT( info.m_ChunkSize ) ;
    info.m_CompareSize = m_convertor->convertUINT( info.m_CompareSize ) ;
    return ;
}
