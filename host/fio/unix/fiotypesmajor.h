
///
/// \file unix/fiotypesmajor.h
///
/// \brief UNIX speific types needed by Fio
///

#ifndef FIOTYPESMAJOR_H
#define FIOTYPESMAJOR_H

#include <cstdio>
#include <cerrno>

namespace FIO {

    unsigned long const FIO_SUCCESS(0);          ///< success code
    unsigned long const FIO_EOF((unsigned long)~0);             ///< eof code
    unsigned long const FIO_NOT_FOUND(ENOENT);   ///< not found code
    unsigned long const FIO_WOULD_BLOCK(EAGAIN); ///< would block try again (if desired)

#ifdef off64_t
    typedef off64_t offset_t; ///< IO offset
#else
    typedef off_t offset_t;
#endif
    
    // postion to start seek at
    int const FIO_BEG(SEEK_SET);   ///< seek from begining
    int const FIO_CUR(SEEK_CUR);   ///< seek from current postion
    int const FIO_END(SEEK_END);   ///< seek from end

} // namespace FIO


#endif // FIOTYPESMAJOR_H
