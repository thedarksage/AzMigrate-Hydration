#ifndef READINFO_H
#define READINFO_H

#include "svtypes.h"
#include "volume.h"
#include "cdpvolume.h"
#include "inmquitfunction.h"
#include "sharedalignedbuffer.h"


class ReadInfo
{
public:
    typedef struct ReadRetryInfo
    {
        bool m_PutZerosForFailure;
        unsigned int m_NumberOfRetries;
        unsigned int m_RetryInterval;

        ReadRetryInfo(const bool &putzerosforfailure, 
                      const unsigned int &numberofretries,
                      const unsigned int &retryinterval);
        ReadRetryInfo(const ReadRetryInfo &rhs);
		ReadRetryInfo();
        
    } ReadRetryInfo_t;
    
    typedef struct ReadInputs
    {
        cdp_volume_t *m_pVolume;
        SV_LONGLONG m_Offset;
        unsigned int m_Length;
        bool m_ShouldSeek;
        ReadRetryInfo_t m_ReadRetryInfo;
        QuitFunction_t m_QuitFunction;
		
        ReadInputs(cdp_volume_t *pvolume, const SV_LONGLONG &offset,
                   const unsigned int &length, const bool &shouldseek,
                   const ReadRetryInfo_t &rri,
                   QuitFunction_t &quitfunction);
    } ReadInputs_t;

public:
    ReadInfo();
    ~ReadInfo();
    bool Read(ReadInputs_t &ri);
    bool Init(const SV_UINT &allocatelen, const SV_UINT &alignment);
    void Print(void);
    void Reset(void);
    char * GetData(void);
    SV_UINT GetLength(void);
    SV_LONGLONG GetOffset(void);
    std::string GetErrorMessage(void);

private:
    bool AllocateReadBuffer(const SV_UINT &allocatelen, const SV_UINT &alignment);

private:
    SharedAlignedBuffer m_Data;
    SV_UINT m_AllocatedLen;
    SV_UINT m_Length;
    SV_LONGLONG m_Offset;
    std::string m_ErrorMessage;
};

#endif /* READINFO_H */

