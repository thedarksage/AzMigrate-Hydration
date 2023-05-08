
///
/// \file fiotypes.h
///
/// \brief options used when opening a file

// TODO:
// * need to allow users to specify file attributes, permissions/security when creating a file
// * support more options as needed

#ifndef FIOTYPES_H
#define FIOTYPES_H

#include "fiotypesmajor.h"

namespace FIO {

    /// \brief test if a specific mode is set in a given mode
#define FIO_MODE_ON(fioChkMode, fioCurMode) ((fioChkMode) == ((fioChkMode) & (fioCurMode)))

    // access (use FIO_READ | FIO_WRITE for read and write acesss)
    int const FIO_READ(0x1);    ///< read only (windows: GENERIC_READ, unix: O_RDONLY (O_RDWR if FIO_WRITE specified))
    int const FIO_WRITE(0x2);   ///< write only (windows: GENERIC_WRITE, unix: O_WRONLY (O_RDWR if FIO_READ specified))

    // optional access
    int const FIO_APPEND(0x4);  ///< all writes done at end of file (windows: all settings in GENERIC_WRITE without FILE_WRITE_DATA, unix: O_APPEND)

    // open/create modes (combine open and create to open if existing else and create new)
    int const FIO_OPEN(0x10);       ///< open if exists else fail. (windows: OPEN_EXISTING, unix: no option needed). Combine with FIO_CREATE to always open
    int const FIO_CREATE(0x20);     ///< create new file, error if exists. (windows: CREATE_NEW, unix: O_CREATE | O_EXCL). Combine with FIO_OPEN to always open

    // optional open/create 
    int const FIO_TRUNCATE(0x40);   ///< truncate existing file. (windows: CREATE_ALWAYS if FIO_OPEN and FIO_CREATE, TRUNCATE_EXISTING if only FIO_OPEN, unix: O_TRUNC)

    // share
    int const FIO_SHARE_DELETE(0x100);   ///< allow shared delete (windows: FILE_SHARE_DELETE, unix: none same as FIO_SHARE_ALL)
    int const FIO_SHARE_READ(0x200);     ///< allow shared read. Enabled by default (windows: FILE_SHARE_READ,  unix: none same as FIO_SHARE_ALL)
    int const FIO_SHARE_WRITE(0x400);    ///< allow shared write. enabled by default (windows: FILE_SHARE_WRITE, unix: none same as FIO_SHARE_ALL)
    int const FIO_SHARE_NONE(0x800);     ///< open exclusive (windows: none, unix: O_EXCL)

    // buffering and directy writing (may be useful on other OSes)
    int const FIO_NO_BUFFERING(0x1000);   ///< no buffering (windows: FILE_FLAG_NO_BUFFERING, unix: O_DIRECT)
    int const FIO_WRITE_THROUGH(0x2000);  ///< enable direct write to disk (windows: FILE_FLAG_WRITE_THROUGH, unix: O_DIRECT)
    int const FIO_NONBLOCK(0x4000);       ///< non blocking io (windows: FILE_FLAG_OVERLAPPED, unix: O_NONBLOCK)

    // misc
    int const FIO_LARGEFILE(0x10000);  ///< largefile support (windows: none (large files are supported), unix: O_LARGEFILE)
    int const FIO_NOATIME(0x20000);    ///< turn off atime updates (wdinows: uses SetFileTime to disable, unix: O_NOATIME);

    int const FIO_ERROR_STRING_MAX_LEN(1024); ///< max length an FIO error string can be

    // convenience combinations of access and open/create options
    int const FIO_READ_EXISTING(FIO_OPEN | FIO_READ);                                           ///< open existing file for reading
    int const FIO_WRITE_EXISTING(FIO_OPEN | FIO_WRITE);                                         ///< open existing file for writing
    int const FIO_RW_EXISTING(FIO_OPEN | FIO_READ| FIO_WRITE);                                  ///< open existing file for both reading and writing
    int const FIO_WRITE_NEW(FIO_CREATE | FIO_WRITE);                                            ///< create new file for writing
    int const FIO_RW_NEW(FIO_CREATE | FIO_READ | FIO_WRITE);                                    ///< create new file for both reading and writing 
    int const FIO_WRITE_ALWAYS(FIO_CREATE | FIO_WRITE | FIO_OPEN);                              ///< opening existing or create new file for writing
    int const FIO_RW_ALWAYS(FIO_OPEN | FIO_CREATE | FIO_READ | FIO_WRITE);                      ///< opening existing or create new file for both reading and writing
    int const FIO_WRITE_TRUNCATE(FIO_OPEN | FIO_TRUNCATE | FIO_WRITE);                          ///< open and truncate existing file for writing
    int const FIO_RW_TRUNCATE(FIO_OPEN | FIO_TRUNCATE | FIO_READ | FIO_WRITE);                  ///< open and truncate existing file for both reading and writing
    int const FIO_OVERWRITE(FIO_OPEN | FIO_CREATE | FIO_TRUNCATE | FIO_WRITE);                  ///< open and truncate existing or create new file for writing
    int const FIO_READ_OVERWRITE(FIO_OPEN | FIO_CREATE | FIO_TRUNCATE | FIO_READ | FIO_WRITE);  ///< open and truncate existing or create new file for both reading and writing
    

} // namespace FIO

#endif // FIOTYPES_H
