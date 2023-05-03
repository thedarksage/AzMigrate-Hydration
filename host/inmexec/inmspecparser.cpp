#include "inmspecparser.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string/trim.hpp>
#include "inmlogger.h"

int inm_spec_parser::parse()
{
	int rv = 0;
	std::ifstream input_stream;
	input_stream.open(m_specfilename.c_str());

	if(input_stream.is_open())
	{
		Data::ptr next_data;
		while(input_stream.good())
		{
			std::string nextline;
			std::getline(input_stream,nextline);
			
			//Remove any blank spaces before and after the string.
			boost::algorithm::trim(nextline);

			//check if we have an empty line.
			if(nextline.empty())
				continue;


			//check if the give line is a comment.
			if(nextline.find_first_of("#")==0)
				continue;

			//Verify if the nextline is a new section. 
			if(nextline.find("[") != std::string::npos)
			{
				next_data.reset(new Data);
				m_data_list.push_back(next_data);
				nextline.erase(nextline.find("["),1);
				nextline.erase(nextline.find("]"));
				next_data->section_name = nextline;
			}
			else
			{
				std::string parameter;
				std::string value;
				size_t seperator_pos = 0;
				if((seperator_pos = nextline.find("="))!=std::string::npos)
				{
					parameter = nextline.substr(0,seperator_pos);
					boost::algorithm::trim(parameter);
					value = nextline.substr(seperator_pos+1);
					boost::algorithm::trim(value);
					if(next_data)
					{
						if(!value.empty())
						{
							std::pair<std::map<std::string,std::string>::iterator,bool> ret;
							ret = (next_data->entries).insert(std::pair<std::string,std::string>(parameter,value));
							if(!ret.second)
							{
								std::stringstream message;
								message<<"Duplicate entry ("<<parameter<<") in file "<<m_specfilename
									<<" for section "<<next_data->section_name<<std::endl;
								inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
								rv = 1; 
								break;
							}
						}
					}
					else
					{
						std::stringstream message;
						message<<"Found entry ("<<parameter<<") in file "<<m_specfilename<<" with out a section"<<std::endl;
						inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
						rv = 1; 
						break;
					}
				}
				else
				{
					std::stringstream message;
					message<<"Invalid entry ("<<nextline<<") in file "<<m_specfilename<<std::endl;
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
					rv = 1; 
					break;
				}
			}
		}

		input_stream.close();
	}
	else
	{
		rv = 1;
		std::stringstream message;
		message << "Failed to open file in file "<<m_specfilename<<std::endl; 
		inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
	}
	
	return rv;
}

Data::ptr inm_spec_parser::get_next_data()
{
	if(m_position < m_data_list.size())
	{
		return m_data_list[m_position++];
	}
	else
	{
		return Data::ptr();
	}
}

