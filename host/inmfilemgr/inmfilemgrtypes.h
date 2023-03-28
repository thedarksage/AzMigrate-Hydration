
///
/// \file inmfilemgrtypes.h
///
/// \brief data types used by inmfilemgr interface and implementation classes
/// 
///

#ifndef INMFILEMGRTYPES__H
#define INMFILEMGRTYPES__H

///
/// \brief allocation and de-allocation algorithm
///
/// ALLOC_ALGO_NONE:
///   a no-op algorithm i.e it does not perform any de-allocation.
///   allocations are performed at end of the file
///

enum inm_allocation_algo_t { ALLOC_ALGO_NONE };


///
/// \brief file open mode passed as input parameter to inm_filemgr
/// 
/// FILEMGR_RW_ALWAYS:
///   open existing or create new file for both reading and writing
///
/// FILEMGR_READ_OVERWRITE:
///   open and truncate existing or create new file for both reading and writing
///

enum inm_filemgr_mode_t { FILEMGR_RW_ALWAYS , FILEMGR_READ_OVERWRITE };


///
/// \brief inm_filemgr allocates in terms of extents if contigous allocation is
///        not possible.
/// 

struct inm_extent_t
{
    inm_extent_t():offset(0), length(0) {}
    inm_extent_t(inm_u64_t o, inm_u32_t l):offset(o),length(l) {}

    inm_u64_t offset; /// \brief base offset of allocated space
    inm_u32_t length; /// \brief length of allocated space
};

///
/// \brief a list of allocated extents
///

typedef std::vector<inm_extent_t> inm_extents_t;

#endif
