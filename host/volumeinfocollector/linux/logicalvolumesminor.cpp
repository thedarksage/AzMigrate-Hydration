#include <cstring>
#include <set>
#include <map>
#include <cctype>
#include "logicalvolumes.h"

bool IsDmPartition(const Lv_t &disklv,  const Lv_t &partitionlv)
{
    bool bisdmpart = false;
    const char *d = disklv.m_Name.c_str();
    const char *p = partitionlv.m_Name.c_str();

    size_t dlen = strlen(d);
    size_t plen = strlen(p);

    if ((plen > dlen) && (0 == strncmp(d, p, dlen)) && isdigit(p[plen - 1]))
    {
        bisdmpart = (disklv.m_Devt && (partitionlv.m_InsideDevts.end() != partitionlv.m_InsideDevts.find(disklv.m_Devt)));
    }

    return bisdmpart;
}
