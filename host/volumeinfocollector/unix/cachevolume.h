//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : cachevolume.h
// 
// Description: 
// 

#ifndef CACHEVOLUME_H
#define CACHEVOLUME_H

class CacheVolume {
public:
    CacheVolume();
    
    bool IsCacheVolume(dev_t deviceId)
        {
            return m_DeviceId == deviceId;
        }
    
protected:

private:
    dev_t m_DeviceId;
};
    



#endif // ifndef CACHEVOLUME_H
