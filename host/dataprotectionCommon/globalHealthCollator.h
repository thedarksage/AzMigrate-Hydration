#include "HealthCollator.h"

class DataProtectionGlobalHealthCollator
{

private:
    static HealthCollatorPtr m_healthCollatorPtr;
    static boost::mutex m_healthCollatorPtrInstanceMutex;
    static std::string m_diskId;
    static std::string m_healthCollatorDirPath;
    static std::string m_hostId;

    // initializes the global health issues collator shared pointer
    static void init();

public:

    //Constructor
    DataProtectionGlobalHealthCollator(const std::string& diskId, const std::string& healthCollatorDirPath, const std::string& hostId);

    //Returns the initialized healthcollator
    static HealthCollatorPtr getInstance();

    //Destructor
    ~DataProtectionGlobalHealthCollator() {}
};
