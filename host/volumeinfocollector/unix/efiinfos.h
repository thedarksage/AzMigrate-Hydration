#ifndef _EFI__INFOS__H_
#define _EFI__INFOS__H_

#include <string>
#include "dlheadersminor.h"
#include "voldefs.h"
#include "portablehelpersmajor.h"

/* TODO: should we do this exercise on linux ? */
/* contains libefi.so handle so that we open only once; 
 * also has pointers to efiread and efifree */
class EfiInfo
{
    void *m_LibEfiHandle;
    void *m_pEfiRead;
    void *m_pEfiFree;

public:
    EfiInfo()
    {
        m_LibEfiHandle = 0;
        m_pEfiRead = 0;
        m_pEfiFree = 0;
        std::string libefiso;
        libefiso = LIBEFI_DIRPATH;
        libefiso += UNIX_PATH_SEPARATOR;
        libefiso += LIBEFINAME;
        m_LibEfiHandle = dlopen(libefiso.c_str(), RTLD_LAZY);

        if (m_LibEfiHandle)
        {
            const std::string EFI_ALLOC_AND_READ_FUNCTIONNAME = "efi_alloc_and_read";
            const std::string EFI_FREE_FUNCTIONNAME = "efi_free";

            m_pEfiRead = dlsym(m_LibEfiHandle, EFI_ALLOC_AND_READ_FUNCTIONNAME.c_str());
            m_pEfiFree = dlsym(m_LibEfiHandle, EFI_FREE_FUNCTIONNAME.c_str());
        }
    }

    ~EfiInfo()
    {
        if (m_LibEfiHandle)
        {
            dlclose(m_LibEfiHandle);
        }
    }

    void *GetEfiReadAddress(void)
    {
        return m_pEfiRead;
    }

    void *GetEfiFreeAddress(void)
    {
        return m_pEfiFree;
    }
};

typedef EfiInfo EfiInfo_t;

#endif /* _EFI__INFOS__H_ */
