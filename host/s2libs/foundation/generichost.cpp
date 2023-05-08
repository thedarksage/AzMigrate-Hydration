/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : generichost.cpp
 *
 * Description: Currently this class doesnt implement anything.
 */


#include <string>
#include <ostream>
#include <istream>

#include "entity.h"

#include "operatingsystem.h"
#include "generichost.h"
#include "host.h"

/*
 * FUNCTION NAME : GenericHost
 *
 * DESCRIPTION : default constructor
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
GenericHost::GenericHost()
{

}

/*
 * FUNCTION NAME : GenericHost
 *
 * DESCRIPTION : copy constructor
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
GenericHost::GenericHost(const GenericHost &right)
{

}

/*
 * FUNCTION NAME : ~GenericHost
 *
 * DESCRIPTION : destructor
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
GenericHost::~GenericHost()
{

}

/*
 * FUNCTION NAME : operator=
 *
 * DESCRIPTION : 
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
GenericHost & GenericHost::operator=(const GenericHost &right)
{
    return *this;
}


/*
 * FUNCTION NAME : operator==
 *
 * DESCRIPTION : 
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
int GenericHost::operator==(const GenericHost &right) const
{
    return 0;
}

/*
 * FUNCTION NAME : operator!=
 *
 * DESCRIPTION : 
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
int GenericHost::operator!=(const GenericHost &right) const
{
    return 0;
}


/*
 * FUNCTION NAME : operator<
 *
 * DESCRIPTION : 
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
int GenericHost::operator<(const GenericHost &right) const
{
    return 0;
}

/*
 * FUNCTION NAME : operator>
 *
 * DESCRIPTION : 
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
int GenericHost::operator>(const GenericHost &right) const
{
    return 0;
}

/*
 * FUNCTION NAME : operator<=
 *
 * DESCRIPTION : 
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
int GenericHost::operator<=(const GenericHost &right) const
{
    return 0;
}

/*
 * FUNCTION NAME : operator>=
 *
 * DESCRIPTION : 
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
int GenericHost::operator>=(const GenericHost &right) const
{
    return 0;
}


/*
 * FUNCTION NAME : operator->
 *
 * DESCRIPTION : 
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
void GenericHost::operator->() const
{
}


/*
 * FUNCTION NAME : operator*
 *
 * DESCRIPTION : 
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
void GenericHost::operator*() const
{
}


/*
 * FUNCTION NAME : operator<<
 *
 * DESCRIPTION : 
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
std::ostream & operator<<(std::ostream &stream,GenericHost &right)
{
    return stream;
}

/*
 * FUNCTION NAME : operator>>
 *
 * DESCRIPTION : 
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
std::istream & operator>>(std::istream &stream,GenericHost &object)
{
    return stream;
}

/*
 * FUNCTION NAME : GetOperatingSystem
 *
 * DESCRIPTION : 
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
OperatingSystem* GenericHost::GetOperatingSystem ()
{
    return m_pOperatingSystem;
}

