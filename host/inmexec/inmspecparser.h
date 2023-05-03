#ifndef INM_SPEC_PARSER_H
#define INM_SPEC_PARSER_H

#include <string>
#include <map>
#include <vector>
#include <boost/shared_ptr.hpp>

struct Data{
	std::string section_name;
	std::map<std::string,std::string> entries;
	typedef boost::shared_ptr<Data> ptr;
};

class inm_spec_parser {
public:
	inm_spec_parser(const std::string & specfilename):m_specfilename(specfilename),m_position(0){}
	~inm_spec_parser(){m_data_list.clear();}
	int parse();
	Data::ptr get_next_data();

private:
	int m_position;
	const std::string m_specfilename;
	std::vector<Data::ptr> m_data_list;
};

#endif 

