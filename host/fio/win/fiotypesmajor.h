
///
/// \file win/fiotypesmajor.h
///
/// \brief window specific types needed by Fio
///

#ifndef FIOTYPESMAJOR_H
#define FIOTYPESMAJOR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace FIO {

    long const FIO_SUCCESS(ERROR_SUCCESS);           ///< success code
    long const FIO_EOF(ERROR_HANDLE_EOF);            ///< eof code
    long const FIO_NOT_FOUND(ERROR_FILE_NOT_FOUND);  ///< not found code
    long const FIO_WOULD_BLOCK(ERROR_RETRY);         ///< would block try again (if desired)
    
    typedef long long offset_t; ///< IO offset    
    
    // postion to start seek at
    int const FIO_BEG(FILE_BEGIN);    ///< seek from begining
    int const FIO_CUR(FILE_CURRENT);  ///< seek from current postion
    int const FIO_END(FILE_END);      ///< seek from end

} // namespace FIO

#endif // FIOTYPESMAJOR_H
