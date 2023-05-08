#include <iostream>
#include "simplevolumescopierapp.h"


SimpleVolumesCopierApp::SimpleVolumesCopierApp()
: m_QuitEvent(false, false),
  m_BytesCovered(0),
  m_FilesystemUnusedBytes(0)
{
}


SimpleVolumesCopierApp::~SimpleVolumesCopierApp()
{
}


bool SimpleVolumesCopierApp::ActionOnNoBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
	m_BytesCovered = bytescovered.m_BytesCovered;
	m_FilesystemUnusedBytes = bytescovered.m_FilesystemUnusedBytes;
    std::cout << "starting copy...\n";

    return true;
}


bool SimpleVolumesCopierApp::ActionOnBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
	m_BytesCovered = bytescovered.m_BytesCovered;
	m_FilesystemUnusedBytes = bytescovered.m_FilesystemUnusedBytes;
	std::cout << "=============================\n";
    std::cout << "bytes covered: " << m_BytesCovered << '\n';
	std::cout << "file system unused bytes: " << m_FilesystemUnusedBytes << '\n';
	std::cout << "=============================\n";

    return true;
}


bool SimpleVolumesCopierApp::ActionOnAllBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
	m_BytesCovered = bytescovered.m_BytesCovered;
	m_FilesystemUnusedBytes = bytescovered.m_FilesystemUnusedBytes;
    std::cout << "copy completed\n";
    SignalQuit();

    return true;
}


void SimpleVolumesCopierApp::SignalQuit(void)
{
    m_QuitEvent.Signal(true);
}


bool SimpleVolumesCopierApp::ShouldQuit(int secs)
{
    return m_QuitEvent(secs);
}


SV_ULONGLONG SimpleVolumesCopierApp::GetBytesCovered(void)
{
    return m_BytesCovered;
}


SV_ULONGLONG SimpleVolumesCopierApp::GetFilesystemUnusedBytes(void)
{
    return m_FilesystemUnusedBytes;
}