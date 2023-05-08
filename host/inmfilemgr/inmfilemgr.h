
///
/// \file inmfilemgr.h
///
/// \brief class inm_filemgr allocates and deallocates storage in a disk file, much like a "freespace" manager
/// 
/// inm_filemgr is interface class and actual implementation is in *_impl classes
///
///


#ifndef INMFILEMGR__H
#define INMFILEMGR__H

#include <boost/shared_ptr.hpp>

#include "inmtypes.h"
#include "inmstatus.h"
#include "inmfilemgrtypes.h"
#include "inmfilemgrimpl.h"
#include "inmfilemgrimpl_noop.h"


class inm_filemgr_t
{

public:

    ///
    /// \brief constructor
    ///  instantiates the appropriate implementation class
    ///
    inm_filemgr_t(char const* name,                       ///< name of file to manage
        inm_filemgr_mode_t mode = FILEMGR_RW_ALWAYS,     ///< open options to use
        const unsigned long long & maxsize = 0,          ///< max size the file can grow
                                                         /// if zero, no limit is imposed at application level
                                                         /// limit is based on filesystem support
        const inm_allocation_algo_t & algo = ALLOC_ALGO_NONE ///< implementation algorithm 
        )
    {
        switch(algo)
        {
        case ALLOC_ALGO_NONE:
            m_impl.reset(new inm_filemgr_impl_noop_t(name, mode, maxsize));
            break;

        default:
            m_impl.reset(new inm_filemgr_impl_noop_t(name, mode, maxsize));
            break;
        };
    }

    ///
    /// \brief destructor
    ///   destroy the implementation instance
    ///
    ~inm_filemgr_t() 
    {
        m_impl.reset();
    }


    ///
    /// \brief  allocate the requested length bytes of storage from the file
    ///

    inm_status_t allocate(const inm_u32_t & length, ///< length to allocate
        inm_extents_t & extents                     ///> list of allocated extents
        ) 
    { 
        return m_impl -> allocate(length, extents); 
    }

    ///
    /// \brief allocate the requested length bytes as a single contigous allocation
    ///

    inm_status_t allocate_contigous(const inm_u32_t & length,
        inm_u64_t & offset)
    {
        return m_impl -> allocate_contigous(length, offset);
    }


    ///
    /// \brief deallocate previously allocated space
    ///

    inm_status_t deallocate(const inm_extent_t & space)
    {
        return m_impl -> deallocate(space);
    }


    ///
    /// \brief deallocate previously allocated space
    ///

    inm_status_t deallocate(const inm_extents_t & extents)
    {
        return m_impl -> deallocate(extents);
    }

    ///
    /// \brief return the list of free extents within the file
    ///

    inm_status_t get_free_extent_list(inm_extents_t & extents)
    {
        return m_impl -> get_free_extent_list(extents);
    }

protected:

private:

    inm_filemgr_t(const inm_filemgr_t&);
    inm_filemgr_t& operator=(const inm_filemgr_t&);

    boost::shared_ptr<inm_filemgr_impl_t> m_impl;
};


#endif