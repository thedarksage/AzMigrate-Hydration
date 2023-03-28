
//
// Copyright InMage Systems 2004
//

#include <global.h>



BaseClass::BaseClass()
{
    refCount = 1; // assume there is a single pointer from the caller
}

BaseClass::~BaseClass()
{
}

void BaseClass::AddRef()
{
    InterlockedIncrement(&refCount);
}

void BaseClass::Release()
{
LONG newCount;

    newCount = InterlockedDecrement(&refCount);
    if (newCount == 0) {
        if (this->Finalize()) {
            delete this;
        }
    }
}

BOOLEAN BaseClass::Finalize()
{
    return TRUE;
}
