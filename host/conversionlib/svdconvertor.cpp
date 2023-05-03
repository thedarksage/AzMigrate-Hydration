#include <stdio.h>
#include "svdconvertor.h"
#include "svdparse.h"


SVDConvertor::SVDConvertor(void)
{
    m_convertor = NULL ;
}

SVDConvertor::SVDConvertor( BaseConvertor* convertor ) 
{
    m_convertor = convertor ;
}

SVDConvertor::~SVDConvertor(void)
{
    if( m_convertor != NULL ) 
        delete m_convertor ;
    m_convertor = NULL ;
}


void SVDConvertor::convertPrefix( SVD_PREFIX& prefix)  
{
    prefix.tag = m_convertor->convertUINT( prefix.tag ) ;
    prefix.count = m_convertor->convertUINT( prefix.count ) ;
    prefix.Flags = m_convertor->convertUINT( prefix.Flags ) ;
    return ;
}

void SVDConvertor::convertHeader( SVD_HEADER1& header ) 
{
    convertGUID( header.DestHost ) ;
    convertGUID( header.DestVolume ) ;
    convertGUID( header.DestVolumeGroup ) ;
    convertGUID( header.OriginHost ) ;
    convertGUID( header.OriginVolume ) ;
    convertGUID( header.OriginVolumeGroup ) ;
    convertGUID( header.SVId ) ;
    return ;
}

void SVDConvertor::convertHeaderV2( SVD_HEADER_V2& header ) 
{
    convertGUID( header.DestHost ) ;
    convertGUID( header.DestVolume ) ;
    convertGUID( header.DestVolumeGroup ) ;
    convertGUID( header.OriginHost ) ;
    convertGUID( header.OriginVolume ) ;
    convertGUID( header.OriginVolumeGroup ) ;
    convertGUID( header.SVId ) ;

    convertVersionInfo( header.Version ) ;
    return ;
}

void SVDConvertor::convertTimeStamp( SVD_TIME_STAMP& ts ) 
{
    convertTimeStampHeader( ts.Header ) ;
    ts.TimeInHundNanoSecondsFromJan1601 = m_convertor->convertULONGLONG( ts.TimeInHundNanoSecondsFromJan1601 ) ;
    ts.ulSequenceNumber = m_convertor->convertUINT( ts.ulSequenceNumber ) ;
    return ;
}

void SVDConvertor::convertTimeStampV2( SVD_TIME_STAMP_V2& ts ) 
{
    convertTimeStampHeader( ts.Header ) ;
    ts.SequenceNumber = m_convertor->convertULONGLONG( ts.SequenceNumber ) ;
    ts.TimeInHundNanoSecondsFromJan1601 = m_convertor->convertULONGLONG( ts.TimeInHundNanoSecondsFromJan1601 ) ;
    return ;
}

void SVDConvertor::convertDBHeader( SVD_DIRTY_BLOCK& drtdHeader ) 
{
    drtdHeader.Length = m_convertor->convertLONGLONG( drtdHeader.Length ) ;
    drtdHeader.ByteOffset = m_convertor->convertLONGLONG( drtdHeader.ByteOffset ) ;
    return ;
}

void SVDConvertor::convertDBHeaderV2( SVD_DIRTY_BLOCK_V2& drtdHeader ) 
{
    drtdHeader.Length = m_convertor->convertUINT( drtdHeader.Length ) ;
    drtdHeader.uiSequenceNumberDelta = m_convertor->convertUINT( drtdHeader.uiSequenceNumberDelta ) ;
    drtdHeader.uiTimeDelta = m_convertor->convertUINT( drtdHeader.uiTimeDelta ) ;
    drtdHeader.ByteOffset = m_convertor->convertULONGLONG( drtdHeader.ByteOffset ) ;

    return ;
}


void SVDConvertor::convertSVDDataInfo( SVD_DATA_INFO& dataInfo ) 
{
     dataInfo.drtdStartOffset = m_convertor->convertLONGLONG( dataInfo.drtdStartOffset ) ;   
     dataInfo.drtdLength.QuadPart = m_convertor->convertULONGLONG( dataInfo.drtdLength.QuadPart ) ;   
     dataInfo.diffEndOffset = m_convertor->convertLONGLONG( dataInfo.diffEndOffset ) ;   

     return ;
}

void SVDConvertor::convertVersionInfo( SVD_VERSION_INFO& versionInfo ) 
{
    versionInfo.Major = m_convertor->convertUINT( versionInfo.Major ) ;
    versionInfo.Minor = m_convertor->convertUINT( versionInfo.Minor ) ;
    return ;
}

void SVDConvertor::convertTimeStampHeader( SVD_TIME_STAMP_HEADER& tsHeader ) 
{
    tsHeader.usStreamRecType = m_convertor->convertUSHORT( tsHeader.usStreamRecType ) ;
    return ;
}

void SVDConvertor::convertOthrInfo( SVD_OTHR_INFO & info )
{
	info.type = m_convertor->convertUINT( info.type );
	return;
}

void SVDConvertor::convertFostInfo( SVD_FOST_INFO & info )
{
	info.startoffset = m_convertor ->convertLONGLONG(info.startoffset);
	return ;
}


void SVDConvertor::convertCdpDrtd( SVD_CDPDRTD & drtd )
{
	drtd.voloffset = m_convertor -> convertLONGLONG( drtd.voloffset );
	drtd.length = m_convertor -> convertUINT( drtd.length );
	drtd.fileid = m_convertor -> convertUINT( drtd.fileid );
	drtd.uiTimeDelta = m_convertor -> convertUINT( drtd.uiTimeDelta );
	drtd.uiSequenceNumberDelta = m_convertor -> convertUINT( drtd.uiSequenceNumberDelta );
	return ;
}


void SVDConvertor::convertLONGLONG( SV_LONGLONG & value )
{
	value = m_convertor ->convertLONGLONG(value);
}
