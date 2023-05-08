
///
/// \file inmbtcacheimpl.h
///
/// \brief class inm_btree_cache_t caches the btree nodes
/// 
/// inm_btree_cache_t is interface class and actual implementation is in *_impl classes
///
///

#ifndef INMFILEMGR__H
#define INMFILEMGR__H

#include <boost/shared_ptr.hpp>

#include "inmtypes.h"
#include "inmstatus.h"
#include "inmbtreetypes.h"
#include "inmbtree.h"

#include "inmbtcacheimpl.h"
#include "inmbtcacheimpl_levelone.h"

class inm_btree_cache_impl_t
{
public:

    ///
    /// \brief constructor
    ///  instantiates the appropriate implementation class
    ///
    inm_btree_cache_impl_t(inm_btree_t * bt, ///<
        const inm_btcache_options_t & options ///<
        )
    {
    }
    
    ///
    /// \brief destructor
    ///   destroy the implementation instance
    ///
    ~inm_btree_cache_impl_t()
    {
        m_impl.reset();
    }

    ///
    /// \brief
    ///
    inm_status_t get_from_cache(void * parent, ///<
        const inm_u64_t offset, ///<
        void * requested_node ///<
        )
    {
    }

    ///
    /// \brief
    ///
    inm_status_t invalidate_cache()
    {
        return m_impl -> invalidate_cache();
    }

    ///
    /// \brief
    ///
    inm_status_t validate_cache()
    {
        return m_impl -> validate_cache();
    }

protected:

private:

    boost::shared_ptr<inm_btree_cache_impl_t> m_impl;
};

#endif


class inm_btree_cache_impl_t
{
public:
    inm_btree_cache_t(inm_btree_t * bt,
        const inm_btcache_options_t & options);
    
    ~inm_btree_cache_t();
    inm_status_t get_from_cache(void * parent, const inm_u64_t offset, void * requested_node);
    inm_status_t invalidate_cache();
    inm_status_t validate_cache();

protected:

private:
    inm_status_t init();
    inm_status_t destroy();
};

class inm_btree_cache_impl_levelone_t
{
public:
    inm_btree_cache_t(inm_btree_t * bt,
        const inm_btcache_options_t & options);
    
    ~inm_btree_cache_t();
    inm_status_t get_from_cache(void * parent, const inm_u64_t offset, void * requested_node);
    inm_status_t invalidate_cache();
    inm_status_t validate_cache();

protected:

private:
    inm_status_t init();
    inm_status_t destroy();
};

