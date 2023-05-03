#ifndef APP_PARSER_H
#define APP_PARSER_H
#include <string>
#include <ace/Get_Opt.h>
#include <ace/Configuration.h> 
#include <ace/Configuration_Import_Export.h>

#include "appconfigvalueobj.h"

class ConfigValueParser
{
    private:
        std::string m_szconfigPath; 
    public:
        ConfigValueParser();
        ConfigValueParser(std::string);
        bool parseInput( AppConfigValueObject& ); 
        bool parseConfigFileInput( AppConfigValueObject& );
        void fillVacpOptionsDetails(ACE_Configuration_Heap& , 
                            ACE_Configuration_Section_Key&,
                            AppConfigValueObject&) ;
        void fillRecoveryPointDetails(ACE_Configuration_Heap&, 
                             ACE_Configuration_Section_Key&,
                            AppConfigValueObject&) ;
        void fillRecoveryOptionsDetails(ACE_Configuration_Heap& configHeap, 
                            ACE_Configuration_Section_Key& sectionKey,
                            RecoveryOptions & recoveryOptions) ;
        void fillSQLInstanceDetails(ACE_Configuration_Heap& configHeap, 
                             ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj );
		void fillReplPairStatusDetails(ACE_Configuration_Heap& configHeap,
							 ACE_Configuration_Section_Key& sectionKey,
							 AppConfigValueObject& obj );
        void fillCustomVolumeDetails(ACE_Configuration_Heap& configHeap, 
                             ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj );
         bool UpdateConfigFile( AppConfigValueObject& obj);
};

#endif