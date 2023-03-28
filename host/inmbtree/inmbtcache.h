
///
/// \file inmbtcache.h
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

class inm_btree_cache_t
{
public:

    ///
    /// \brief constructor
    ///  instantiates the appropriate implementation class
    ///
    inm_btree_cache_t(inm_btree_t * bt, ///< btree object
        const inm_btcache_options_t & options ///< cache options
        )
    {
        switch(options.cachealgo)
        {

        case BTCACHE_LEVELONE:
            m_impl.reset(new inm_btree_cache_impl_levelone_t(bt,options));
            break;

        case BTCACHE_NONE:
        default:
            m_impl.reset(new inm_btree_cache_impl_none_t(bt,options));
            break;

        };
    }

    ///
    /// \brief destructor
    ///   destroy the implementation instance
    ///
    ~inm_btree_cache_t()
    {
        m_impl.reset();
    }

    ///
    /// \brief returned the cached node
    ///  if the node is not in the cache, we try and get 
    ///  the node into cache now. if the new attempt fails as well
    ///  return NULL which should be handled by the caller
    ///

    inm_status_t get_from_cache(void * parent, ///< parent node
        const inm_u64_t offset, ///< offset to read the node from
        void * requested_node ///< requested node (output parameter)
        )
    {
        return m_impl -> get_from_cache(parent, offset, requested_node);
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

    inm_btree_cache_t(const inm_btree_cache_t&);
    inm_btree_cache_t& operator=(const inm_btree_cache_t&);

    boost::shared_ptr<inm_btree_cache_impl_t> m_impl;
};

#endif
