#include "inmlogger.h"


inm_logger * inm_logger::m_instance = NULL;


inm_logger * inm_logger::instance()
{
	if(m_instance == NULL)
	{
		m_instance = new inm_logger();
	}

	return m_instance;
}

void inm_logger::log_message(int log_level, std::string message)
{
	if(log_level)
	{
		std::cerr<<message<<std::endl;
	}
	else
	{
		std::cout<<message<<std::endl;
	}
	write_log(message, log_level);
}

void inm_logger::write_log(std::string message, int loglevel)
{
	try
	{
		std::ofstream outputstream;
		outputstream.open(m_outputfile.c_str(), std::ios_base::out | std::ios_base::app);
		if (!outputstream.good())
		{
			throw ERROR_EXCEPTION << "Could not open inmexec output file " << m_outputfile;
		}
		else
		{
			if (loglevel)
			{
				outputstream << "ERROR:" << std::endl;
			}
			else
			{
				outputstream << "DEBUG:" << std::endl;
			}
			outputstream << message << std::endl;
			outputstream.flush();
			outputstream.close();
		}
	}
	catch (std::exception & e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cout << "Unhandled exception. Something wrong in writing the inmexec logs to ouput file." << std::endl;
	}
}