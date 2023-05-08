
struct bt_hdr_t
{
  inm_u32_t magicbytes;
  inm_u32_t hdrlen;
  inm_u32_t version;
  inm_u32_t keylength;
  inm_u32_t pagesize;
  inm_u64_t root_offset;
  inm_u8_t  unused[1];
};


enum inm_btcache_algo_t { BTCACHE_NONE, BTCACHE_LEVELONE };
enum inm_btree_mode_t { BTREE_RW_EXISTING , BTREE_OVERWRITE };

struct inm_btcache_options_t
{
    inm_btcache_algo_t cachealgo;
};

#define INM_BT_DEFAULT_PAGE_SIZE 4096