/*
ACE 
http://ace.sourcearchive.com/documentation/5.7.7/ACE_8cpp-source.html

Copyright (c) 1995-2003 Douglas C. Schmidt and his research group at Washington University, University of California, Irvine, and [Vanderbilt University. All rights reserved. 
Since the software is open-source, free software, you are free to use, modify, copy, and distribute--perpetually and irrevocably--the software source code and object code produced from the source, as well as copy and distribute modified versions of this software. You must, however, include this copyright statement along with code built using the software.
*/

#include "ace/config-all.h"
#include "ace/Reactor.h"
#include "ace/Mutex.h"
#include "ace/Null_Mutex.h"
#include "ace/Null_Condition.h"
#include "VacpService.h"
#include "ace/OS_NS_unistd.h"
#include "ace/os_include/os_netdb.h"
#include <exception>
#include <sstream>

#include "CxCommunicator.h"
#include "inmsafecapis.h"

int
VacpService::open (void)
{
  ACE_TCHAR peer_name[MAXHOSTNAMELEN];
  ACE_INET_Addr peer_addr;
  if (this->sock_.get_remote_addr (peer_addr) == 0 &&
      peer_addr.addr_to_string (peer_name, MAXHOSTNAMELEN) == 0)
	{
		DebugPrintf(SV_LOG_DEBUG, "Processing Connection Request from  %s\n",peer_name);
	}
  return this->reactor ()->register_handler (this, ACE_Event_Handler::READ_MASK);
}



int
VacpService::handle_input (ACE_HANDLE)
{
	bool rv = true;
	uint32_t totalTagDataLen;
	ssize_t r_bytes;
	uint32_t volbuffersize;
	bool isLatestClient = true;
	bool dataTypeConversionRequired  = false;


	r_bytes = this->sock_.recv_n ((void *)&totalTagDataLen, sizeof totalTagDataLen);
	if (r_bytes == -1)
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED: ACE_SOCK_Stream recv_n Error occurred return -1\n");
		return -1;
	}
	else if (r_bytes == 0)
	{
		DebugPrintf(SV_LOG_DEBUG, "Reached end of input, connection closed by client\n");
		return -1;
	}
	else
	{
		unsigned short us = 1;
		char isArchLE = *((char *)&us);
		if (memcmp("SVDB", &totalTagDataLen, sizeof totalTagDataLen) == 0)
		{
			DebugPrintf(SV_LOG_DEBUG, "Latest Client is Connected.\n");
			//If underlying arch is little endian and client is big endian than only we need to convert data.
			if(isArchLE)
			{
				DebugPrintf(SV_LOG_DEBUG, "Conversion to Little Endian is required.\n");
			   	dataTypeConversionRequired = true;
			}
			DebugPrintf(SV_LOG_DEBUG, "Connected Client is Big Endian.\n");
		}
		else if(memcmp("SVDL", &totalTagDataLen, sizeof totalTagDataLen) == 0)
		{
			DebugPrintf(SV_LOG_DEBUG, "Latest Client is Connected.\n");
			//If underlying arch is not little endian and client is little endian than only we need to convert data.
			if(!isArchLE)
			{
				DebugPrintf(SV_LOG_DEBUG, "Conversion to Big Endian is required.\n");
				dataTypeConversionRequired = true;
			}
		    DebugPrintf(SV_LOG_DEBUG, "Connected Client is Little Endian.\n");
		}
		else
		{
			isLatestClient = false;
			DebugPrintf(SV_LOG_DEBUG, "Legacy Client is Connected.\n");
			totalTagDataLen = ntohl(totalTagDataLen);
		}
	}

	if(isLatestClient)
	{
		r_bytes = this->sock_.recv_n ((void *)&totalTagDataLen, sizeof totalTagDataLen);
		if (r_bytes == -1)
		{
			DebugPrintf(SV_LOG_ERROR, "FAILED: ACE_SOCK_Stream recv_n Error occurred return -1\n");
			return -1;
		}
		else if (r_bytes == 0)
		{
			DebugPrintf(SV_LOG_DEBUG, "Reached end of input, connection closed by client\n");
			return -1;
		}
		else
		{
			totalTagDataLen = ntohl(totalTagDataLen);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"totalTagDataLen =  %ld\n", totalTagDataLen);

	char *TagDataBuffer = new char[totalTagDataLen];

	r_bytes = this->sock_.recv_n ((void *)TagDataBuffer, totalTagDataLen);
	if (r_bytes == -1)
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED: ACE_SOCK_Stream recv_n Error occurred return -1\n");
		return -1;
	}
	else if (r_bytes == 0)
	{
		DebugPrintf(SV_LOG_DEBUG, "Reached end of input, connection closed by client\n");
		return -1;	
	}
	else if (r_bytes != totalTagDataLen)
	{
		DebugPrintf(SV_LOG_ERROR, "Number of bytes received is not the same as totalTagDataLen\n");
		return -1;
	}

	int iPass = ProcessTags(TagDataBuffer, totalTagDataLen, dataTypeConversionRequired);
	
	ACE_OS::sleep(2);
	VACP_SERVER_RESPONSE vacpServerResponse = {0};
	
	std::string info = "Server VACP function Passed";
	if(!iPass)
	{
		info = "Server VACP function Failed";
	}

	inm_memcpy_s((void *)vacpServerResponse.str, sizeof(vacpServerResponse.str), info.c_str(), info.length());
	vacpServerResponse.iResult = htonl(iPass);
	vacpServerResponse.val = htonl(info.length());


	if (0 >= (r_bytes = this->sock_.send_n ((void *)&vacpServerResponse, sizeof vacpServerResponse))) 
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to send back acknowledgement to client\n");
		return -1;
	}
  return 0;
}

int
VacpService::handle_close (ACE_HANDLE, ACE_Reactor_Mask mask)
{
  if (mask == ACE_Event_Handler::WRITE_MASK)
    return 0;
  mask = ACE_Event_Handler::ALL_EVENTS_MASK |
         ACE_Event_Handler::DONT_CALL;
  this->reactor ()->remove_handler (this, mask);
  this->sock_.close ();
  delete this;
  return 0;
}


int
VacpService::OpenAndIssueIoctl(char *buffer)
{
	SV_INT rv = 0;
	do
	{
		ACE_HANDLE m_Driver = 
			ACE_OS::open(ACE_TEXT(INMAGE_FILTER_DEVICE_NAME), O_RDWR, FILE_SHARE_READ );

		if (ACE_INVALID_HANDLE == m_Driver) {
			DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :unable to open %s\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
			rv = -1;
			break;
		}

		if(ioctl(m_Driver, IOCTL_INMAGE_TAG_VOLUME, buffer) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, "ioctl failed: tags could not be issued. errno %d\n", errno);
			rv = -1;
			ACE_OS::close(m_Driver);
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "tags successfully issued\n");
			rv = 0;
			ACE_OS::close(m_Driver);
			break;
		}

	} while (0);

	return rv;
}

int
VacpService::ProcessTags(char *TagDataBuffer, uint32_t totalTagDataLen, bool isDataTypeConversionRequired)
{
	bool rv = true;
	ACE_INT32 iPass = 1;
	ssize_t r_bytes;
	uint32_t volbuffersize;

	unsigned short ulTotalNumberOfLuns = 0;
	unsigned short TotalNumberOfTags = 0;
	uint32_t ulBytesCopied = 0;
	unsigned short tagLen = 0;
	unsigned short usLunIdSize = 0;
	unsigned short usTagSize = 0;
	std::vector<std::string> vApplianceName;
	unsigned short totalApplianceNameLength = 0;
	uint32_t tagBuggerLen = 0;

	uint32_t ulFlag = 0;
	inm_memcpy_s(&ulFlag, sizeof(ulFlag), TagDataBuffer, sizeof(uint32_t));
	if(isDataTypeConversionRequired)
		ulFlag = ntohl(ulFlag);       //Convert from bigendian to little endian
	ulBytesCopied +=  sizeof(uint32_t);
	DebugPrintf(SV_LOG_DEBUG,"Flag:  %ld\n",ulFlag);

	//Total number of Luns
	inm_memcpy_s(&ulTotalNumberOfLuns, sizeof(ulTotalNumberOfLuns), TagDataBuffer + ulBytesCopied, sizeof(unsigned short));
	if(isDataTypeConversionRequired)
		ulTotalNumberOfLuns = ntohs(ulTotalNumberOfLuns);
	ulBytesCopied +=  sizeof(unsigned short);
	DebugPrintf(SV_LOG_DEBUG,"ToalNumberOfLuns :  %ld\n", ulTotalNumberOfLuns);
	LocalConfigurator m_localConfigurator;
	std::string agentMode = m_localConfigurator.getAgentMode();
	do {
		if ("host" != agentMode)
		{
			for(size_t i = 0;i < ulTotalNumberOfLuns; i++)
			{
				inm_memcpy_s(&usLunIdSize, sizeof(usLunIdSize), TagDataBuffer + ulBytesCopied, sizeof(unsigned short));
				if(isDataTypeConversionRequired)
					usLunIdSize = ntohs(usLunIdSize);
				ulBytesCopied +=  sizeof(unsigned short);
				DebugPrintf(SV_LOG_DEBUG,"Lun ID size : %ld\n", usLunIdSize);

				std::string deviceName(TagDataBuffer + ulBytesCopied, usLunIdSize);
				ulBytesCopied +=  usLunIdSize;
				DebugPrintf(SV_LOG_DEBUG,"Lun ID:  %s\n",  deviceName.c_str());

				std::string strApplianceName;

				try 
				{
                     size_t found = deviceName.rfind("/");
                     if (found != std::string::npos)
                     {
                        //Extract lun id from dev mapper path.
                        deviceName = deviceName.substr(found + 1); 
                     }
					 strApplianceName = CxComm.getAtLun(deviceName);
				}
				catch (...)
				{
					DebugPrintf(SV_LOG_ERROR,"GenerateSanDeviceName thrown an unknown error\n");
					return false;
				}

				DebugPrintf(SV_LOG_DEBUG,"Appliance Name: %s\n", strApplianceName.c_str());

				totalApplianceNameLength += strApplianceName.size();
				vApplianceName.push_back(strApplianceName);

			}
            if (totalApplianceNameLength <= 0)
            {
                DebugPrintf(SV_LOG_ERROR,"None of the Server Devices are protected by this Appliance. Tag will not be issued\n");
                return false;
            }

			char* pTagDataStartsFromHere = TagDataBuffer + ulBytesCopied;
			uint32_t TagLengthData = totalTagDataLen - ulBytesCopied;
			inm_memcpy_s(&TotalNumberOfTags, sizeof(TotalNumberOfTags), TagDataBuffer + ulBytesCopied, sizeof(unsigned short));
			if(isDataTypeConversionRequired)
			{
			  	TotalNumberOfTags = ntohs(TotalNumberOfTags);
				//converting tag data to native format and overwrite to original buffer.
				inm_memcpy_s(TagDataBuffer + ulBytesCopied, TagLengthData, &TotalNumberOfTags, sizeof(unsigned short));
			}
			ulBytesCopied +=sizeof(unsigned short);
			DebugPrintf(SV_LOG_DEBUG,"TotalNumberOfTags: %d\n", TotalNumberOfTags);

			for(size_t i = 0;i < TotalNumberOfTags; i++)
			{
                /*
                   +-------------------------------------------------------------+
                   |TotalTagSize|STREAM_REC_HDR_4B|Guid|STREAM_REC_HDR_4B|TagName|
                   +-------------------------------------------------------------+
                */
				inm_memcpy_s(&usTagSize, sizeof(usTagSize), TagDataBuffer + ulBytesCopied, sizeof(unsigned short));
				if(isDataTypeConversionRequired)
				{
			  		usTagSize = ntohs(usTagSize);
					//converting tag data to native format and overwrite to original buffer.
					inm_memcpy_s(TagDataBuffer + ulBytesCopied, (totalTagDataLen - ulBytesCopied), &usTagSize, sizeof(unsigned short));
				}
				ulBytesCopied +=  sizeof(unsigned short);
				DebugPrintf(SV_LOG_DEBUG,"Tag Data Length: %d\n", usTagSize);

              // We need to convert usStreamRecType of STREAM_REC_HDR_4B if we get a tag from big-endian client.
				if(isDataTypeConversionRequired)
				{
                    STREAM_REC_HDR_4B *pHdr4B;
                    pHdr4B = (STREAM_REC_HDR_4B *)(TagDataBuffer + ulBytesCopied);
                    pHdr4B -> usStreamRecType = ntohs(pHdr4B -> usStreamRecType); 
                }
                size_t streamSize = ((STREAM_REC_HDR_4B *)(TagDataBuffer + ulBytesCopied)) -> ucLength;
				std::string tagName(TagDataBuffer + ulBytesCopied + sizeof (STREAM_REC_HDR_4B), streamSize - sizeof (STREAM_REC_HDR_4B));
				DebugPrintf(SV_LOG_DEBUG, "Tag : %s\n", tagName.c_str());

				ulBytesCopied +=  streamSize;
                if (usTagSize >= streamSize)
                {
                    if(isDataTypeConversionRequired)
                    {
                        STREAM_REC_HDR_4B *pHdr4B;
                        pHdr4B = (STREAM_REC_HDR_4B *)(TagDataBuffer + ulBytesCopied);
                        pHdr4B -> usStreamRecType = ntohs(pHdr4B -> usStreamRecType); 
                    }
                    streamSize = ((STREAM_REC_HDR_4B *)(TagDataBuffer + ulBytesCopied)) -> ucLength;
                    tagName = std::string(TagDataBuffer + ulBytesCopied + sizeof (STREAM_REC_HDR_4B), streamSize -  sizeof (STREAM_REC_HDR_4B));
                    ulBytesCopied +=  streamSize;
                    DebugPrintf(SV_LOG_DEBUG, "Tag : %s\n", tagName.c_str());
                }
			}

			//Create packet of tag to send to deviceioctl
			uint32_t tagBufferLen = 0;
			ulBytesCopied = 0;


			tagBufferLen = ((sizeof(uint32_t)) + (sizeof(unsigned short)) + (sizeof(unsigned short) * vApplianceName.size()) + totalApplianceNameLength  + TagLengthData);

			char* TagDataBufferToDriver = new char[tagBufferLen];
			inm_memcpy_s(TagDataBufferToDriver + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulFlag, sizeof(uint32_t));
			ulBytesCopied += sizeof(uint32_t);

			//Copy # of ApplianceIds
			unsigned short ulNumberOfApplianceName = (unsigned short)vApplianceName.size();
			inm_memcpy_s(TagDataBufferToDriver + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfApplianceName, sizeof(unsigned short));
			ulBytesCopied += sizeof(unsigned short);

			//Copy all ApplianceName
			for(uint32_t i = 0; i < vApplianceName.size(); i++)
			{
				//Copy Lun ID length
				unsigned short size = vApplianceName[i].size();
				inm_memcpy_s(TagDataBufferToDriver + ulBytesCopied,(tagBufferLen-ulBytesCopied), &size, sizeof(unsigned short));
				ulBytesCopied += sizeof(unsigned short);

				//Copy appliance name
				inm_memcpy_s(TagDataBufferToDriver + ulBytesCopied,(tagBufferLen-ulBytesCopied), vApplianceName[i].c_str() , vApplianceName[i].size());
				ulBytesCopied += vApplianceName[i].size();
			}

            //Copy the Tag data to TagDataBufferToDriver
			inm_memcpy_s(TagDataBufferToDriver + ulBytesCopied,(tagBufferLen-ulBytesCopied), pTagDataStartsFromHere, TagLengthData);
			vApplianceName.clear();

			iPass = OpenAndIssueIoctl(TagDataBufferToDriver);
			if(iPass)
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED: in function  OpenAndIssueIoctl\n");
				rv = false;
				break;
			}

			if(TagDataBufferToDriver)
			{
				delete[] TagDataBufferToDriver;
				TagDataBufferToDriver = NULL;
			}
		}
		else
		{					
			//Sucess  = 1, Failure = 0	
			iPass = OpenAndIssueIoctl(TagDataBuffer);
			if(iPass)
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED: in function  OpenAndIssueIoctl\n");
				rv = false;
				break;
			}

			DebugPrintf(SV_LOG_DEBUG, "Stream got from client is %s\n",TagDataBuffer);					
		}
	}
	while (0);
	if (TagDataBuffer)
	{
		delete[] TagDataBuffer;
		TagDataBuffer = NULL;
	}

	return rv;
}
