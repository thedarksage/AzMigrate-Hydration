
///
/// \file inmfilemgrimpl_noop.h
///
/// \brief class inm_filemgr_impl_noop is basic implementation class for performing allocation and deallocation of storage in a disk file, 
///        much like a "freespace" manager. It does nothing as part of deallocation. 
///        allocations are performed at end of file.
/// 


#ifndef INMFILEMGRIMPL_NOOP__H
#define INMFILEMGRIMPL_NOOP__H

#include <boost/interprocess/sync/file_lock.hpp> 
#include <boost/thread/mutex.hpp>

#include "inmtypes.h"
#include "inmfilemgrtypes.h"
#include "inmfilemgrimpl.h"
#include "fio.h"
#include "extendedlengthpath.h"

class inm_filemgr_impl_noop_t : public inm_filemgr_impl_t
{

public:

    ///
    /// \brief
    ///

    inm_filemgr_impl_noop_t(char const* name,          ///< name of file to manage
        inm_filemgr_mode_t mode = FILEMGR_RW_ALWAYS,  ///< open options to use
        const unsigned long long & maxsize = 0       ///< max size the file can grow
        ):m_name(name), m_mode(mode), m_maxsize(maxsize)
    {

    }

    ///
    /// \brief
    ///

    virtual ~inm_filemgr_impl_noop_t()
    {

    }

    ///
    /// \brief allocate the requested length bytes of storage from the file
    ///

    virtual inm_status_t allocate(const inm_u32_t & length, ///< length to allocate
        inm_extents_t & extents                     ///> list of allocated extents
        )
    {
        inm_extent_t extent;
        inm_status_t rv = allocate_contigous(length,extent.offset);

        if(rv.succeeded())
        {
            extent.length = length;
            extents.clear();
            extents.push_back(extent);
        }

        return rv;
    }


    ///
    /// \brief allocate the requested length bytes as a single contigous allocation
    ///

    virtual inm_status_t allocate_contigous(const inm_u32_t & length,
        inm_u64_t & offset)
    {
        boost::mutex::scoped_lock lock(m_mutex);

        FIO::Fio fio;
        int open_mode = FIO::FIO_RW_ALWAYS;
        FIO::offset_t current_offset = 0;
        FIO::offset_t new_offset = 0;

        std::string lock_name = m_name + ".lck";
        if(!fio.open(ExtendedLengthPath::name(lock_name).c_str(), FIO::FIO_RW_ALWAYS))
        {
            return inm_status_t(INM_FAILURE, fio.error(), fio.errorAsString());
        }

        boost::interprocess::file_lock f_lock(lock_name.c_str());
        fio.close();


        if(FILEMGR_RW_ALWAYS == m_mode)
            open_mode = FIO::FIO_RW_ALWAYS;
        else
            open_mode = FIO::FIO_READ_OVERWRITE;


        if(!fio.open(ExtendedLengthPath::name(m_name).c_str(), m_mode))
        {
            return inm_status_t(INM_FAILURE, fio.error(), fio.errorAsString());
        }

        current_offset = fio.seek(0,FIO::FIO_END);
        if(current_offset < 0)
        {
            return inm_status_t(INM_FAILURE, fio.error(), fio.errorAsString());
        }

        if((current_offset + length) > m_maxsize)
        {
            return inm_status_t(INM_FAILURE, 0, "reached max size limit");
        }

        new_offset = fio.resize(current_offset + length);
        if(new_offset != current_offset)
        {
            return inm_status_t(INM_FAILURE, fio.error(), fio.errorAsString());
        }

        offset = current_offset;
        return inm_status_t(INM_SUCCESS);
    }

    ///
    /// \brief deallocate previously allocated space
    ///


    virtual inm_status_t deallocate(const inm_extent_t & space)
    {
        return inm_status_t(INM_SUCCESS);
    }

    ///
    /// \brief deallocate previously allocated space
    ///

    virtual inm_status_t deallocate(const inm_extents_t & space)
    {
        return inm_status_t(INM_SUCCESS);
    }

    ///
    /// \brief return the list of free extents within the file
    ///

    virtual inm_status_t get_free_extent_list(inm_extents_t & extents)
    {
        extents.clear();
        return inm_status_t(INM_SUCCESS);
    }

protected:

private:
    std::string m_name;
    inm_filemgr_mode_t m_mode;
    unsigned long long m_maxsize;
    boost::mutex m_mutex;
};



#endif
