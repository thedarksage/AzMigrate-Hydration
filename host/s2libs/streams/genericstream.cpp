/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : genericstream.cpp
 *
 * Description: 
 */
 
 #include <assert.h>

#include "entity.h"
#include "genericstream.h"
#include "error.h"
#include "ace/OS.h"


/*
 * FUNCTION NAME :  GenericStream   
 *
 * DESCRIPTION :    Default Constructor. Initializes the stream open status to false and sets state to clear
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
GenericStream::GenericStream(): m_bOpen(false)
{
    SetState(STRM_CLEAR);
}

/*
 * FUNCTION NAME :  GenericStream   
 *
 * DESCRIPTION :    Destructor.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
GenericStream::~GenericStream()
{

}

/*
 * FUNCTION NAME :  ModeOn   
 *
 * DESCRIPTION :    Checks if a stream mode is specified.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
bool GenericStream::ModeOn(STREAM_MODE chkMode, STREAM_MODE mode)
{
    return (chkMode == (chkMode & mode));
}

/*
 * FUNCTION NAME :  SetState   
 *
 * DESCRIPTION :    Sets the stream to a specified state. 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :          Refer STREAM_STATE enum.
 *
 * return value :
 *
 */
 
void GenericStream::SetState(STREAM_STATE state)
{
    m_State = state;
}

/*
 * FUNCTION NAME :  Clear   
 *
 * DESCRIPTION :    Sets the stream state to clear status.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
void GenericStream::Clear()
{
    SetState(STRM_CLEAR);
}

/*
 * FUNCTION NAME :  IsOpen   
 *
 * DESCRIPTION :    Inspector which returns if stream open for operations
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
const bool GenericStream::IsOpen() const
{
    return m_bOpen;
}

int GenericStream::Abort(const void* pData)
{
    assert("To be implemented in derived classes\n");
    return 0;
}

int GenericStream::Delete(const void* pData)
{
    assert("To be implemented in derived classes\n");
    return 0;
}

/*
 * FUNCTION NAME :  Good   
 *
 * DESCRIPTION :    checks if stream state is good.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns true if stream state is good else false
 *
 */
 
const bool GenericStream::Good() const
{
    bool bGood = false;
    switch(m_State)
    {
        case STRM_GOOD:
		case STRM_CLEAR:
        case STRM_MORE_DATA:
            bGood = true;
        break;
        
        case STRM_BAD:
        case STRM_FAIL:
			bGood = false;
        break;
    }
    return bGood;
}

/*
 * FUNCTION NAME :  Bad   
 *
 * DESCRIPTION :    checks if stream state is bad.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns true if stream state is bad else false
 *
 */
 
const bool GenericStream::Bad() const
{
    return (STRM_BAD == m_State);
}

/*
 * FUNCTION NAME :  Eof   
 *
 * DESCRIPTION :    checks if stream has encountered end of file during read/write operations
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns true if eof is encountered.
 *
 */
 
const bool GenericStream::Eof() const
{
    return (STRM_EOF == m_State);
}

/*
 * FUNCTION NAME :  Fail   
 *
 * DESCRIPTION :    Checks if stream is in failed state.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns true if stream is in a failed state.
 *
 */
 
const bool GenericStream::Fail() const
{
    return (STRM_FAIL == m_State);
}

/*
 * FUNCTION NAME :  MoreData   
 *
 * DESCRIPTION :    Checks if mode data is available in the stream for read / write operations
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns true if more data is available else false.
 *
 */
 
const bool GenericStream::MoreData() const
{
    return (STRM_MORE_DATA == m_State);
}

