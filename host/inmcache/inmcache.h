
///
/// \file inmcache.h
///
/// \brief class inm_cache_t can be used to cache pages from disk
/// 
/// 
///
///

#ifndef INMCACHE__H
#define INMCACHE__H

#include <boost/shared_ptr.hpp>

#include "inmtypes.h"
#include "inmstatus.h"

template <class T, class KEY_T>
class inm_cache_t
{
public:

    ///
    /// \brief constructor
    ///  instantiates the appropriate implementation class
    ///
    inm_cache_t(T * on_disk_store, ///< on disk store
        const inm_u32_t & page_size, ///< size of each object to be cached
        const inm_cache_options_t & options ///< cache options
        );

    ///
    /// \brief destructor
    ///   destroy the implementation instance
    ///
    ~inm_btree_cache_t()
    {
        m_impl.reset();
    }

    ///
    /// \brief return the cached data
    ///  if the data is not in the cache, we try and get 
    ///  the data into cache now. if the new attempt fails as well
    ///  return NULL which should be handled by the caller
    ///

    inm_status_t get_from_cache(const KEY_T & lookup_key, ///< offset to read the data from
        void * requested_page ///< requested page
        )
    {
        // return m_impl -> get_from_cache(offset, requested_page);
    }

    ///
    /// \brief write the data to disk and also update the cache
    ///

    inm_status_t update_cache_and_disk(
        const inm_u64_t &offset, ///< offset to write the data
        void * updated_page      ///< page to write 
        )
    {
        // return m_impl -> update_cache_and_disk(offset, updated_page);
    }

    ///
    /// \brief erase the cached entry
    ///

    inm_status_t erase_from_cache(
        const inm_u64_t &offset ///< erase cached entry for specified offset        
        )
    {
        // return m_impl -> erase_from_cache(offset);
    }


    ///
    /// \brief clean up the cache
    ///

    inm_status_t invalidate_cache()
    {
        return m_impl -> invalidate_cache();
    }


protected:

private:

    inm_cache_t(const inm_cache_t&);
    inm_cache_t& operator=(const inm_cache_t&);

    boost::shared_ptr<inm_cache_impl_t> m_impl;
};

#endif
