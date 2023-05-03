#ifndef _INMAGE_REG_MULTISZ_H_
#define _INMAGE_REG_MULTISZ_H_

#include "Common.h"
#include "global.h"

class CRegMultiSz : public BaseClass {
public:
    CRegMultiSz();
    virtual ~CRegMultiSz();

    void * __cdecl operator new(size_t size) {return malloc(size, TAG_REGISTRY_MULTISZ, NonPagedPool);};
    NTSTATUS    SetMultiSz(PWCHAR pMultSz, ULONG ulCbLen);
    void        ResetEnumeration();
    NTSTATUS    GetNextSz(PWCHAR *ppNextSz);
protected:
    PWCHAR  m_pMultiSz;
    size_t  m_MaxCch;
    size_t  m_ProcessedCch;
    PWCHAR  m_pNextSz;
    ULONG   m_ulNumSzReturned;
};

#endif // _INMAGE_REG_MULTISZ_H_