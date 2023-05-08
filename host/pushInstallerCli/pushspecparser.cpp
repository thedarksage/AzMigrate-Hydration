///
/// \file pushspecparser.cpp
///
/// \brief
///

#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string/trim.hpp>

#include "errorexception.h"
#include "pushspecparser.h"


namespace PI
{

	void PushSpecParser::parse()
	{

		std::ifstream input_stream;
		input_stream.open(m_specFileName.c_str());

		if (input_stream.is_open())
		{
			PushCliSpec::ptr next_section;
			while (input_stream.good())
			{
				std::string nextline;
				std::getline(input_stream, nextline);

				//Remove any blank spaces before and after the string.
				boost::algorithm::trim(nextline);

				//check if we have an empty line.
				if (nextline.empty())
					continue;


				//check if the give line is a comment.
				if (nextline.find_first_of("#") == 0)
					continue;

				//Verify if the nextline is a new section. 
				if (nextline.find("[") != std::string::npos)
				{
					next_section.reset(new PushCliSpec);
					m_spec.push_back(next_section);
					nextline.erase(nextline.find("["), 1);
					nextline.erase(nextline.find("]"));
					next_section->section_name = nextline;
				}
				else
				{
					std::string parameter;
					std::string value;
					size_t seperator_pos = 0;
					if ((seperator_pos = nextline.find("=")) != std::string::npos)
					{
						parameter = nextline.substr(0, seperator_pos);
						boost::algorithm::trim(parameter);
						value = nextline.substr(seperator_pos + 1);
						boost::algorithm::trim(value);
						if (next_section)
						{
							if (!value.empty())
							{
								std::pair<std::map<std::string, std::string>::iterator, bool> ret;
								ret = (next_section->entries).insert(std::pair<std::string, std::string>(parameter, value));
								if (!ret.second)
								{
									std::stringstream message;
									message << "duplicate entry (" << parameter << ") in file " << m_specFileName
										<< " for section " << next_section ->section_name;
									throw ERROR_EXCEPTION << message.str();
								}
							}
						}
						else
						{
							std::stringstream message;
							message << "found entry (" << parameter << ") in file " << m_specFileName << " with out a section" << std::endl;
							throw ERROR_EXCEPTION << message.str();
						}
					}
					else
					{
						std::stringstream message;
						message << "fnvalid entry (" << nextline << ") in file " << m_specFileName << std::endl;
						throw ERROR_EXCEPTION << message.str();
					}
				}
			}

			input_stream.close();
		}
		else
		{
			std::stringstream message;
			message << "failed to open " << m_specFileName << std::endl;
			throw ERROR_EXCEPTION << message.str();
		}
	}

	std::string PushSpecParser::get(const std::string & section, const std::string & key)
	{
		std::vector<PushCliSpec::ptr>::iterator sectionIter = m_spec.begin();
		for (; sectionIter != m_spec.end(); ++sectionIter) {
			if ((*sectionIter)->section_name == section) {
				std::map<std::string, std::string>::iterator keyIter = (*sectionIter)-> entries.find(key);
				if (keyIter != (*sectionIter)->entries.end())
				{
					return keyIter->second;
				}
			}
		}

		return std::string();
	}

}