#include "SegStream.h"
#include "assert.h"
#include "inmsafecapis.h"

CSegStream::CSegStream(void **BufferArray, unsigned long nBuffers, size_t StreamSize, size_t BufferSize) : 
    m_StreamSize(StreamSize), m_SegmentSize(BufferSize), m_BufferSize(0), m_Buffer(NULL)
{
    assert(nBuffers >= ((StreamSize + BufferSize) / BufferSize));

    for (unsigned long i = 0; i < nBuffers; ++i) {
        m_vecSegments.push_back((const char *)BufferArray[i]);
    }
}

CSegStream::~CSegStream()
{
    if (NULL != m_Buffer) {
        delete[] m_Buffer;
    }
}

char *
CSegStream::GetInternalBuffer(size_t BufferSize)
{
    if (NULL != m_Buffer) {
        if (m_BufferSize < BufferSize) {
            delete[] m_Buffer;
            m_Buffer = NULL;
        }
    }

    if (NULL == m_Buffer) {
        m_BufferSize = ((BufferSize + m_BufferSizeIncrement) / m_BufferSizeIncrement) * m_BufferSizeIncrement;
        m_Buffer = new char[m_BufferSize];
    }

    return m_Buffer;
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

    while (cBytes) {
        const char *Segment = m_vecSegments.at(SegIndex);
		inm_memcpy_s(&Buffer[BytesCopied], cBytes, &Segment[SegOffset], BytesToCopyFromSegment);
        BytesCopied += BytesToCopyFromSegment;
        cBytes -= BytesToCopyFromSegment;
        SegOffset = 0;
        SegIndex++;
        if (cBytes < m_SegmentSize) {
            BytesToCopyFromSegment = cBytes;
        } else {
            BytesToCopyFromSegment = m_SegmentSize;
        }
    }

    return;
}

const void *
CSegStream::buffer(size_t index, size_t cBytes)
{
    assert(index < m_StreamSize);
    assert((index + cBytes) <= m_StreamSize);

    if (index >= m_StreamSize) {
        return NULL;
    }

    if (index + cBytes > m_StreamSize) {
        return NULL;
    }

    size_t  SegOffset = index % m_SegmentSize;

    if ((SegOffset + cBytes) <=  m_SegmentSize) {
        size_t SegIndex = index / m_SegmentSize;
        const char *Segment = m_vecSegments.at(SegIndex);
        return (const void *)&Segment[SegOffset];
    }

    // data is not in one segment.
    char *Buffer = GetInternalBuffer(cBytes);
    copy(index, cBytes, Buffer);

    return Buffer;
}

CFileStream::CFileStream(const char * FileName)
:m_Buffer(NULL)
{		
	m_FileName = FileName;

	m_fp = fopen (m_FileName,"rb");
	if (!m_fp)
		printf("%s: File open failed for %s \n", __FUNCTION__, m_FileName);
	else
	{
		// Calculate SVD data file size
		fseek (m_fp, 0, SEEK_END);
		m_StreamSize = ftell (m_fp);
		fseek (m_fp, 0, SEEK_SET);
	}
}

CFileStream::~CFileStream()
{
	fclose(m_fp);
	if (NULL != m_Buffer) {
		delete[] m_Buffer;
	}		
}

void
CFileStream::copy(size_t index, size_t cBytes, void* Dst)
{	
	// no of items to read  = 1 
	const int count = 1;

	// set file pointer at offset = index from the beginning 
	if ( fseek (m_fp,(long)index, SEEK_SET ))
	{
		printf("%s: File seek failed at offset %ld for %s \n", __FUNCTION__, index , m_FileName);
		return;
	}
	
	if( count != fread (Dst, cBytes, count, m_fp ))
	{
		printf("%s: File read failed for %d no of bytes for %s \n", __FUNCTION__, cBytes , m_FileName);	
		return;
	}

	return;

}

const void *
CFileStream::buffer(size_t index, size_t cBytes)
{
	assert(index < m_StreamSize);
	assert((index + cBytes) <= m_StreamSize);

	if (index >= m_StreamSize) {
		return NULL;
	}

	if (index + cBytes > m_StreamSize) {
		return NULL;
	}

	if (NULL != m_Buffer) {		
		delete[] m_Buffer;
		m_Buffer = NULL;		
	}

	if (NULL == m_Buffer) {		
		m_Buffer = new char[cBytes];
	}

	char *Buffer = m_Buffer;
	copy(index, cBytes, Buffer);

	return Buffer;

}



