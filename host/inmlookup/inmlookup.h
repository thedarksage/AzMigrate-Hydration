#ifndef INMLOOKUP_H
#define INMLOOKUP_H

#include <string>

/* TODO: how to make this an interface along
 * with some templatized key and contents pairs ?
 * Because mixing template and abstract type is not good 
 */
class InmLookup
{
public:
    /* TODO: templatize key too */
    class LookupReaderWriter
    {
    public:
        /* TODO: avoid void * here and use template 
         * but cannot for now as mixing template and
         * abstract type */
        virtual void *Read(const std::string &key) = 0;    /* 0 return indicates failure */
        virtual bool Write(const std::string &key) = 0;    /* false return indicates failure */
        
        /* This is expected to write the contents to store and cache contents */
        virtual bool Write(const std::string &key, const void *contents) = 0;    /* false return indicates failure */
        virtual std::string GetErrorMessage(void) = 0;
    };

    InmLookup(LookupReaderWriter *plookupreaderwriter);
    ~InmLookup();

    /* template here because we want atleast the
     * user code to be type safe */
    template <typename CONTENTS>
        CONTENTS *GiveContents(const std::string &key);    /* 0 return indicates failure */

    /* no contents needed here as the assumption is that
     * the contents attached to this key are directly 
     * modified. That is cache is updated by application, 
     * this updates store */
    bool UpdateStore(const std::string &key);              /* false return indicates failure */
    std::string GetErrorMessage(void);
    
    /* name is kept same because final job of UpdateStore is same but
     * this one takes contents as input; This is expected to be called 
     * if key and contents do not already exist in cache; Subsequent 
     * updates should call the UpdateStore with no contents attached 
     * to key are directly given to application on GiveContents which 
     * is modified by application and UpdateStore is asked for */
    template <typename CONTENTS>
        bool UpdateStore(const std::string &key, const CONTENTS *contents);

    /* TODO: should there be methods to
     * 1. Remove key contents pair from cache ? This can 
     *    be done 
     * 2. should there be an option to handle first update 
     *    store ? Need to ask if this is needed ? If yes,
     *    provide one more function that takes contents 
     *    pointer and writes to file along with caching
     *    and the other details (mtime). But this has to
     *    be called only once. Subsequent calls have to
     *    update cache directly and then call the existing
     *    version of update store.
     */

private:
    LookupReaderWriter *m_pLookupReaderWriter;
    std::string m_ErrMsg;
};


template <typename CONTENTS> inline
    CONTENTS *InmLookup::GiveContents(const std::string &key)
{
    void *p;

    if ((p = m_pLookupReaderWriter->Read(key)) == 0)
    {
        m_ErrMsg = m_pLookupReaderWriter->GetErrorMessage();
    }

    CONTENTS *pc = static_cast<CONTENTS *>(p);
    return pc;
}


template <typename CONTENTS> inline
    bool InmLookup::UpdateStore(const std::string &key, const CONTENTS *contents)
{
    bool bwritten = false;

    if (m_pLookupReaderWriter->Write(key, contents))
    {
        bwritten = true;
    }
    else
    {
        m_ErrMsg = m_pLookupReaderWriter->GetErrorMessage();
    }

    return bwritten;
}

#endif /* INMLOOKUP_H */
