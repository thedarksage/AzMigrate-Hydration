#include "appglobals.h"
#include "wmiutil.h"

std::string WMIDateTimeToStr(std::string datetimeStr)
{
    std::string year, month, date, hour, mins, secs; 
    year = datetimeStr.substr(0, 4); 
    month = datetimeStr.substr(4, 2) ;
    date = datetimeStr.substr(6, 2) ;
    hour = datetimeStr.substr(8, 2) ;
    mins = datetimeStr.substr(10, 2) ;
    secs = datetimeStr.substr(12, 2) ;
    return std::string( year + "-" + month + "-" + date + " " + hour + ":" + mins + ":" + secs ) ;
}