
//
// main.cpp: unlockVolumes utility using ncurses and CDK (Curses Development Kit)
//

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "portablehelpers.h"
#include "volumegroupsettings.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "inmsafeint.h"
#include "inmsafecapis.h"

// NOTE: include this last as it has some #define names conflicts with some ace template variable names
// so as  long as this is last (more technically after all ace includes, everything is ok.
#include <cdk_test.h>


std::vector<std::string> getLockedVolumes()
{
    
  static std::vector<std::string> unlockedVolumes;
  //unlockedVolumes.clear();

  VolumeSummaries_t volumeSummaries;
  VolumeDynamicInfos_t volumeDynamicInfos;
  VolumeInfoCollector volumeCollector;
  volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos);
  VolumeReporter *pvr = new VolumeReporterImp();
  VolumeReporter::CollectedVolumes_t cvs;
  pvr->GetCollectedVolumes(volumeSummaries, cvs);
  delete pvr;
  
  for( std::set<std::string>::const_iterator i( cvs.m_Volumes.begin() ); i != cvs.m_Volumes.end(); ++i )
    {
      const std::string &vol = *i;
      VOLUME_STATE st = GetVolumeState(vol.c_str());
      if (st == VOLUME_HIDDEN)
	unlockedVolumes.push_back(vol);
    }
  return unlockedVolumes;
}

int UnlockVolumes(int argc, char **argv)
{
  /* Declare variables. */
  CDKSCREEN *cdkscreen = 0;
  CDKSELECTION *selection = 0;
  WINDOW *cursesWin = 0;
  char *title = "<C></B></U>Unlock Volumes<!U><!B>\n\n<C>Press SPACE to pick or unpick one or more volumes\n";
  char *choices[] = {"   ", "-->"};
  char **item = 0;
  char *mesg[200];
  unsigned used = 0;
  unsigned count = 0;
  
  /* Set up CDK. */
  cursesWin = initscr();
  cdkscreen = initCDKScreen (cursesWin);
  
  /* Set up CDK Colors. */
  initCDKColor();
  
  std::vector<std::string> drv = getLockedVolumes();
  if (drv.size() == 0) 
    {
      CDKDIALOG *question  = 0;
      char *buttons[]      = {"</B/24>Ok"};
      char *message[3];
      message[0]   = "<C></B></U>Unlock Volumes<!U><!B>";
      message[1]   = " ";
      message[2]   = "<C>There are no Target Volumes to be unlocked";
      question = newCDKDialog (cdkscreen,
			       CENTER,
			       CENTER,
			       message, 3, buttons, 1,
			       COLOR_PAIR(2)|A_REVERSE,
			       TRUE,
			       TRUE,
			       FALSE);
      if (question == 0)
	{
	  /* Shut down Cdk. */
	  destroyCDKScreen (cdkscreen);
	  endCDK();

	  /* Spit out a message. */
	  printf ("Unable to create the dialog box, window might be too small\n");
	  ExitProgram (EXIT_FAILURE);
	}
      int choice = activateCDKDialog (question, 0);
      destroyCDKDialog (question);
      destroyCDKScreen (cdkscreen);
      endCDK();
      ExitProgram (EXIT_SUCCESS);
    }
  
  for (int i=0;i<drv.size();i++) 
    {
      used = CDKallocStrings(&item, (char *) drv[i].c_str(), i, used);
    }	

   /* Create the selection list. */
  selection = newCDKSelection (cdkscreen,
			       CENTER,
			       CENTER,
			       RIGHT,
			       10, // Height
			       40, // Width
			       title,
			       item,
			       drv.size(),
			       choices, 2,
			       A_REVERSE,
			       TRUE,
			       FALSE);
  
  
  /* Is the selection list null? */
  if (selection == 0)
    {
      /* Exit CDK. */
      destroyCDKScreen (cdkscreen);
      endCDK();
      
      /* Print out a message and exit. */
      printf ("Unable to create the selection list, window might be too small\n");
      ExitProgram (EXIT_FAILURE);
    }	

  /* Activate the selection list. */
  activateCDKSelection (selection, 0);
  
  /* Check the exit status of the widget. */
  if (selection->exitType == vESCAPE_HIT)
    {	
      mesg[0] = "<C>Pressed escape, No items selected.";
      mesg[1] = "";
      mesg[2] = "<C>Press any key to continue.";
      popupLabel (cdkscreen, mesg, 3);
    }	
  else if (selection->exitType == vNORMAL)
    {
      mesg[0] = "<C>Following volumes are unlocked.";
      count = 1;
      bool notSelected = true;
      bool failedVolumes = false;
      bool addComma = false;
      std::string tempstr;
      SVERROR sve = SVS_OK;
      std::string errMessage = "<C>Outpost Agent was unable to unhide the following volumes:\n";
      for (int i=0; i < drv.size(); i++)
	{
	  if (selection->selections[i] == 1)
	    {
	      notSelected = false;
	      tempstr = std::string("<C></5>") + item[i];
          
          size_t buflen;
          INM_SAFE_ARITHMETIC(buflen = 1 + InmSafeInt<size_t>::Type(tempstr.size()), INMAGE_EX(tempstr.size()))
          
	      mesg[count] = new (std::nothrow)char[buflen];
          if(mesg[count])
          {
            inm_strcpy_s(mesg[count++], buflen, tempstr.c_str());    
          }
	      sve = UnhideDrive_RW(item[i],item[i]/*TODO:MPOINT*/);
	      if (sve.failed()) 
		{
		  failedVolumes = true;
		  if (addComma) {
		    errMessage += ", ";
		  }
		  else
		    {
		      addComma = true;
		    }
		  errMessage +=  drv[i].c_str();
		}

	    }
	}
      if (notSelected) 
	{
	  mesg[0] = "<C>No volumes selected to unlock.";
	  mesg[1] = "";
	  mesg[2] = "<C>Press any key to continue.";
	  count = 3;
	  popupLabel (cdkscreen, mesg, count);
	}
      else if (failedVolumes)
	{
	  mesg[0] = (char *) errMessage.c_str();
	  mesg[1] = "";
	  mesg[2] = "<C>Press any key to continue.";
	  count = 3;
	  popupLabel (cdkscreen, mesg, count);
	}
      else 
      {
	mesg[count] = "";
	mesg[count+1] = "<C>Press any key to continue.";
      	popupLabel (cdkscreen, mesg, count+2);
	for (int i = 1;i<count; i++)
		delete[] mesg[i];
      }
    }

  /* Clean up. */
  CDKfreeStrings (item);
  destroyCDKSelection (selection);
  destroyCDKScreen (cdkscreen);
  endCDK();
  ExitProgram (EXIT_SUCCESS);
}

int main (int argc, char **argv)
{	
  //return UnlockVolumes(argc, argv);
  return 0;
}

