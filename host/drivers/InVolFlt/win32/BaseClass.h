#ifndef _INMAGE_BASECLASS_CPP_H_
#define _INMAGE_BASECLASS_CPP_H_

// root class for all object, contains debugging support and reference counting
class BaseClass {
public:
    void * __cdecl operator new(size_t size) {return malloc(size, TAG_BASECLASS_OBJECT, NonPagedPool);};
    BaseClass();
    virtual ~BaseClass();
    void AddRef(void);  // increment reference count
    void Release(void); // decrement reference count
    virtual BOOLEAN Finalize();
protected:
    tInterlockedLong refCount; // destroy object when this goes to zero
};

#endif // _INMAGE_BASECLASS_CPP_H_