#ifndef SV_UTILS__H
#define SV_UTILS__H

void trim(std::string& s, std::string stuff_to_trim);
SVERROR GetProcessOutput(const std::string& cmd, std::string& output);

#endif
