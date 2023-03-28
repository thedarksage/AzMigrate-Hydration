#include "InmConfig.h"

class BaseClass;
    class OSDevice;
    class AsyncIORequest;
    class IOBuffer;
    class FileStream;
    class Registry;
    class SegmentedBitmap;
    class SegmentMapper;
        class FileStreamSegmentMapper;


class BaseClass;
// This is the root of a single inheritance tree. This class has support for 
// reference counting, event communication, and some basic debugging.

class Policy;
// This is an abstract superclass to represent a policy of some kind. Policies
// are used to make decisions for lower level components. They may decide which
// concrete class to make instances of depending on operating modes, or may
// change this based on user configuration. I expect Policy objects to interact
// with a user interface or configuration data in the registry.

class OSDevice;
// Instances represent very simple OS device objects. For example we may create
// an object that's used to communicate IOCTL's to the driver. Instances do not
// understand PnP. This class implement generic dispatch for major
// IRP OS calls into the driver, and routes them to the virtual dispatch function
// based on the IRP major code and the per device filter device object.

class AsyncIORequest;
// Instances of this class ae expected to be short lived, and represent an
// single I/O operation. There are methods to read/write/flush/ioctl, and an
// event system to inform some object when an I/O has completed. They can also
// do synchronous I/O

class FileStream;
// A FileStream represents a device/file object that AsyncIORequests are
// performed on. This class handles the parts that are long lived, like 
// open/close.

class Registry;
// This is a class with easy to use functions for registry access. A very
// important feature is OS API's used function in a boot driver, most of the
// fancier ones don't.

class SegmentedBitmap;
// This represents a bitmap that is broken into pieces as it's too big to
// be memory resident. It's used for dirty block tracking. 

class SegmentMapper;
// This is an abstract class for some large byte array that can only be
// accessed in chunks. Among other things, it defines the protocol for a
// SegmentedBitmap to a storage area. Segment mappers are also responsible for
// address translation. For example a FileStream on a RAW device may need to be
// split into multiple areas. A segment mapper turns offset 0 for callers to this
// class, into the offset actually needed. Initially, only support for contigious
// mapping is expected (i.e. adding a base offset)

class FileStreamSegmentMapper;
// This is a SegmentMapper that is stored on a FileStream object. It handles
// buffer management for the views through the SegmentMapper protocol. It also
// handles the I/O by communicating with IOBuffers.

class IOBuffer;
// This is a class that packages a memory buffer that represents a segment of a
// disk based structure. IOBuffer's know their location and how to access the
// disk or file that they represent. IOBuffers also know when they are dirty, so
// don't match the disk.



#include "Common.h"
#include "DrvDefines.h"

// these defs are needed for the W2k ddk
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif


typedef tInterlockedLong tEventCode;

#define NotifyEventGeneric (1)
#define NotifyEventIoCompletion (2)


#include "Platform.h"
#include "CPP-RunTimeSupport.h"
#include "BaseClass.h"
#include "OSDevice.h"
#include "AsyncIORequest.h"
#include "FileRawIOWrapper.h"
#include "IOBuffer.h"
#include "FileStream.h"
#include "SegmentMapper.h"
#include "FileStreamSegmentMapper.h"
#include "BitmapOperations.h"
#include "SegmentedBitmap.h"
#include "Registry.h"
#include "ScopeTrace.h"
#include "..\..\..\..\ThirdParty\md5_drv\md5.h"



