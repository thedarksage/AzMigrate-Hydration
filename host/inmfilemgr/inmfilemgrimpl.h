
///
/// \file inmfilemgrimpl.h
///
/// \brief class inm_filemgr_impl is base class for performing allocation and deallocation of storage in a disk file, 
///        much like a "freespace" manager
/// 


#ifndef INMFILEMGRIMPL__H
#define INMFILEMGRIMPL__H

#include "inmtypes.h"
#include "inmstatus.h"
#include "inmfilemgrtypes.h"

class inm_filemgr_impl_t
{

public:

    ///
    /// \brief constructor
    ///
    
    inm_filemgr_impl_t() 
    {
    
    }

    ///
    /// \brief destructor
    ///

    virtual ~inm_filemgr_impl_t() 
    {
    
    }

    ///
    /// \brief allocate the requested length bytes of storage from the file
    ///

    virtual inm_status_t allocate(const inm_u32_t & length, ///< length to allocate
        inm_extents_t & extents                             ///> list of allocated extents
        ) =0 ;

    ///
    /// \brief allocate the requested length bytes as a single contigous allocation
    ///

    virtual inm_status_t allocate_contigous(const inm_u32_t & length,
        inm_u64_t & offset) = 0;

    ///
    /// \brief deallocate previously allocated space
    ///

    virtual  inm_status_t deallocate(const inm_extent_t & space) =0;

    ///
    /// \brief deallocate previously allocated space
    ///

    virtual inm_status_t deallocate(const inm_extents_t & space) = 0;

    ///
    /// \brief return the list of free extents within the file
    ///

    virtual inm_status_t get_free_extent_list(inm_extents_t & extents) =0;

protected:

private:

};

#endif
