///
/// \file pushspecparser.h
///
/// \brief
///

#ifndef INMAGE_PUSHSPEC_PARSER_H
#define INMAGE_PUSHSPEC_PARSER_H

#include <string>
#include <map>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace PI {

	struct PushCliSpec {
		std::string section_name;
		std::map<std::string, std::string> entries;
		typedef boost::shared_ptr<PushCliSpec> ptr;
	};

	class PushSpecParser {
	public:

		PushSpecParser(const std::string & specFile) :m_specFileName(specFile) {}
		~PushSpecParser(){ m_spec.clear(); }

		void parse();

		std::string get(const std::string & section, const std::string & key);

	private:
		const std::string m_specFileName;
		std::vector<PushCliSpec::ptr> m_spec;
	};

}

#endif