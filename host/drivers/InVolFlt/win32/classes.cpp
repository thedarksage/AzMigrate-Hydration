// generic code for classes

//
// Copyright InMage Systems 2004
//

#include <global.h>

PVOID __cdecl malloc(ULONG32 x,  ULONG32 id, POOL_TYPE pool)
{
    ASSERT((NonPagedPool == pool) || (PagedPool == pool));
    PVOID buffer = ExAllocatePoolWithTag(pool, x, id);

    if (buffer) {
        //
        // clear our buffer to zero
        //
        RtlZeroMemory(buffer, x);
    }

    return buffer;
}


void __cdecl free(PVOID x)
{
    if (x != NULL) { // the c++ spec requires freeing a null pointer
        ExFreePool(x);
    }
}


void * __cdecl operator new(size_t size)
{
    return malloc(size, TAG_GENERIC_NON_PAGED, NonPagedPool);
}


void * __cdecl operator new(size_t size,  void *location)
{
    return location;
}

void *__cdecl operator new( size_t size, ULONG32 tag, POOL_TYPE pool)
{
    return malloc(size, tag, pool);
}

void __cdecl operator delete(void * pVoid)
{
    free(pVoid);
}

#if 0
// do something appropriate if you call a pure virtual function
// The Win2k3 kernel exports __purecall ntoskrnl:ntoskrnl.exe
int __cdecl _purecall( void )
{
        assert( 0 );
}
#endif


//*******************************************************************************
// misc defines to make RTTI happy
//*******************************************************************************
class type_info {
public:
    virtual ~type_info();
    int operator==(const type_info& rhs) const;
    int operator!=(const type_info& rhs) const;
    int before(const type_info& rhs) const;
    const char* name() const;
    const char* raw_name() const;
private:
    void *_m_data;
    char _m_d_name[1];
    type_info(const type_info& rhs);
    type_info& operator=(const type_info& rhs);
};

type_info::type_info(const type_info& rhs)
{
}

type_info& type_info::operator=(const type_info& rhs)
{
        return *this;
}

type_info::~type_info()
{
}


