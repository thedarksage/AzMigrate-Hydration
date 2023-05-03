#ifndef PARSER_H
#define PARSER_H
#include <string>
#include <ace/Get_Opt.h>
#include <ace/Configuration.h> 
#include <ace/Configuration_Import_Export.h>
#include <time.h>
#include <fstream>

#include "portablehelpers.h" 

#include "configvalueobj.h"

class ConfigValueParser
{
    private:
        int m_count;
        ACE_TCHAR** m_cmdLine; 
        std::string m_szconfigPath; 
	
    public:
        ConfigValueParser(){}
        ConfigValueParser(int, ACE_TCHAR**);
        bool parseInput( ConfigValueObject& ); 
        
        bool parseCmdLineInput( ConfigValueObject& );
        bool setLongOption( ACE_Get_Opt& );
        
        bool parseConfigFileInput( ConfigValueObject& );
        void fillRemoteOfficeDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
        void fillHcapDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
        void fillContenetSourceDetails(ACE_Configuration_Heap&,ACE_Configuration_Section_Key&,struct ContentSource& );
        void parseFilePattern( std::string tempValue, ContentSource& src );
        void fillTunablesDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
        void fillConfigDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
        void fillDeleteDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
        void fillFilelistDetails(ACE_Configuration_Heap& ,ACE_Configuration_Section_Key&,ConfigValueObject& );
		
        void varifyConfigDuplication();
        void ValidateArgument(ACE_TCHAR argument[], ACE_TString option);
        void ValidateInput( ConfigValueObject& );
        void icatUsage();
        void configFileUsage();
        void cmdLineUsage();
};

#endif

