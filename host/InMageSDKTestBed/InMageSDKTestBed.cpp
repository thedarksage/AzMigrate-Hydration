#include <iostream>
#include <string>
#include "InmXmlParser.h"
#include "InMageSDK.h"
#include "logger.h"
#include "inmstrcmp.h"
#include "portable.h"

#define REQUEST		"-request"
#define RESPONSE	"-response"
#define LOGFILE_NAME ".\\Inmsdktestbed.log" 

using namespace std;

void displayUsage()
{
	cout<< "\nUsage: " << endl
		<< "To get response in a Xml file"
		<< "\nInMageSDKTestBed.exe -request <RequestXMLFilePath> -response <ResponseXMLFilePath>"
		<< "\n\nEX:InMageSDKTestBed.exe -request \"C:\\request.xml\" -response \"C:\\Response.xml\""
		<<"\n\nTo get response on standard output"
		<< "\nInMageSDKTestBed.exe -request <RequestXMLFilePath> -response stdout"
		<<"\n\nEX:InMageSDKTestBed.exe -request \"C:\\request.xml\" -response stdout"
		<< endl;
}

int main(int argc,char** argv)
{
//__asm int 3 ;
	//ACE::init() ;
    std::string RequestXML;
	std::string ResponseXML;
	INMAGE_ERROR_CODE errCode;
	if(argc != 5)
	{
		displayUsage();
		return 0;
	}
	for(int i=1; i<argc ; i++)
	{
		if(InmStrCmp<NoCaseCharCmp>(argv[i],REQUEST) == 0)
			RequestXML.assign(argv[++i]);
		else if(InmStrCmp<NoCaseCharCmp>(argv[i],RESPONSE) == 0)
			ResponseXML.assign(argv[++i]);
		else
		{
			displayUsage();
			return 0;
		}
	}
	
	if(InmStrCmp<NoCaseCharCmp>(ResponseXML.c_str(),"stdout")== 0)
	{
		std::stringstream instrm,outstream;
		std::string Inputxml,Outputxml;
		XmlRequestValueObject m_request;
		try
		{
			m_request.InitializeXmlRequestObject(RequestXML);
		}
		catch(exception e)
		{
			DebugPrintf(SV_LOG_ERROR, "Error while reading in xml file  %s\n",e.what()) ;
			std::cout<<"Error while reading in xml file :"<<e.what()<<std::endl;
			return 0;
		}
		m_request.XmlRequestSave(instrm);
		
		/*std::string Input = instrm.str();
		cout<<"\nResquest Xml stream:\n\n"<<Input.c_str();*/

		errCode = processRequestWithStream(instrm,outstream);
		//DebugPrintf(SV_LOG_ERROR, "Error code:%d returned from processRequestWithStream \n",ErrorCode(errCode) ;

		Outputxml = outstream.str();
		std::cout<<"\nResponse Xml stream:\n\n"<<Outputxml.c_str();
	}
	else
	{
		errCode = processRequestWithFile(RequestXML.c_str(),ResponseXML.c_str());
	}

	if(errCode!=0)
	{
		char *pErrorMessage = NULL;
		ErrorMessage(errCode,&pErrorMessage);
		cout << "\nError: " << pErrorMessage << endl;
		Inm_cleanUp(&pErrorMessage);
	}

	///////////////////////////////////////
	////for test response load and save
	//std::string testResponseXML = "C:\\xml_test\\latest\\stub\\GetRestorePoints_Response.xml";
	//XmlResponseValueObject m_response;
	//	try
	//	{
	//		m_response.InitializeXmlResponseObject(testResponseXML);
	//	}
	//	catch(exception e)
	//	{
	//		std::cout<<"Error while reading in xml file :"<<e.what()<<std::endl;
	//		return 0;
	//	}
	//std::stringstream outteststream;
	//m_response.XmlResponseSave(outteststream);
	//std::string testout = outteststream.str();
	//std::cout<<"\nTest getrestore Response Xml stream:\n\n"<<testout.c_str();
		
}
