#pragma once
#include <string>

class CertUtil {
public:
    explicit CertUtil(const std::string& server,
        const std::string& port);

    bool validate_root_cert(std::string& errmsg);

    std::string get_last_error()
    {
        return m_last_error;
    }

private:
    std::string m_last_error;
    std::string m_lastcertincertchain;
};
