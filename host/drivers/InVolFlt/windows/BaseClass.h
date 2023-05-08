#pragma once

// root class for all object, contains debugging support and reference counting
class BaseClass {
public:
    void * __cdecl operator new(size_t size);
    BaseClass();
    virtual ~BaseClass();
    void AddRef(void);  // increment reference count
    void Release(void); // decrement reference count
    virtual BOOLEAN Finalize();
protected:
    tInterlockedLong refCount; // destroy object when this goes to zero
};

