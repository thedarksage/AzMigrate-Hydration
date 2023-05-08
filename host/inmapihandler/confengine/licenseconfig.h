#ifndef LICENSE_CONFIG_H
#define LICENSE_CONFIG_H
#include <boost/shared_ptr.hpp>
class LicenseConfig ;
typedef boost::shared_ptr<LicenseConfig> LicenseConfigPtr ;

class LicenseConfig
{
    LicenseConfig() ;
public:
    static LicenseConfigPtr GetInstance() ;
} ;
#endif
