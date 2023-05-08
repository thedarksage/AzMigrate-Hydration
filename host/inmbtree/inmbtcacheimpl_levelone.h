#include "inmbtreetypes.h"
#include "inmbtree.h"

class inm_btree_cache_t
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

    boost::shared_ptr<inm_filemgr_impl_t> m_impl;
};

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

