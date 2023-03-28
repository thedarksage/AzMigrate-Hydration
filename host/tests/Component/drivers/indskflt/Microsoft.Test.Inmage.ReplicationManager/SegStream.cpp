#ifdef SV_WINDOWS
#include "stdafx.h"
#endif

#include "ReplicationManager.h"

CSegStream::CSegStream(void **BufferArray, unsigned long nBuffers, size_t StreamSize, size_t BufferSize) : 
    m_StreamSize(StreamSize), m_SegmentSize(BufferSize), m_BufferSize(0)
{
    assert(nBuffers >= ((StreamSize + BufferSize) / BufferSize));

    for (unsigned long i = 0; i < nBuffers; ++i) {
#ifdef SV_WINDOWS
        m_vecSegments.push_back((const char *)BufferArray[i]);
#else
        m_vecSegments.push_back(((const char *)BufferArray)+(i*BufferSize));
#endif
    }
}

CSegStream::~CSegStream()
{
}

char *
CSegStream::GetInternalBuffer(size_t BufferSize)
{
	if ((0 == m_Buffer.size()) || (m_Buffer.size() < BufferSize))
	{
		m_BufferSize = ((BufferSize + m_BufferSizeIncrement) / m_BufferSizeIncrement) * m_BufferSizeIncrement;
		m_Buffer.resize(m_BufferSize);
	}

    return &m_Buffer[0];
}

void
CSegStream::copy(size_t index, size_t cBytes, void* Dst)
{
    size_t  SegIndex = index / m_SegmentSize;
    size_t  SegOffset = index % m_SegmentSize;
    size_t  BytesRemainingInSegment = m_SegmentSize - SegOffset;
    size_t  BytesToCopyFromSegment = (cBytes > BytesRemainingInSegment) ? BytesRemainingInSegment : cBytes; 
    size_t  BytesCopied = 0;
    char    *Buffer = (char *)Dst;

    while (cBytes) 
	{
        const char *Segment = m_vecSegments.at(SegIndex);
        memcpy(&Buffer[BytesCopied], &Segment[SegOffset], BytesToCopyFromSegment);
        BytesCopied += BytesToCopyFromSegment;
        cBytes -= BytesToCopyFromSegment;
        SegOffset = 0;
        SegIndex++;

		BytesToCopyFromSegment = (cBytes < m_SegmentSize) ? cBytes : m_SegmentSize;
    }

    return;
}

const void *
CSegStream::buffer(size_t index, size_t cBytes)
{
    assert(index < m_StreamSize);
    assert((index + cBytes) <= m_StreamSize);

    if (index >= m_StreamSize) 
	{
        return NULL;
    }

    if (index + cBytes > m_StreamSize) 
	{
        return NULL;
    }

    size_t  SegOffset = index % m_SegmentSize;

    if ((SegOffset + cBytes) <=  m_SegmentSize) 
	{
        size_t SegIndex = index / m_SegmentSize;
        const char *Segment = m_vecSegments.at(SegIndex);
        return (const void *)&Segment[SegOffset];
    }

    // data is not in one segment.
    char *Buffer = GetInternalBuffer(cBytes);
    copy(index, cBytes, Buffer);

    return Buffer;
}
