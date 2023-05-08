#ifndef VIRTUAL_VOLUME_WIN_H
#define VIRTUAL_VOLUME_WIN_H


#if defined(UNICODE)
#undef UNICODE
#endif

class VirVolumeWin
{

public:
BOOLEAN FileDiskUmount(PTCHAR);
VOID DeleteAutoAssignedDriveLetter(HANDLE hDevice, PTCHAR MountPoint);
};

#endif