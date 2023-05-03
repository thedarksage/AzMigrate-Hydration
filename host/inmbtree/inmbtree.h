
///
/// \file inmbtree.h
///
/// \brief class inmbtree implements the b-tree 
/// 
/// 
///
///


#include "inmtypes.h"
#include "inmbtreetypes.h"
#include "inmbtcache.h"


template<typename bt_type> class inm_btree_t
{
public:

    inm_btree_t(boost::shared_ptr<inm_filemgr_t> filemgr,
        const inm_btree_mode_t& mode = BTREE_RW_EXISTING,
        const inm_u64_t & headerlocation = 0,
        const inm_btcache_algo_t & cache_algo = BTCACHE_LEVELONE,
        const inm_u32_t & pagesize = INM_BT_DEFAULT_PAGE_SIZE
        );

    ~inm_btree_t();

    inm_status_t find(const bt_type & searchkey, bt_type & matchingkey_and_value);
    inm_status_t insert(const bt_type & key_value,
		     inm_status_t (*callback)(bt_type & matching_key_value, const bt_type & search_key, void *) = 0,
		     inm_status_t (*callback)(bt_type & key_tobe_inserted, void *) = 0,
		     void * dup_callback_param =0,
		     void * nondup_callback_param =0);

    inm_status_t remove(const bt_type & key);
    inm_status_t replace_value(const bt_type & searchkey, const bt_type & replace_value);

    inm_status_t inorder_traverse(inm_status_t (*callback)(const bt_type &));
    inm_status_t preorder_traverse(inm_status_t (*callback)(const bt_type &));
    inm_status_t postorder_traverse(inm_status_t (*callback)(const bt_type &));
    inm_status_t levelorder_traverse(inm_status_t (*callback)(const bt_type &));

    void set_eq_compare(bool (*callback)(const bt_type & lhs, const bt_type & rhs));
    void set_lt_compare(bool (*callback)(const bt_type & lhs, const bt_type & rhs));
    void set_gt_compare(bool (*callback)(const bt_type & lhs, const bt_type & rhs));

    inm_u32_t height();

protected:

private:

    inm_status_t init();
    inm_status_t read(const inm_u64_t & offset, const inm_u32_t length, void * buffer);


    boost::shared_ptr<inm_filemgr_t> m_filemgr;
    boost::shared_ptr<inm_btcache_t> m_cache;
    Fio m_fio;
    inm_u64_t m_hdroffset;
    bt_hdr_t m_hdr;
    inm_u32_t m_minkeys;
    inm_u32_t m_maxkeys;
    inm_u32_t m_degree;

    ///
    /// \brief
    ///

    bool (*eq_compare)(const bt_type & lhs, const bt_type & rhs);
    bool (*lt_compare)(const bt_type & lhs, const bt_type & rhs);
    bool (*gt_compare)(const bt_type & lhs, const bt_type & rhs); 
};

