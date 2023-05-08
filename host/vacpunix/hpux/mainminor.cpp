#include <string.h>
#include <iostream>
#include <vector>
#include <set>
#include <string>


using namespace std;

//
// Function: GetFSTagSupportedVolumes
//   Gets the list of volumes on which file system consistency tag can be issued
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
//   TODO: we are not verifying the filesystem for now

bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes)
{
    bool rv  = true;
	return rv;

}

void FreezeDevices(const set<string> &inputVolumes)
{
}


void ThawDevices(const set<string> &inputVolumes)
{
}

//
// Function: ParseVolume
//   Go through the list of input volumes
//   if the input is a valid device file
//     accept it
//   else
//     if it is a valid mountpoint
//       get the corresponding device name
//       accept the device name as input volume
//     else
//       error
//     endif
//  endif
//
bool ParseVolume(set<string> & volumes, char *token)
{
	return true;
}
