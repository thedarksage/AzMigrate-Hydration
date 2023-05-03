/****************************************************************************
 ** ui.h extension file, included from the uic-generated form implementation.
 **
 ** If you want to add, delete, or rename functions or slots, use
 ** Qt Designer to update this file, preserving your code.
 **
 ** You should not define a constructor or destructor in this file.
 ** Instead, write your code in functions called init() and destroy().
 ** These will automatically be called by the form's constructor and
 ** destructor.
 *****************************************************************************/
#include <iostream>
#include <string>
#include <vector>
#include "hostagenthelpers_ported.h"
#include "volumegroupsettings.h"
#include "volumeinfocollector.h"

void UnlockDrives::unlock() 
{
  std::string errMessage =  "Outpost Agent was unable to unhide the following volumes\n";
  SVERROR sve = SVS_OK;
  bool failedDrives = false;
  bool   addComma = false;
  for (unsigned int i = 0;i< volumesListBox->count(); i++)  
    {
      if (volumesListBox->isSelected(i)) 
	{
	  sve = UnhideDrive_RW(volumesListBox->text(i).ascii());
	  if (sve.failed()) 
	    {
	      failedDrives = true;
	      if (addComma) 
		{
		  errMessage += ", ";        
		}
	      else
		{
		  addComma = true;
		}       
	      errMessage +=  volumesListBox->text(i).ascii();
	    }   
	}
    }
  if (failedDrives)
    {
      // to work on errMessage 
    }
  else
    close();
}

void UnlockDrives::init() 
{  
  volumesListBox->setSelectionMode(QListBox::Multi);
  std::vector<std::string> drv = UnlockDrives::getLockedDrives();
  for (unsigned int i = 0; i < drv.size(); i++)
    volumesListBox->insertItem( QString::fromLatin1 ( drv[i].c_str() ) , i );
  volumesListBox->selectAll (TRUE);
}

std::vector<std::string> UnlockDrives::getLockedDrives()
{
  std::vector<std::string> unlockedDrives;
  unlockedDrives.clear();

  VolumeSummaries_t volumeSummaries;
  VolumeInfoCollector volumeCollector;
  volumeCollector.GetVolumeInfos(volumeSummaries);

  for( VolumeSummaries_t::const_iterator i( vi.begin() ); i != vi.end(); ++i )
    {
      VOLUME_STATE st = GetVolumeState((*i).deviceName.c_str());
      if (st == VOLUME_HIDDEN)
	unlockedDrives.push_back((*i).devname.c_str());
    }
  return unlockedDrives;
}

bool UnlockDrives::noLockedDrives()
{
  if (getLockedDrives().size() == 0)
    return true;
  else
    return false;
}
