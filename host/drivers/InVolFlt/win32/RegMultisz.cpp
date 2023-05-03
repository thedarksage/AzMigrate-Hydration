#include "RegMultisz.h"

CRegMultiSz::CRegMultiSz()
{
    m_pMultiSz = NULL;
    m_MaxCch = 0;
    m_ProcessedCch = 0;
    m_pNextSz = NULL;
    m_ulNumSzReturned = 0;
}

CRegMultiSz::~CRegMultiSz()
{
    if (NULL != m_pMultiSz)
        ExFreePoolWithTag(m_pMultiSz, TAG_REGISTRY_DATA);

}

void
CRegMultiSz::ResetEnumeration()
{
    m_pNextSz = m_pMultiSz;
    m_ProcessedCch = 0;
    m_ulNumSzReturned = 0;
}

NTSTATUS
CRegMultiSz::SetMultiSz(PWCHAR pMultiSz, ULONG ulCbLen)
{
    ULONG   ulIndex, ulMaxCch;
    BOOLEAN    bValid = FALSE;

    // pMultiSz has to be DOUBLE NULL Terminated.
    ASSERT(0 == (ulCbLen % sizeof(WCHAR)));

    ulMaxCch = ulCbLen / sizeof(WCHAR);
    for (ulIndex = 0; (ulIndex + 1) < ulMaxCch; ulIndex++) {
        if ((L'\0' == pMultiSz[ulIndex]) && (L'\0' == pMultiSz[ulIndex + 1])) {
            bValid = TRUE;
            break;
        }
    }

    if (bValid) {
        m_pMultiSz = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, ulCbLen, TAG_REGISTRY_DATA);
        if (NULL == m_pMultiSz)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlMoveMemory(m_pMultiSz, pMultiSz, ulCbLen);
        m_MaxCch = ulMaxCch;
    } else {
        return STATUS_INVALID_PARAMETER;
    }

    ResetEnumeration();
    return STATUS_SUCCESS;
}

NTSTATUS
CRegMultiSz::GetNextSz(PWCHAR *pSz)
{
    NTSTATUS    Status;
    size_t      StringLenCch;

    ASSERT(m_MaxCch >= m_ProcessedCch);

    if (NULL == pSz)
        return STATUS_INVALID_PARAMETER;
    *pSz = NULL;

    if (m_ProcessedCch >= m_MaxCch)
        return STATUS_NO_MORE_ENTRIES;

    Status = RtlStringCchLengthW(m_pNextSz, (m_MaxCch - m_ProcessedCch), &StringLenCch);
    ASSERT(Status != STATUS_INVALID_PARAMETER);

    if (STATUS_INVALID_PARAMETER == Status)
        return STATUS_NO_MORE_ENTRIES;

    if ((STATUS_SUCCESS == Status) && (0 == StringLenCch))
        return STATUS_NO_MORE_ENTRIES;

    *pSz = m_pNextSz;

    m_pNextSz += StringLenCch + 1;
    m_ProcessedCch += StringLenCch + 1;
    m_ulNumSzReturned++;

    ASSERT(m_MaxCch >= m_ProcessedCch);
    return STATUS_SUCCESS;
}
