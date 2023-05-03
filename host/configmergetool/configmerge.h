#ifndef CONFIG_MERGE_TOOL
#define CONFIG_MERGE_TOOL

#include <iostream>
#include <string>
#include <exception>
#include <map>
#include <ace/Get_Opt.h>
#include <ace/Configuration.h> 
#include <ace/Configuration_Import_Export.h>

typedef std::map<std::string,std::map<std::string,std::string> > configMap;
typedef std::map<std::string,std::string>  keyvaluemap;

class configmerge
{
private:
	int m_argc;
	char** m_argv;

	bool m_overwrite;

	std::string m_oldfilepath;
	std::string m_newfilepath;
	std::string m_overwritefilepath;
	std::string m_operation;
	std::string m_output;
	configMap m_oldconfig;
	configMap m_newconfig;
	configMap m_diffconfigmap;
private:
	void saveoldconffile();
	void parsecommandline();
	void processconfigfile(std::string&,std::map<std::string,std::map<std::string,std::string> >& );
	void enumeratesectiondetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&);
	void calculatediffinconfigmaps();
	void displaydifferenceinconfigfiles();
	void mergeconfigfiles();
	void overwriteoldconfigfile();
	void writediffdatatofile();
public:
	explicit configmerge(int argc,char* argv[])
	{
		m_argc = argc;
		m_argv = argv;
		m_overwrite = false;
	}
	~configmerge()
	{
		//release file handles
	}

	bool run();
	
};


#endif