// This class is responsible for interfacing with the target test volume
#pragma once

class TargetIO
{
public:
    TargetIO();
    void SetPath(char * path);
    DWORD OpenRemote(char * path);
    BOOLEAN VerifyIsRAWVolume();
    DWORD Open();
    DWORD Close();
    DWORD Write(__int64 offset, int size, void * buffer);
    DWORD Read(__int64 offset, int size, void * buffer, int * bytesRead);
    DWORD GetDirtyBlocks(PDIRTY_BLOCKS dirtyBlocks);
    
    DWORD GetDirtyBlocksFromRemote(PDIRTY_BLOCKS dirtyBlocks);
    DWORD GetVolumeSize(LONGLONG * volumeSize);
    DWORD OpenShutdownNotify();
    DWORD CloseShutdownNotify();
protected:
    HANDLE handle;
    HANDLE shutdownHandle;
    HANDLE pipeHandle;
    OVERLAPPED overlapped;
    OVERLAPPED shutdownOverlapped;
    char pathname[1024];
};
