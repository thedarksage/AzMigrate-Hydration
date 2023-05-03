#include "configmerge.h"
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/text_oarchive.hpp> 
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream> 
#include <fstream> 
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>

bool configmerge::run()
{
	int ret = true;
	try
	{
		/*
			Parse command lines
			Read both (old as well as new)config files
			Get a differential map for both files
			depending on operation specified ,perform either merge or display
			if overwrite flag is specified,get the map for this as well
			and then merge it with old config file(which by this time contains all new config file changes)
		*/
		parsecommandline();
		saveoldconffile(); //save a copy of old config file . 
		processconfigfile(m_oldfilepath,m_oldconfig);
		if(!m_overwrite)
		{
	 		processconfigfile(m_newfilepath,m_newconfig);
			calculatediffinconfigmaps();
			//validation done at the time of parsing
			if(boost::iequals(m_operation.c_str(),std::string("display")))
			{
				//send display's data to console or to file
				if(boost::iequals(m_output.c_str(),std::string("stdout")))
				{
					if(!m_diffconfigmap.empty())
						displaydifferenceinconfigfiles();
					else
						std::cout << "\n\n    ***** Files are identical . *****  " << std::endl;
				}
				else
				{
					writediffdatatofile();
				}
			}
			else
			{
				//Merge the contents of new conf file with old conf file. 
				mergeconfigfiles();
			}
		}
		else //if user wants to overwrite the values of fold config file with the new ones
		{
			//read the overwrite file 
			//get the config details from that file
			//overwrite the corresponding value into oldconfig file
			m_newconfig.clear(); //erasing older contents
			processconfigfile(m_overwritefilepath,m_newconfig); //reusing the newconfig map
			overwriteoldconfigfile();
		}
	}
	catch(std::string ex)
	{
		std::cout << "Excaption caught . Exception description :\n " << ex  << std::endl;
		ret = false;
	}
	return ret;
}

void configmerge::mergeconfigfiles()
{	
	ACE_Configuration_Heap configHeap ;
	if( configHeap.open() >= 0 )
	{
		ACE_Ini_ImpExp iniImporter( configHeap );
		if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(m_oldfilepath.c_str())) >= 0)
		{
			configMap::const_iterator diffiter  = m_diffconfigmap.begin();
				while(diffiter != m_diffconfigmap.end())
				{
					std::string sectionname = diffiter->first;
					//write this section
					ACE_Configuration_Section_Key sectionKey;
					if(configHeap.open_section( configHeap.root_section(), ACE_TEXT_CHAR_TO_TCHAR(sectionname.c_str()), true, sectionKey) < 0){
						std::stringstream ss;
						ss << std::string("couldn't create section : ") ;
						ss << sectionname;
						ss << std::string (" Error code  :") <<  ACE_OS::last_error();
						throw ss.str();
					}
					keyvaluemap::const_iterator keymapiter = diffiter->second.begin();
					//write all the keys under this section
					while(keymapiter != diffiter->second.end())
					{
						if(configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(keymapiter->first.c_str()),ACE_TEXT_CHAR_TO_TCHAR( keymapiter->second.c_str())) < 0) {
							std::stringstream ss;
							ss << std::string("couldn't create key : ") ;
							ss << keymapiter->first;
							ss << std::string (" Error code  :") <<  ACE_OS::last_error();
							throw ss.str();

						}
						keymapiter++;
					}
					diffiter++;
				}
				if(iniImporter.export_config(ACE_TEXT_CHAR_TO_TCHAR(m_oldfilepath.c_str())) < 0)
				{
					std::stringstream ss;
					ss << std::string("Failed to export configuration file : ");
					ss << m_oldfilepath;
					ss << std::string (" Error code  :") <<  ACE_OS::last_error();
					throw ss.str();         
				}
		}	
		else
		{
			std::stringstream ss;
			ss << std::string("Failed to read configuration file : ");
			ss << m_oldfilepath;
			ss << std::string (" Error code  :") <<  ACE_OS::last_error();
			throw ss.str();
		}
	}
	else
	{
		std::stringstream ss;
		ss << std::string("Failed to initialize parser object for  file : ");
		ss << m_oldfilepath;
		ss << std::string (" Error code  :") <<  ACE_OS::last_error();
		throw ss.str();
	}
}
void configmerge::displaydifferenceinconfigfiles()
{
	std::map<std::string,std::map<std::string,std::string> >::iterator iter = m_diffconfigmap.begin();
	while(iter != m_diffconfigmap.end())
	{
		std::cout << "________________________________________________" << std::endl;
		std::cout << std::setw(10)  << iter->first << std::endl;
		std::map<std::string,std::string> innermap = iter->second;
		std::map<std::string,std::string>::iterator it = innermap.begin();
		std::cout << "------------------------------------------------ " << std::endl;
		while(it != innermap.end())
		{
			std::stringstream formatstring ;
			formatstring << std::setw(40)  << std::setiosflags( std::ios::left ) << it->first << std::setw(40) << it->second ;
			std::cout <<  formatstring.str() << std::endl;
			it++;
		}
		std::cout << "------------------------------------------------ " << std::endl;
		iter++;
		std::cout << std::endl << std::endl << std::endl;
	}
}

void configmerge::parsecommandline( )
{
	namespace po = boost::program_options;
	std::stringstream errException;
	std::string errString;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "help message")
		("oldfilepath ", po::value<std::string>(&(m_oldfilepath))," Absoulte path of old drscout.conf file ")
		("newfilepath ", po::value<std::string>(&(m_newfilepath)) ," Absoulte path of new drscout.conf file ")
		("operation ", po::value<std::string>(&(m_operation)),"   operation to perform (merge | display) ")
		("output ",  po::value<std::string>(&(m_output )) ," where to send display operation's data : (stdout | file <temp file<diff.txt> in local directory will  be created>).  ")
		("overwritefilepath ", po::value<std::string>(&(m_overwritefilepath)),"  Absoulte path of overwitten file  ")
		;

	po::variables_map vm;
	try
	{
		po::store(po::parse_command_line(m_argc, m_argv, desc), vm);
		po::notify(vm);    
	}
	catch(po::error er)
	{
		errException << desc << std::endl;
		throw errException.str();;
	}

	if(vm.count("help")) 
	{
		errException << desc << std::endl;
		throw errException.str();
	}

	if(m_oldfilepath.empty() && m_newfilepath.empty() && m_operation.empty() && m_output.empty())
	{
		errException << "Invalid arguments specified." << std::endl;
		errException << " \n\n Usage : " << desc << std::endl;
		throw errException.str() ;
	}
	
	if( !m_overwritefilepath.empty())
	{
		m_overwrite = true;
	}

	/*if((stricmp(m_operation.c_str(),"merge") != 0 ) && (stricmp(m_operation.c_str(),"display") != 0 ))
	{
		errException << " Invalid option specified with operation switch. Valid options are \"merge | display\"" << std::endl;
		errException << " \n\n Usage : " << desc << std::endl;
		throw errException.str() ;
	}*/
	if((!boost::iequals(m_operation.c_str(),"merge")) && (!boost::iequals(m_operation.c_str(),"display")))
	{
		errException << " Invalid option specified with operation switch. Valid options are \"merge | display\"" << std::endl;
		errException << " \n\n Usage : " << desc << std::endl;
		throw errException.str() ;
	}

	if((m_operation == std::string("display")) && (!boost::iequals(m_output.c_str(),"stdout")) && (!boost::iequals(m_output.c_str(),"file") != 0 ))
	{
		errException << " Invalid option specified with output switch. Valid options are \"stdout | file\"" << std::endl;
		errException << " \n\n Usage : " << desc << std::endl;
		throw errException.str() ;
	}

}

void configmerge::processconfigfile(std::string& configfile,std::map<std::string,std::map<std::string,std::string> >& configmap)
{
	ACE_Configuration_Heap configHeap ;
	int keyIndex = 0 ;
	ACE_TString sectionName;
	ACE_TString tStrKeyName;

    if( configHeap.open() >= 0 )
    {
        ACE_Ini_ImpExp iniImporter( configHeap );
		if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(configfile.c_str())) >= 0)
        {
            int sectionIndex = 0 ;
            ACE_Configuration_Section_Key sectionKey;
            ACE_TString tStrSectionName;
            while( configHeap.enumerate_sections(configHeap.root_section(), sectionIndex, tStrSectionName) == 0 )
            {
				std::map<std::string,std::string> keyvaluemap;
                configHeap.open_section(configHeap.root_section(), tStrSectionName.c_str(), 0, sectionKey) ;
				std::string sectionName = ACE_TEXT_ALWAYS_CHAR(tStrSectionName.c_str());
				//std::cout << "SectionName = " <<sectionName << std::endl;
				ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
				keyIndex = 0 ;

				while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
				{
					ACE_TString tStrValue ;
					configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
					keyIndex++;

					std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
					std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
					//std::cout << " keyName = " << keyName << std::endl;
					//std::cout << " value = " << value << std::endl;
					keyvaluemap.insert(std::make_pair(keyName,value));
				}
				configmap.insert(std::make_pair(sectionName,keyvaluemap));
				sectionIndex++;
			}
		}
		else
		{
			std::stringstream ss;
			ss << std::string("Failed to read configuration file : ");
			ss << configfile;
			ss << std::string (" Error code  :") <<  ACE_OS::last_error();
			throw ss.str();
		}
	}
	else
	{
		std::stringstream ss;
		ss << std::string("Failed to initialize parser object for  file : ");
		ss << configfile;
		ss << std::string (" Error code  :") <<  ACE_OS::last_error();
		throw ss.str();
	}
}
void configmerge::calculatediffinconfigmaps()
{
	/*std::map<std::string,std::map<std::string,std::string> >::iterator iter = m_oldconfig.begin();
	while(iter != m_oldconfig.end())
	{
		std::cout << iter->first << std::endl;
		std::map<std::string,std::string> innermap = iter->second;
		std::map<std::string,std::string>::iterator it = innermap.begin();
		while(it != innermap.end())
		{
			std::cout << it->first << it->second << std::endl;
			it++;
		}
		iter++;
		getchar();
	}*/

	/*
		Loop till newconfig file 
		 search the first section of new config file into old config file
			if (section doesnt exist)
				insert it into diff map
			else (section does exist)
				enumerate the keys of old and new config file for the above mentioned section
				if(key is missing)
					insert it along with its value

				
	*/
	configMap::iterator newconfigiter = m_newconfig.begin();
	while(newconfigiter != m_newconfig.end())
	{
		configMap::iterator oldconfigiter = m_oldconfig.find(newconfigiter->first);
		if(oldconfigiter == m_oldconfig.end()) //this section not in old drscout.conf file
		{
			//Since section keys does not exist,lets add all values realed to this section into diffmap
			m_diffconfigmap.insert(std::make_pair(newconfigiter->first,newconfigiter->second));
		}
		else //section exists now enumerate keys
		{
			//Enumerate all the keys of both maps
			keyvaluemap& newconfigkeys = newconfigiter->second;
			keyvaluemap& oldconfigkeys = oldconfigiter->second;

			keyvaluemap::iterator newkeysiter = newconfigkeys.begin();
			while(newkeysiter != newconfigkeys.end())
			{
				//map<section_name,map<key,value> >
				keyvaluemap::iterator oldkeyiter = oldconfigkeys.find(newkeysiter->first);
				if(oldkeyiter == oldconfigkeys.end()) // Key does not exist in old config file
				{
					//section exist ??
					//configMap::iterator& diffconfigiter = m_diffconfigmap.find(newconfigiter->first); 
					//on gcc 4.5 its failing with "invalid initialization of non-const reference of type" error
					configMap::iterator diffconfigiter = m_diffconfigmap.find(newconfigiter->first);
					if(diffconfigiter == m_diffconfigmap.end()) //no need of this check,dead block,always else will be executed
					{	
						//this section doesnt exist in diff map yet .Lets insert it along with keys
						keyvaluemap diffkeyconfig;
						diffkeyconfig.insert(std::make_pair(newkeysiter->first,newkeysiter->second));
						m_diffconfigmap.insert(std::make_pair(newconfigiter->first,diffkeyconfig));
					}
					else // seciton exist
					{
						//lets insert this value directly into diffconfigmap.
						keyvaluemap& insertmap = diffconfigiter->second ;
						//insert the newest key and its value
						insertmap.insert(std::make_pair(newkeysiter->first,newkeysiter->second));
						//push the value into diff map
						m_diffconfigmap[diffconfigiter->first] = insertmap;
					}
				} 
				/*
					else if key does exist,then its overwrite scenario,handled separately
				*/
				newkeysiter++;
			}
		}
		newconfigiter++;
	}
}

/*
	Write the contents of overwrite map to oldconfig file
	m_newconfig contains the overwrtie config file's contents
*/
void configmerge::overwriteoldconfigfile()
{
	ACE_Configuration_Heap configHeap ;
	if( configHeap.open() >= 0 )
	{
		ACE_Ini_ImpExp iniImporter( configHeap );
		if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(m_oldfilepath.c_str())) >= 0)
		{
			configMap::const_iterator diffiter  = m_newconfig.begin();
				while(diffiter != m_newconfig.end())
				{
					std::string sectionname = diffiter->first;
					//write this section
					ACE_Configuration_Section_Key sectionKey;
					if(configHeap.open_section( configHeap.root_section(), ACE_TEXT_CHAR_TO_TCHAR(sectionname.c_str()), false, sectionKey) < 0){
						std::stringstream ss;
						ss << std::string("couldn't open section") ;
						ss << sectionname;
						ss << std::string (" Error code  :") <<  ACE_OS::last_error();
						throw ss.str();
					}
					keyvaluemap::const_iterator keymapiter = diffiter->second.begin();
					//write all the keys under this section
					while(keymapiter != diffiter->second.end())
					{
 						if(configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(keymapiter->first.c_str()),ACE_TEXT_CHAR_TO_TCHAR( keymapiter->second.c_str())) < 0) {
							std::stringstream ss;
							ss << std::string("couldn't create key : ") ;
							ss << keymapiter->first;
							ss << std::string (" Error code  :") <<  ACE_OS::last_error();
							throw ss.str();

						}
						keymapiter++;
					}
					diffiter++;
				}
				if(iniImporter.export_config(ACE_TEXT_CHAR_TO_TCHAR(m_oldfilepath.c_str())) < 0)
				{
					std::stringstream ss;
					ss << std::string("Failed to export configuration file : ");
					ss << m_oldfilepath;
					ss << std::string (" Error code  :") <<  ACE_OS::last_error();
					throw ss.str();         
				}
		}	
		else
		{
			std::stringstream ss;
			ss << std::string("Failed to read configuration file : ");
			ss << m_oldfilepath;
			ss << std::string (" Error code  :") <<  ACE_OS::last_error();
			throw ss.str();
		}
	}
	else
	{
		std::stringstream ss;
		ss << std::string("Failed to initialize parser object for  file : ");
		ss << m_oldfilepath;
		ss << std::string (" Error code  :") <<  ACE_OS::last_error();
		throw ss.str();
	}
}
void configmerge::writediffdatatofile()
{
	//create temp file in local directory
	//dump diff map here
	std::ofstream file("diff.txt"); 
	if(!file.is_open())
	{
		std::stringstream ss;
		ss << "Failed to open file " << std::endl;
		throw ss.str();
	}
	if(!m_diffconfigmap.empty())
	{
		configMap::iterator iter = m_diffconfigmap.begin();
		while(iter != m_diffconfigmap.end())
		{
			file  << "[";
			file  << iter->first ;
			file  << "]" << "\n" ;
			keyvaluemap::iterator keyvaliter = iter->second.begin();
			while(keyvaliter != iter->second.end())
			{
				file << keyvaliter->first << "	" << keyvaliter->second << "\n";
				keyvaliter++;
			}
			iter++;
		}
	}
	else
	{
		file << "***** Files are identical . ***** " << std::endl;
	}
	file.close();
	std::cout << "Please find file <\"diff.txt\"> under current directory "  << std::endl;
}

void configmerge::saveoldconffile()
{
	namespace bfs=boost::filesystem;
	ACE_Time_Value startTime = ACE_OS::gettimeofday();
	std::string oldfilename = m_oldfilepath;

	try
	{
		oldfilename += boost::lexical_cast<std::string>( startTime.sec() ) ;
		bfs::copy_file(m_oldfilepath,oldfilename);
	}
	catch(boost::bad_lexical_cast ex)
	{
		std::string exception = ex.what();
		throw exception;
	}
	catch (boost::filesystem::filesystem_error &e) 
	{ 
		std::string exception = e.what();
		throw exception;
	} 
}
