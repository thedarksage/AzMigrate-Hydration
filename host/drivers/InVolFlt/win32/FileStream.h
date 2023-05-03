#ifndef _INMAGE_FILE_STREAM_H_
#define _INMAGE_FILE_STREAM_H_

class FileStream : public BaseClass {
public:
    void * __cdecl operator new(size_t size) {return malloc(size, TAG_FILESTREAM_OBJECT, NonPagedPool);};
    FileStream();
    virtual ~FileStream();
    virtual NTSTATUS Open(PUNICODE_STRING name, ULONG32 accessMode, 
                  ULONG32 createDisposition, ULONG32 createOptions, ULONG32 sharingMode, LONGLONG allocationSize);
    virtual NTSTATUS Close();
    virtual NTSTATUS SetFileSize(LONGLONG llFileSize);
    virtual NTSTATUS DeleteOnClose();
    NTSTATUS Size(LONGLONG * fileEof);
    PDEVICE_OBJECT GetDeviceObject(void) { return osDevice;};
    PFILE_OBJECT GetFileObject(void) { return fileObject;};
    HANDLE GetFileHandle(void) { return fileHandle;};
    bool IsOurWriteAtSizeAndOffset(PIRP Irp, ULONG32 size, ULONG64 offset);
    virtual BOOLEAN WasCreated(void) 
    { 
        return ((m_iosbCreateStatus.Status == STATUS_SUCCESS) && (m_iosbCreateStatus.Information == FILE_CREATED));
    } // return true if the open created a new file
protected:
    PDEVICE_OBJECT osDevice;
    PFILE_OBJECT fileObject;
    HANDLE  fileHandle;
    IO_STATUS_BLOCK m_iosbCreateStatus;
};

#endif  // _INMAGE_FILE_STREAM_H_
