//#pragma once

#ifndef VOLUME_MANAGER__H
#define VOLUME_MANAGER__H


#include <set>
#include <string>

#include "cdpvolume.h"

class Lockable;

class VolumeManager :
	public Entity
{
public:
	static VolumeManager& GetInstance();
    static bool Destroy();
	cdp_volume_t::Ptr CreateVolume(const std::string &name,                       
                                   const int &devicetype,                         
								   const VolumeSummaries_t *pVolumeSummariesCache
								   );
	int Init();
private:
	//static auto_ptr<VolumeManager> theVolMan;
    static  Lockable m_CreateLock;
    static VolumeManager* theVolMan;
    
private:
	VolumeManager();
	~VolumeManager();

};

#endif

