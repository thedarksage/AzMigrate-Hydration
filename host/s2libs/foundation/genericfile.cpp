/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : genericfile.cpp
 *
 * Description: 
 */
#ifdef SV_WINDOWS
#include <windows.h>
#endif

#include <string>

#include "entity.h"

#include "portablehelpers.h"
#include "genericfile.h"


/*
 * FUNCTION NAME : GenericFile
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
GenericFile::GenericFile():m_sName("")
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : GenericFile
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
GenericFile::GenericFile(const GenericFile& genericfile):m_sName("")
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SetName(genericfile.GetName());
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
 * FUNCTION NAME : GenericFile
 *
 * DESCRIPTION : constructor
 *
 * INPUT PARAMETERS : file name as a pointer to char
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
GenericFile::GenericFile(const char* szName):m_sName(szName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SetName(szName);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}


/*
 * FUNCTION NAME : GenericFile
 *
 * DESCRIPTION : Constructor
 *
 * INPUT PARAMETERS : file name as a std::string
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
GenericFile::GenericFile(const std::string& sName):m_sName("")
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SetName(sName);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : ~GenericFile
 *
 * DESCRIPTION : Destructor
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
GenericFile::~GenericFile()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	Close();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : GetName
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : Returns the file name as a std::string
 *
 */
const std::string& GenericFile::GetName() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_sName;

}

/*
 * FUNCTION NAME : SetName
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : file name as a string
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void GenericFile::SetName(const std::string& sName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_sName = sName;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
 * FUNCTION NAME : SetName
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : File name as a pointer to char
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void GenericFile::SetName(const char* szName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_sName = szName;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

