#ifndef INM_LOGGER_H__
#define INM_LOGGER_H__

#include <iostream>
#include <string>
#include "inmcmd.h"

class inm_logger
{
public:
	enum log_level
	{
		INM_LOG_INFO = 0,
		INM_LOG_ERROR =1
	};
	static inm_logger * instance();
	std::string m_outputfile;

	void log_message(int log_level, std::string message);
protected:
private:
	inm_logger(){}
	~inm_logger(){}

	static inm_logger * m_instance;
	void write_log(std::string message, int loglevel);
};

#endif

