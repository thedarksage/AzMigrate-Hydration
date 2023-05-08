#ifndef AZURE_RCM_CLI_EXCP_H
#define AZURE_RCM_CLI_EXCP_H

#include <iostream>
#include <string>

class AzureRcmCliException : public std::exception
{
    std::string m_what;
    int m_status;
public:
    explicit AzureRcmCliException(
        const std::string & message,
        int exitcode)
        : m_what(message),
        m_status(exitcode)
    {}

    const char * what() const throw()
    {
        return m_what.c_str();
    }

    int status() const
    {
        return m_status;
    }

    ~AzureRcmCliException() throw()
    {}
};

#endif // AZURE_RCM_CLI_EXCP_H
