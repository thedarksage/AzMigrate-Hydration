#include "convertorfactory.h"
#include "svtypes.h"
#include "standardtypeconvertor.h"
#include "emptyconvertor.h"
#include "svdconvertor.h"
#include "hashconvertor.h"
#include "conversionexception.h" 

//ConvertorFactory ConvertorFactory::factory ;

//ConvertorFactory::ConvertorFactory(void)
//{
//    SV_UINT tempValue = 1 ;
//    SV_UCHAR *intermittant = (SV_UCHAR*) &tempValue ;
//    if( *intermittant == 1 )
//         m_machineType = SV_LITTLE_ENDIAN ;
//    else
//        m_machineType = SV_BIG_ENDIAN ; 
//
//    m_bOldSVDFile = false ;
//}
//
//ConvertorFactory::~ConvertorFactory(void)
//{
//}

//ConvertorFactory& ConvertorFactory::getInstance()
//{
//    return factory ;
//}

void ConvertorFactory::getSVDConvertor( SV_UINT& dataFormatFlags, SVDConvertorPtr& svdConvertor )
{
    svdConvertor.reset( new SVDConvertor( getBaseConvertor( dataFormatFlags ))  ) ;
    return ;
}

void ConvertorFactory::getHashConvertor( SV_UINT& dataFormatFlags,HashConvertorPtr& hashConvertor ) 
{
  
    hashConvertor.reset( new HashConvertor( getBaseConvertor( dataFormatFlags ) ) ) ; 
    return ;
}


bool ConvertorFactory::isConversionReqd( SV_UINT& dataFormatFlags ) 
{
    bool bConversionReqd = true ;
    if( machineType() == SV_BIG_ENDIAN )
    {
         if( INMAGE_MAKEFOURCC( 'D','R','T','B' ) == dataFormatFlags )
         {
             bConversionReqd = false ;
         }
         else if( INMAGE_MAKEFOURCC( 'L', 'T', 'R', 'D' ) != dataFormatFlags ) 
         {
              throw NoConversionRequired("File Received from the agent with lesser compatibility Number."
                  "Agents with verions less than 5.5 on Little Endian machines can not work with Agents on Big Endian machine") ;
         }
    }
    else
    {
         if( INMAGE_MAKEFOURCC( 'D','R','T','L' ) == dataFormatFlags )
         {
              bConversionReqd = false ;
         }
         else if( INMAGE_MAKEFOURCC( 'B','T','R','D' ) != dataFormatFlags )
         {
              //this is old file, no conversion required and svd file should be read from starting again...
              throw NoConversionRequired("File Received from the agent with lesser compatibility Number.") ;
         }
    }

    return bConversionReqd ;
}

void ConvertorFactory::getBaseConvertor( SV_UINT& dataFormatFlags, BaseConvertorPtr& baseConvertor )
{
    baseConvertor.reset( getBaseConvertor( dataFormatFlags ) ) ;
    return ;
}

BaseConvertor * ConvertorFactory::getBaseConvertor( SV_UINT& dataFormatFlags )
{
    BaseConvertor *convertor ;
    try
    {
        if( isConversionReqd( dataFormatFlags ) )
        {
            convertor = new StandardTypeConvertor() ;
        }
        else
        {
            convertor = new EmptyConvertor() ;
        }
    }
     catch( NoConversionRequired& ) 
    {
        convertor = new EmptyConvertor() ;        
    }

   return convertor ;   
}

MACHINETYPE ConvertorFactory::machineType() 
{
    MACHINETYPE procType ;
    SV_UINT tempValue = 1 ;
    SV_UCHAR *intermittant = (SV_UCHAR*) &tempValue ;
    if( *intermittant == 1 )
         procType = SV_LITTLE_ENDIAN ;
    else
        procType = SV_BIG_ENDIAN ; 

    return procType ;
}

void ConvertorFactory::setDataFormatFlags( SV_UINT& dataFormatFlags )
{
    if( machineType() == SV_LITTLE_ENDIAN )
        dataFormatFlags = SVD_TAG_LEFORMAT ;
    else
        dataFormatFlags = SVD_TAG_BEFORMAT ;

    return ;
}

bool ConvertorFactory::isOldSVDFile( SV_UINT& dataFormatFlags )
{
    bool bOldFile = false ;
    try
    {
         isConversionReqd( dataFormatFlags ) ;       
    }
    catch ( NoConversionRequired& ) 
    {
         bOldFile = true ;
    }

    return bOldFile ;
}
