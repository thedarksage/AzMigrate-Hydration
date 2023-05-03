#ifndef _INVOLFLT_DRIVER_DEFINED_TAGS_H__
#define _INVOLFLT_DRIVER_DEFINED_TAGS_H__

// These are tags are in sub name space of kernel defined tags.
// Two most significant bits are used to indicate the type of tag
// 00 - Unicode string
// 01 - String in US-ASCII codepage
// 10 - Binary data
// 11 - Reserved.
// ----------------------------------------------------------------
// | 2 bits Type |                   14 bits ID                   |
// ----------------------------------------------------------------
//
#define TAG_TYPE_UNKNOWN    0x00
#define TAG_TYPE_UNICODE    0x01
#define TAG_TYPE_BINARY     0x02
#define TAG_TYPE_NAMESPACE  0x03

#define TAG_TYPE_MASK       0xC000
#define TAG_ID_MASK         0x3FFF
#define DEFINE_TAG(Id, Type) ((USHORT)((Type << 14) & TAG_TYPE_MASK) | (Id & TAG_ID_MASK))

typedef struct _STREAM_REC_DATA_TYPE_UNICODE {
    USHORT  usLen;  // Len is in bytes and includes NULL terminator. This
                    // filed does not include any padding left in the record.
    WCHAR   wString[1]; // This string includes NULL terminator
}STREAM_REC_DATA_TYPE_UNICODE, *PSTREAM_REC_DATA_TYPE_UNICODE;

#define STREAM_REC_TYPE_START_OF_TAG_LIST   0x0001
#define STREAM_REC_TYPE_END_OF_TAG_LIST     0x0002
#define STREAM_REC_TYPE_TIME_STAMP_TAG      0x0003
#define STREAM_REC_TYPE_DATA_SOURCE         0x0004
#define STREAM_REC_TYPE_USER_DEFINED_TAG    0x0005
#define STREAM_REC_TYPE_PADDING             0x0006

// This is defined in vacp project.
#define STREAM_REC_TYPE_USERDEFINED_EVENT   0x0108

// STREAM_REC_TYPE_USER_DEFINED_NAMESPACE is top level name space
#define STREAM_REC_TYPE_USER_DEFINED_NAMESPACE          DEFINE_TAG(0x07, TAG_TYPE_NAMESPACE)
// STREAM_REC_TYPE_KERNEL_NAMESPACE is a second leve name space for kernel define tags
// This tag will always be in sub name space defined by STREAM_REC_TYPE_USER_DEFINED_NAMESPACE

#define STREAM_REC_TYPE_KERNEL_NAMESPACE                DEFINE_TAG(0x01, TAG_TYPE_NAMESPACE)

#define KERNEL_TAG_BITMAP_TO_METADATA_MODE_CHANGE       DEFINE_TAG(0x01, TAG_TYPE_UNICODE)
#define KERNEL_TAG_BITMAP_TO_DATA_MODE_CHANGE           DEFINE_TAG(0x02, TAG_TYPE_UNICODE)
#define KERNEL_TAG_METADATA_TO_DATA_MODE_CHANGE         DEFINE_TAG(0x03, TAG_TYPE_UNICODE)
#define KERNEL_TAG_METADATA_TO_BITMAP_MODE_CHANGE       DEFINE_TAG(0x04, TAG_TYPE_UNICODE)
#define KERNEL_TAG_DATA_TO_METADATA_MODE_CHANGE         DEFINE_TAG(0x05, TAG_TYPE_UNICODE)
#define KERNEL_TAG_DATA_TO_BITMAP_MODE_CHANGE           DEFINE_TAG(0x06, TAG_TYPE_UNICODE)

#endif //_INVOLFLT_DRIVER_DEFINED_TAGS_H__
