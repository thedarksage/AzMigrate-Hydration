/*****************************************************************************
*
* $Id: main.cpp,v 1.10.38.2 2017/10/13 13:56:12 bhayachi Exp $
*
*
* The imaginary form we'll fill in looks like:
*
* <form method="post" enctype="multipart/form-data" action="http://<csip>/cli/cx_cli_no_upload.php">
* XMLFile: <input type="file" name="xml" size="40">
* Operation: <input type="text" name="operation" size="40">
* <input type="submit" value="submit" name="submit">
* </form>
*
*/

#include <string.h>
#include <iostream>
#include "inmsafecapis.h"
#include "curlwrapper.h"
#include "terminateonheapcorruption.h"

using namespace std;

#include <curl/curl.h>
#include <curl/easy.h>
#include "CSApiHelper.h"

void usage()
{
	std::cerr << " <url> <xmlfile> [operation]\n"
		" -i <CSIP>\n "
		<< std::endl;
}

int main(int argc, char *argv[])
{
	TerminateOnHeapCorruption();
	init_inm_safe_c_apis();
	int rv = 1;

	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;

	char *operation;
	char *xmlfile;
	char *url;
	string csIP;
	int i = 1;
	bool argsOk = true;

	if (argc == 3)
	{
		switch (argv[i][1]) {
		case 'i':
			++i;
			if (i >= argc || '-' == argv[i][0]) {
				--i;
				std::cout << "*** missing value for -i\n";
				argsOk = false;
				break;
			}
			csIP = argv[i];
			break;
		default:
			std::cout << "*** invalid arg: " << argv[i] << "\n";
			argsOk = false;
			break;
		}

		if (!argsOk) {
			std::cout << "\n";
			usage();
			return 3;
		}

		csIP = argv[i];
		std::string xmlData = prepareXmlFile(csIP);
		std::string file;
#ifdef WIN32
		string strSysVol;
		if (false == GetSystemVolume(strSysVol))
		{
			std::cout << "Unable to fetch System drive." << endl;
			return 2;
		}

		file = strSysVol + "\\ProgramData\\ASRSetupLogs\\CxPsConfiguration.xml";
#else
		file = "/var/log/CxPsConfiguration.xml";
#endif

		processXmlData(xmlData, csIP, file);
		std::string cxpsSslPort;
		if (!ReadResponseXmlFile(file, "GetCxPsConfiguration", cxpsSslPort))
		{
			std::cout << "Response = failed  Output - " << cxpsSslPort << endl;
			return 1;
		}
		else
		{
			std::cout << cxpsSslPort;
			return 0;
		}
	}
	else if (argc < 3)
	{
		usage();
		return 4;
	}

	url = argv[1];
	xmlfile = argv[2];
	if(argc > 3)
	{
		operation = argv[3];
	}

	curl_global_init(CURL_GLOBAL_ALL);


	/* Fill in the file upload field */
	curl_formadd(&formpost,
		&lastptr,
		CURLFORM_COPYNAME, "xml",
		CURLFORM_FILECONTENT, xmlfile,
		CURLFORM_END);

	if(argc > 3)
	{
		/* Fill in the filename field */
		curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "operation",
			CURLFORM_COPYCONTENTS, operation,
			CURLFORM_END);
	}

	CurlOptions options(url);

	CurlWrapper cw;
	std::string result;

	try {
		cw.formPost(options,formpost, result);
		std::cout << result;
		rv = 0;
	} catch(ErrorException& exception )
	{
		std::cout << "<xml>" << std::endl
			<< "<cxcli_api>" << std::endl
			<< "<responce_code>FAILED</responce_code>" << std::endl
			<< "<info>" << exception.what() << "</info>" << std::endl
			<< "</cxcli_api>" << std::endl
			<< "</xml>" << std::endl;
		rv = 1;
	}


	// cleanup the formpost chain 
	curl_formfree(formpost);

	return rv;
}
