#ifndef UTIL__FUNCTIONSMAJOR__H_
#define UTIL__FUNCTIONSMAJOR__H_

#include <string>
#include "volumeinfocollectorinfo.h"
#include <unistd.h>
#include <inttypes.h>
bool parseSparseFileNameAndVolPackName(const std::string lineinconffile, std::string &sparsefilename, std::string &volpackname);
bool GetSymbolicLinksAndRealName(
                                 std::string const & deviceName, 
                                 VolumeInfoCollectorInfo::SymbolicLinks_t & symbolicLinks, 
                                 std::string & realName
                                );
bool AreDigits(const char *p);
void swap16bits(uint16_t *pdata);
bool IsLittleEndian(void);

#endif /* UTIL__FUNCTIONSMAJOR__H_ */

