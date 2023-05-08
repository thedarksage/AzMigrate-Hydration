#ifndef _DRVUTIL_SEG_STREAM_H_
#define _DRVUTIL_SEG_STREAM_H_

#include <windows.h>
#include <vector>

class CDataStream{
public:
    virtual const void *buffer(size_t index, size_t cBytes) = 0;
    virtual void copy(size_t index, size_t cBytes, void* Dst) = 0;
    virtual size_t  size() = 0;
};

class CSegStream : public CDataStream{
public:
    CSegStream(void **BufferArray, unsigned long nBuffers, size_t StreamSize, size_t BufferSize);
    ~CSegStream();

    // This function returns either pointer into internal cache or
    // pointer into segment. Caller should assume data returned is valid
    // only till next call of this function. As next call of this function
    // can reuse the internal buffer.
    virtual const void *buffer(size_t index, size_t cBytes);

    // This function always copied data to address passed in Buffer
    virtual void copy(size_t index, size_t cBytes, void* Dst);
    virtual size_t size() { return m_StreamSize; }
    
private:
    CSegStream(const CSegStream &);
    CSegStream& operator=(const CSegStream&);

    char *GetInternalBuffer(size_t BufferSize);
private:
    std::vector<const char *>   m_vecSegments;
    size_t                      m_SegmentSize;
    size_t                      m_StreamSize;

    // m_Buffer and m_BufferSize are used for caching.
    // If requested data is in a single segment. pointer into segment is returned.
    // If requested data spans more than one segment, data is copied into internal
    // buffer (m_Buffer) and pointer to m_Buffer is returned.
    char *                      m_Buffer;
    size_t                      m_BufferSize;
    const static size_t         m_BufferSizeIncrement = 0x1000;
};


class CFileStream : public CDataStream{
public:
	CFileStream(const char * FileName);
    ~CFileStream();

    // This function returns either pointer into internal cache or
    // pointer into segment. Caller should assume data returned is valid
    // only till next call of this function. As next call of this function
    // can reuse the internal buffer.
    virtual const void *buffer(size_t index, size_t cBytes);

    // This function always copied data to address passed in Buffer
    virtual void copy(size_t index, size_t cBytes, void* Dst);
    virtual size_t size() { return m_StreamSize; }
    
private:
    CFileStream(const CFileStream &);
    CFileStream& operator=(const CFileStream&);
    
private:
	const char *		        m_FileName;
	FILE *						m_fp;
	size_t                      m_StreamSize;
    
    char *                      m_Buffer;
};

#endif // _DRVUTIL_SEG_STREAM_H_
