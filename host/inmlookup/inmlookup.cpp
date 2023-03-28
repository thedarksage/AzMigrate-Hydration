#include "inmlookup.h"


InmLookup::InmLookup(LookupReaderWriter *plookupreaderwriter)
 :m_pLookupReaderWriter(plookupreaderwriter)
{
}


InmLookup::~InmLookup()
{
    /* TODO: signal done to m_pLookupReaderWriter ? */
}


bool InmLookup::UpdateStore(const std::string &key)
{
    bool bwritten = false;

    if (m_pLookupReaderWriter->Write(key))
    {
        bwritten = true;
    }
    else
    {
        m_ErrMsg = m_pLookupReaderWriter->GetErrorMessage();
    }

    return bwritten;
}


std::string InmLookup::GetErrorMessage(void)
{
    return m_ErrMsg;
}
