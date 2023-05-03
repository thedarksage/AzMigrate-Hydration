//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : swapvolumes.h
// 
// Description: 
// 

#ifndef SWAPVOLUMES_H
#define SWAPVOLUMES_H

#include <sys/stat.h>
#include <sys/types.h>
#include <string>

#include "volumeinfocollectorinfo.h"

class SwapVolumes {
public:
    
    SwapVolumes();
    
    bool IsSwapVolume(VolumeInfoCollectorInfo const & volInfo) const;
    bool IsSwapFS(const std::string &fileSystem) const;
    
protected:        
    typedef std::set<dev_t> SwapVolumes_t;
    
    template <class STREAM> void ParseSwapVolumes(STREAM & swaps)
        {
            // assumes the format of the data in the stream "swaps" consists of
            // an intial header line followed by 0 or more lines. each of those
            // lines begins with the swap device name
            // 
            // e.g
            // linux looks like
            // Filename				Type		Size	Used	Priority
            // /dev/sda2                        partition	1020116	9324	42
            // 
            // sun looks like
            // swapfile             dev  swaplo blocks   free
            // /dev/dsk/c0t0d0s1   32,17     16 8386736 8386736
            // 
            // we only care about the device name not the other information


            std::string swapName;

            std::string line;

            std::getline(swaps, line); //skip the header line
            while (!swaps.eof()) {
                swaps >> swapName;
                if (!swapName.empty()) {
                    struct stat64 volStat;            
                    // make sure device is valid
                    if ((0 == stat64(swapName.c_str(), &volStat)) && volStat.st_rdev) 
                    {
                        m_SwapVolumes.insert(volStat.st_rdev);
                    }
                }
                else {
                    break;
                }
                std::getline(swaps, line);
                swapName.clear();
            }        
    
        }

    
private:    
    SwapVolumes_t m_SwapVolumes;
};



#endif // ifndef SWAPVOLUMES_H
