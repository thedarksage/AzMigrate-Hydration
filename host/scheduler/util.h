#include "portablehelpers.h"
#include <time.h>
#include <string>
std::string convertTimeToString(SV_ULONGLONG timeInSec);
time_t convertStringToTime(std::string szTime);
int IsDayLightSavingOn( void ) ;