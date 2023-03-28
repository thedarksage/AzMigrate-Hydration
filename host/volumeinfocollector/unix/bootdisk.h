#ifndef _BOOTDISK__MAJOR__H_
#define _BOOTDISK__MAJOR__H_

#include <string>

class BootDiskInfo
{
    std::string m_BootDisk;

private:
    void KnowBootDisk(void);

public:
    BootDiskInfo()
    {
        KnowBootDisk();
    }

    std::string GetBootDisk(void)
    {
        return m_BootDisk;
    }
};

typedef BootDiskInfo BootDiskInfo_t;

#endif /* _BOOTDISK__MAJOR__H_ */
