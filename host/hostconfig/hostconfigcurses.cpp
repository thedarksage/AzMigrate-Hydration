//
// hostconfigcurses.cpp: Hostconfig utility using ncurses and CDK (Curses Development Kit)
//

#include "localconfigurator.h"
#include "svutils.h"
#include "logger.h"
#include "svconfig.h"

#include "csgetfingerprint.h"

#include "configurator2.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <cdk_test.h>
#include <string>
#include <sstream>
#include <iostream>
#define MIN_INTERNET_PORT 1
#define MAX_INTERNET_PORT 65535
#ifdef SV_UNIX
#include "ace/OS_NS_unistd.h"
#include "ace/OS_main.h"
#include "ace/Service_Config.h"
#include "ace/Thread_Manager.h"
#include "ace/Process.h"
#include "ace/Process_Manager.h"
#include "ace/Get_Opt.h"
#include "branding_parameters.h"
#endif
#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>

Configurator* TheConfigurator = 0; // singleton

// 127 is the scancode of BACKSPACE on RHEL
#define CODE_BACKSPACE 127
#define TMPPATH "/usr/tmp"

#define DIR_NAME "/usr/local/"
#define FX_FILENAME ".fx_version"
#define VX_FILENAME ".vx_version"

enum { SCOUT_FX = 1, SCOUT_VX = 2, SCOUT_BOTH = 3 };

const int DEFAULT_HTTPS_PORT = 443;
const int DEFAULT_HTTP_PORT = 80;
const int  HTTPS_PROTOCOL_VAL = 1;
const int MODIFY_PASSPRASE_VAL = 0;

std::string configFilePath;

using namespace std;

void RestartServices( std::string installPath);

static void drawCDKEntryField (CDKENTRY *entry)
{
   int infoLength = 0;
   int x = 0;

   /* Draw in the filler characters. */
   mvwhline (entry->fieldWin, 0, x, entry->filler, entry->fieldWidth);

   /* If there is information in the field. Then draw it in. */
   if (entry->info != 0)
   {
      infoLength = (int)strlen (entry->info);

      /* Redraw the field. */
      if (isHiddenDisplayType (entry->dispType))
      {
         for (x = entry->leftChar; x < infoLength; x++)
         {
            mvwaddch (entry->fieldWin, 0, x - entry->leftChar, entry->hidden);
         }
      }
      else
      {
         for (x = entry->leftChar; x < infoLength; x++)
         {
            mvwaddch (entry->fieldWin, 0, x - entry->leftChar,
                      ((char) (entry->info[x])) | entry->fieldAttr);
                      //CharOf (entry->info[x]) | entry->fieldAttr);
         }
      }
      wmove (entry->fieldWin, 0, entry->screenCol);
   }

   wrefresh (entry->fieldWin);
}
static void CDKEntryBackSpaceCB (CDKENTRY *entry, chtype character)
{
	if (character == CODE_BACKSPACE)
		return;	
   int plainchar = filterByDisplayType (entry->dispType, character);
   unsigned temp;

   if (plainchar == ERR ||
       ((int)strlen (entry->info) >= entry->max))
   {
#ifdef SV_UNIX
      Beep ();
#else
	   Beep(100, 100);
#endif
   }
   else
   {
      /* Update the screen and pointer. */
      if (entry->screenCol != entry->fieldWidth - 1)
      {
         int x;

         for (x = (int)strlen (entry->info);
              x > (entry->screenCol + entry->leftChar);
              x--)
         {
            entry->info[x] = entry->info[x - 1];
         }
         entry->info[entry->screenCol + entry->leftChar] = plainchar;
         entry->screenCol++;
      }
      else
      {
         /* Update the character pointer. */
         temp = strlen (entry->info);
         entry->info[temp] = plainchar;
         entry->info[temp + 1] = '\0';
         /* Do not update the pointer if it's the last character */
         if ((int)(temp + 1) < entry->max)
            entry->leftChar++;
      }

      /* Update the entry field. */
      drawCDKEntryField (entry);
   }
}
static int onBackSpaceCB (EObjectType cdktype GCC_UNUSED,
			 void *object,
			 void *clientData GCC_UNUSED,
			 chtype key GCC_UNUSED)
{	
	CDKENTRY *entry  = (CDKENTRY *) object;
	injectCDKEntry(entry, KEY_BACKSPACE);
	return 0;
}
bool validateip(std::string ip)
{
	unsigned int a, b, c, d;
	stringstream tmpstr;
	if (ip.length() > 15)
		return false;
	int rc = sscanf(ip.c_str(), "%3d.%3d.%3d.%3d", &a, &b, &c, &d);
	tmpstr << a << "." << b << "." << c << "." << d ;
	if (tmpstr.str() != ip)
		return false;
	if (rc != 4)
		return false;
	if (a > 255 || b > 255 || c > 255 || d > 255)
		return false;
	return true;
}
bool validateport(std::string port)
{
	unsigned int portval;
	stringstream tmpstr;
	int rc = sscanf(port.c_str(), "%d", &portval);
	tmpstr << portval ;
	if (tmpstr.str() != port)
		return false;
	if (rc != 1)
		return false;
	if (MIN_INTERNET_PORT > portval)
		return false;
	if (MAX_INTERNET_PORT < portval)
		return false;
	return true;
}

bool validatepassphrase( int httpsItemval, std::string ipAddress, std::string port, std::string hostId, std::string passphrase, std::string &err_msg, bool & isVerifiedCreatedpassphrase ) 
{
	bool rc;
	std::string reply;
        std::string user;
        bool verifyPassphraeOnly = false;
        bool useSsl;
        bool overwrite = true;	

	if ( passphrase.empty() ) {
		err_msg = "Passphrase should not be empty";
		return false;
	}

	if ( passphrase.length() < 16 ) {
		err_msg = "Passphrase length should be atleast 16 characters";
		return false;
	}
	
	if ( httpsItemval )
		useSsl = true;
	else {
		useSsl = false;
		verifyPassphraeOnly = true;
	}

	rc = securitylib::csGetFingerprint(reply, hostId, passphrase, ipAddress, port, verifyPassphraeOnly, useSsl, overwrite); 

	err_msg = reply;
	isVerifiedCreatedpassphrase = true;

	return rc;
}
SVERROR getContentsOfFile(const std::string &strFileName, std::string& strOutPut)
{
  SVERROR bRet = SVS_FALSE;
  ACE_HANDLE handle = ACE_INVALID_HANDLE;
  SVERROR status = SVS_OK;
  std::ifstream is;
  SV_ULONG fileSize = 0;
  is.open(strFileName.c_str(), std::ios::in | std::ios::binary);
  if (is.good())
  {
      is.seekg(0, std::ios::end);
      fileSize = is.tellg();
      is.seekg(0, std::ios::beg);
      is.close();
  }
  if (fileSize != 0)
  {
      handle = ACE_OS::open(strFileName.c_str(), O_RDONLY);
      if (handle == ACE_INVALID_HANDLE)
      {
          strOutPut = std::string("Failed to open file %s for reading .", strFileName.c_str());
      }
      else
      {
          size_t infolen;
          INM_SAFE_ARITHMETIC(infolen = InmSafeInt<SV_ULONG>::Type(fileSize) + 1, INMAGE_EX(fileSize))
          boost::shared_array<char> info(new (std::nothrow) char[infolen]);
          if (fileSize == ACE_OS::read(handle, info.get(), fileSize))
          {
              info[fileSize] = '\0';
              strOutPut = info.get();
              bRet = SVS_OK;
          }
          else
          {
              DebugPrintf(SV_LOG_ERROR, "Failed to read the entire  file %s. Requested Read size %d\n", strFileName.c_str(), fileSize);
              strOutPut = std::string("Failed to read entire file : %s", strFileName.c_str());
          }
          ACE_OS::close(handle);
      }
  }
  else
  {
     DebugPrintf(SV_LOG_DEBUG, "The file %s's size is empty...\n", strFileName.c_str());
     bRet = SVS_OK;
  }
  DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", __FUNCTION__);
  return bRet;
}
bool execProc(const std::string& tmppath, const std::string& cmdLine, std::string& err_msg)
{
        bool rv = true;
        bool isExecuteFine = true;
        std::string tmpFile = tmppath;
        tmpFile += ACE_DIRECTORY_SEPARATOR_CHAR;
        tmpFile += "tmpLog.log";

        ACE_HANDLE handle = ACE_OS::open(getLongPathName(tmpFile.c_str()).c_str(), O_RDWR | O_CREAT | O_TRUNC);
        if (handle == ACE_INVALID_HANDLE)
        {
            err_msg = "failed to create tmp file " + tmpFile;
            return false;
        }
        ACE_Time_Value waitTime(ACE_OS::gettimeofday());
        pid_t pid, rc;
        ACE_Process_Options opt;
        opt.command_line("%s", cmdLine.c_str());
        if (opt.set_handles(ACE_STDIN, handle, handle) == -1)
        {
            err_msg = "failed in set_handles for STDOUT AND STDERR, error :" + boost::lexical_cast<std::string>(ACE_OS::last_error());
            isExecuteFine = false;
        }
        else
        {
            ACE_Process_Manager* proc = ACE_Process_Manager::instance();
            if ((pid = proc->spawn(opt)) == ACE_INVALID_PID)
            {
               err_msg = "cxpscli spawned failed, error: " + boost::lexical_cast<std::string>(ACE_OS::last_error());
               isExecuteFine = false;
            }
            else
            {
               ACE_exitcode exitcode = 0;
               waitTime.sec(waitTime.sec() + 5);
               rc = ACE_Process_Manager::instance()->wait(pid, waitTime, &exitcode);
               if (ACE_INVALID_PID == rc) {
                 err_msg = "script failed in ACE_Process_Manager::wait with error no: " + boost::lexical_cast<std::string>(ACE_OS::last_error());
                 isExecuteFine = false;
                 }
            }
            opt.release_handles();
        }
        ACE_OS::close(handle);
        if (isExecuteFine)
        {
           std::string contents;
           SVERROR gotContent = getContentsOfFile(tmpFile, contents);
           if (gotContent == SVS_OK)
           {
              if (!contents.empty())
              {
                 err_msg = contents;
                 rv = false;
              }
              else
              {
                 //DebugPrintf(SV_LOG_DEBUG, "tmp error log file is empty. \n");
              }
           }
           else
           {
                err_msg = contents;
                rv = false;
           }
        }
        else
        {
          rv = false;
        }
        ACE_OS::unlink(getLongPathName(tmpFile.c_str()).c_str()) ;
        return rv;
}
void MessageBox(CDKSCREEN *cdkscreen, char *mesg)
{
  CDKDIALOG *question  = 0;
  char *buttons[]      = {"</B/24>Ok"};
  char *message[2];
  message[0]   = "<C></B></U>Host Config<!U><!B>";
  message[1]   = mesg;
  
  question = newCDKDialog (cdkscreen,
			   CENTER,
			   0, 
			   message, 2, buttons, 1,
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
      DebugPrintf ("Unable to create the dialog box, window might be too small\n");
      ExitProgram (EXIT_FAILURE);
    }
  int choice = activateCDKDialog (question, 0);
  destroyCDKDialog (question);
}

void PassphraseMessageBox(CDKSCREEN *cdkscreen, char *mesg) 
{
  CDKDIALOG *question  = 0;
  char *buttons[]      = {"</B/24>Ok"};
  char *message[6];
  message[0]   = "<C></B></U>Host Config<!U><!B>";
  message[1]   = mesg;
  message[2]   = "If you forgot passphrase, follow the steps below to get the passphrase";
  message[3]   = "Login to CS and open the command prompt";
  message[4]   = "Navigate to CS installation path(Eg: C:\\home\\svsystems\\bin)";
  message[5]   = "Execute the command \"genpassphrase.exe -n"; 

  question = newCDKDialog (cdkscreen,
			   CENTER,
			   TOP,
			   message, 6, buttons, 1,
			   COLOR_PAIR(2)|A_REVERSE,
			   TRUE,
			   TRUE,
			   FALSE);	
  if (question == 0)
  {
	destroyCDKScreen (cdkscreen);
	endCDK();
	DebugPrintf ("Unable to create the dialog box, window might be too small\n");
	ExitProgram (EXIT_FAILURE);
  }
  int choice = activateCDKDialog (question, 0);
  destroyCDKDialog (question);
}

static int dialogHelpCB (EObjectType cdktype GCC_UNUSED,
			 void *object,
			 void *clientData GCC_UNUSED,
			 chtype key GCC_UNUSED)
{
  CDKDIALOG *dialog = (CDKDIALOG *) object;
  char *mesg[5];	
  
  /* Check which button we are on. */
  if (dialog->currentButton == 0)
    {
      mesg[0] = "<C></U>Help for </U>Gloabal<!U>.";
      mesg[1] = "";
      if ("CX" == SERVER_NAME )
        mesg[2] = "<C>CX server IP address";
      else if ("TS" == SERVER_NAME )
        mesg[2] = "<C>TS server IP address";
      else
        mesg[2] = "<C>CX server IP address";
      mesg[3] = "<C>and port number, https, Passphrase need to be entered here.";
      popupLabel (ScreenOf (dialog), mesg, 4);
   }
  else	if (dialog->currentButton == 1)
    {	
      mesg[0] = "<C></U>Help for </U>Agent<!U>.";
      mesg[1] = "<C>When Agent button is picked, all the inputs/options";
      mesg[2] = "<C>related to agent side need to be entered.";
      popupLabel (ScreenOf (dialog), mesg, 3);
    }
  else if (dialog->currentButton == 2)
    {
      mesg[0] = "<C></U>Help for </U>NAT<!U>.";
      mesg[1] = "<C>When NAT button is picked, NAT related";
      mesg[2] = "<C>flags and NAT Hostname/IP need to be set.";
      popupLabel (ScreenOf (dialog), mesg, 3);
   }	
  else if (dialog->currentButton == 3)
    {
      mesg[0] = "<C></U>Help for </U>Logging<!U>.";
      mesg[1] = "<C>When Logging button is picked,  logging related";
      mesg[2] = "<C>options need to be entered.";
      popupLabel (ScreenOf (dialog), mesg, 3);
   }
  else if (dialog->currentButton == 4)
    {
      mesg[0] = "<C></U>Help for </U>Quit<!U>.";
      mesg[1] = "<C>When this button is picked the dialog box is exited.";
      popupLabel (ScreenOf (dialog), mesg, 2);
   }
  return (FALSE);	
}

int GetScoutAgentInstallType() 
{
    int scoutAgentInstallCode = 0;

    ACE_DIR *Dir = sv_opendir(DIR_NAME);
    if(Dir == NULL)
    {
        fprintf(stderr, "unable to open directory %s, error no: %d\n", DIR_NAME, ACE_OS::last_error());
        return scoutAgentInstallCode;
    }

    struct ACE_DIRENT *dEnt = NULL;
    while((dEnt = ACE_OS::readdir(Dir)) != NULL)
    {
        std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
        if(!stricmp(dName.c_str(), FX_FILENAME))
        {
            scoutAgentInstallCode += SCOUT_FX;
	}
	
        if(!stricmp(dName.c_str(), VX_FILENAME))
        {
            scoutAgentInstallCode += SCOUT_VX;
        }
    }
    return scoutAgentInstallCode;
}

int interface(int argc, char **argv)
{
   /* Declare variables. */
  CDKSCREEN	*cdkscreen;
  CDKDIALOG	*question;
  WINDOW	*cursesWin;
  char		*buttons[40];
  char		*message[40];
  int		selection;
  int 		scoutAgentInstallCode = 0;

  scoutAgentInstallCode = GetScoutAgentInstallType();
  if((scoutAgentInstallCode != SCOUT_FX) && (scoutAgentInstallCode != SCOUT_VX) && (scoutAgentInstallCode != SCOUT_BOTH))
  {
    fprintf(stderr, "Scout Agent is not installed\n" );
    ExitProgram (EXIT_FAILURE);
  }
  

try
{
  LocalConfigurator config;

  SetLogLevel(SV_LOG_DISABLE);
  
  //CDKparseParams(argc, argv, &params, CDK_MIN_PARAMS);


  /* Set up CDK. */
  cursesWin = initscr ();
  cdkscreen = initCDKScreen (cursesWin);
  
  /* Start color. */
  initCDKColor ();
  
  /* Set up the dialog box. */
  message[0] = "<C></U></32>Host Config Interface<!32>";
  message[1] = "Pick the command you wish to run.";
  message[2] = "<C>Press </R>?<!R> for help.";
  buttons[0] = "Global";
  buttons[1] = "Agent";
  buttons[2] = "NAT";
  buttons[3] = "Logging";
  buttons[4] = "Quit";
  
  /* Create the dialog box. */
  question = newCDKDialog (cdkscreen,
			   CENTER, TOP,
			   message, 3 /* no fo mesgs */, buttons, 5, A_REVERSE,
			   TRUE,
			   TRUE,
			   FALSE);
  
  /* Check if we got a null value back. */
  if (question == (CDKDIALOG *) 0)
    {
      destroyCDKScreen (cdkscreen);
      
      /* End curses... */
      endCDK ();
      
      /* Spit out a message. */
      DebugPrintf (SV_LOG_DEBUG, "Unable to create the dialog box. ");
      DebugPrintf (SV_LOG_DEBUG, "Window too small...\n");
      ExitProgram (EXIT_FAILURE);	
   }
  
  /* Create the key binding. */
  bindCDKObject (vDIALOG, question, '?', dialogHelpCB, 0);
  
  /* Get the config file path for Fx. */
  std::string FilePath = config.getInstallPath();
  std::string::size_type pos = FilePath.rfind("Vx");
  configFilePath = FilePath.substr(0, pos);
  configFilePath += "Fx";
  configFilePath += ACE_DIRECTORY_SEPARATOR_CHAR;
  configFilePath += "config.ini";

  /* Activate the dialog box. */
  selection = 0;
  bool isPassphraseValidated = false;     //This flag will be set to true if the passphrase validation is successful 
  bool isPassphraseNotModified = false;   //This flag will be set to true if the passphrase is not modified
  bool bNeedToQuit = false; 		  //This flag will be set to true if user wants to quit without editing any option/widgets
  while ( selection != 4 || (isPassphraseValidated == true) )
    {
      if ( selection == 4 && isPassphraseNotModified )
          break;

      if( selection == 4 && !bNeedToQuit)
          break;

      /* Get the users button selection. */
      selection = activateCDKDialog (question, (chtype *) 0);
      
      /* Check the results. */
      if (selection == 0)
	{
	  
	  int portVal = DEFAULT_HTTPS_PORT;
	  int protocolVal = HTTPS_PROTOCOL_VAL;
	  std::string portvalueFx;
	  std::string protocolvalueFx;
	  std::string ipaddressvalueFx;
	  if(argc > 1 && !strcasecmp(argv[1],"--enablehttps"))
	  {
		portVal = DEFAULT_HTTPS_PORT;
		protocolVal = HTTPS_PROTOCOL_VAL;
	  }
	  else
	  {
		portVal = config.getHttp().port;
		protocolVal = config.IsHttps();
	  }
          bNeedToQuit = true;

	  CDKLABEL     *demo;
	  CDKENTRY *ipAddressEntry  = 0;
	  CDKENTRY *portEntry = 0;
	  CDKENTRY *passphraseEntry = 0;
	  CDKITEMLIST *Modify_passphraseItem;
	  int modifyVal = MODIFY_PASSPRASE_VAL;
	  bool Passphrase_FileExist = false;
	  std::string newIpAddress;
	  std::string newPort;
	  std::string newPassphrase;
	  int newHttps;
	  char         *mesg[1];
	  char *info[2];
 	  info[0]      = "<C></5>No";
          info[1]      = "<C></5>Yes";
	  CDKITEMLIST *httpsItem;
          if ("CX" == SERVER_NAME )
             mesg[0] = "</B></U>CX Server settings<!U><!B>";
          else if ("TS" == SERVER_NAME )
             mesg[0] = "</B></U>TS Server settings<!U><!B>";
          else
             mesg[0] = "</B></U>CX Server settings<!U><!B>";
	  demo = newCDKLabel (cdkscreen,
			      getbegx (question->win), getbegy (question->win) + question->boxHeight,	
			      mesg, 
			      1,
			      FALSE, 
			      FALSE);
	  if (demo == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      fprintf(stderr, "Cannot create label-widget: demo\n");
	      ExitProgram (EXIT_FAILURE);
	    }

	  ipAddressEntry  = newCDKEntry (cdkscreen, getbegx (demo->win), getbegy (demo->win) + demo->boxHeight,
					 "<C>Enter FQDN or IP Address", "FQDN or IP: ", A_NORMAL, '_', vMIXED,
					 20, 0, 255, TRUE, FALSE);
	  portEntry = newCDKEntry (cdkscreen, getbegx (ipAddressEntry->win), getbegy (ipAddressEntry->win) + ipAddressEntry->boxHeight  ,
				   "<C>Enter Port number", "Port: ", A_NORMAL, '_', vMIXED,
				   10, 0, 5, TRUE, FALSE);
	  httpsItem = newCDKItemlist (cdkscreen,
                                            getbegx (portEntry->win), getbegy (portEntry->win) + portEntry->boxHeight,
                                            "Use https: ",
                                            NULL,
                                            info,
                                            2,
                                            protocolVal,
                                            TRUE,
                                            FALSE);

	 if ( securitylib::isPassphraseFileExists() )
	 {
		passphraseEntry = newCDKEntry (cdkscreen, getbegx (httpsItem->win), getbegy (httpsItem->win) + httpsItem->boxHeight, "<C>Existing Passphrase", "Passphrase: ", A_NORMAL, '_', vHMIXED, 20, 0, 256, TRUE, FALSE);
		Modify_passphraseItem = newCDKItemlist (cdkscreen, getbegx (passphraseEntry->win), getbegy (passphraseEntry->win) + passphraseEntry->boxHeight, "Modify Passphrase:", NULL, info, 2, modifyVal, TRUE, FALSE); 
		Passphrase_FileExist = true;		
	 }
	 else 
	 { 
		passphraseEntry = newCDKEntry (cdkscreen, getbegx (httpsItem->win), getbegy (httpsItem->win) + httpsItem->boxHeight, "<C>Enter Passphrase", "Passphrase: ", A_NORMAL, '_', vHMIXED, 20, 0, 256, TRUE, FALSE);
	 }
 
	  if (ipAddressEntry == 0  || portEntry == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create entry-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }

	  if (httpsItem == 0)
          {
              destroyCDKScreen (cdkscreen);
              endCDK();

              fprintf(stderr, "Cannot create httpsItem-widget\n");
              ExitProgram (EXIT_FAILURE);
          }
	
	if (passphraseEntry == 0 )
	{
	    destroyCDKScreen (cdkscreen);
	    endCDK();

	    fprintf(stderr, "Cannont create passphrase-widget\n");
	    ExitProgram (EXIT_FAILURE);
	}
	
        if (  Passphrase_FileExist && Modify_passphraseItem == 0 )
	{
		destroyCDKScreen (cdkscreen);
		endCDK();

		fprintf(stderr, "Cannont create  Modify_passphraseItem-widget\n");
		ExitProgram (EXIT_FAILURE);
        }
	  
  	  bindCDKObject (vENTRY, ipAddressEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(ipAddressEntry, CDKEntryBackSpaceCB);

  	  bindCDKObject (vENTRY, portEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(portEntry, CDKEntryBackSpaceCB);

	  setCDKEntryValue(ipAddressEntry, config.getHttp().ipAddress);

	  bindCDKObject (vENTRY, passphraseEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(passphraseEntry, CDKEntryBackSpaceCB);
  
	  stringstream portstr;
	  portstr << portVal;
	  setCDKEntryValue(portEntry, (char *)portstr.str().c_str());
	  drawCDKLabel(demo, ObjOf(demo)->box);
	  drawCDKEntry (ipAddressEntry,    ObjOf(ipAddressEntry)->box);
	  drawCDKEntry (portEntry,    ObjOf(portEntry)->box);
	  drawCDKItemlist(httpsItem, ObjOf(httpsItem)->box);
	
	  std::string passphrase;
	  if ( Passphrase_FileExist )
	  {
		passphrase = securitylib::getPassphrase();
		setCDKEntryValue(passphraseEntry, passphrase.c_str());
          } 

	  drawCDKEntry (passphraseEntry, ObjOf(passphraseEntry)->box);
	

	  if ( Passphrase_FileExist ) {
		drawCDKItemlist(Modify_passphraseItem, ObjOf(Modify_passphraseItem)->box);
	  } 
	
	  activateCDKLabel(demo, 0);

	newIpAddress = activateCDKEntry (ipAddressEntry, 0);
	  if (newIpAddress.empty())
	  {
		MessageBox(cdkscreen, "FQDN/IP address is empty");
	  	destroyCDKLabel (demo);
	  	destroyCDKEntry (ipAddressEntry);
	  	destroyCDKEntry (portEntry);
		destroyCDKItemlist (httpsItem);
		destroyCDKEntry (passphraseEntry);
		continue;
	  }
	  newPort = activateCDKEntry (portEntry, 0);

	  bool needsRestart = false;
	  bool validinfo = true;
	  //if (!validateip(newIpAddress))
	  //{
		//// not a valid ip address
		//// print message
		//validinfo = false;
		//MessageBox(cdkscreen, "Error!!! Not a valid IP Address");
	  //}
	  /*
	  unsigned int portnum = strtol(newPort, (char **)NULL, 10);
	  if ( portnum == 0 || errno == EINVAL)
		 validinfo = false;
	  */
	  if (newPort.empty())
	  {
		MessageBox(cdkscreen, "Port number is empty");
	  	destroyCDKLabel (demo);
	  	destroyCDKEntry (ipAddressEntry);
	  	destroyCDKEntry (portEntry);
		destroyCDKItemlist (httpsItem);
		destroyCDKEntry (passphraseEntry);
		continue;
	  }
	  if(!validateport(newPort))
	  {
		// not a valid port number
		// print message
		validinfo = false;
		MessageBox(cdkscreen, "Error!!! Not a valid Port");
	  }
	  //unsigned int newPortint = strtol(str, (char **)NULL, 10);
	  
	  int httpsItemval = activateCDKItemlist (httpsItem, 0);

	  int modifypassphraseItemval;
          bool isPassphraseEditable = true;

	  if ( Passphrase_FileExist ) {
		modifypassphraseItemval = activateCDKItemlist ( Modify_passphraseItem, 0); 
		if(!modifypassphraseItemval) {
                	isPassphraseEditable = false;
			isPassphraseNotModified = true;
		}
			
	  }

	  if ( Passphrase_FileExist ) 
	  {
	  	destroyCDKEntry(Modify_passphraseItem); 
	  } 
	   
	  if (isPassphraseEditable ) 
	  {
	  	newPassphrase = std::string(activateCDKEntry (passphraseEntry, 0));
		
	  	if (newPassphrase.empty())
	  	{			
			MessageBox(cdkscreen, "Passphrase is empty");
			destroyCDKLabel (demo);
			destroyCDKEntry (ipAddressEntry);
			destroyCDKEntry (portEntry);
			destroyCDKItemlist (httpsItem);
			destroyCDKEntry (passphraseEntry);
			continue;
	  	}
	 }
	 else {
		newPassphrase = passphrase;
	 }

	std::string err_msg;
	bool isVerifiedCreatedpassphrase = false;
	if ( (strcmp(config.getHttp().ipAddress, newIpAddress.c_str()) != 0) || (atoi(newPort.c_str()) != config.getHttp().port) || (config.IsHttps()!= httpsItemval) || ( !newPassphrase.empty() && (strcmp(passphrase.c_str(), 
		newPassphrase.c_str()) != 0)))
	{
		if ( validatepassphrase( httpsItemval, newIpAddress, newPort, config.getHostId(), newPassphrase, err_msg, isVerifiedCreatedpassphrase ) )
	  	{
	  		if ( securitylib::writePassphrase(newPassphrase) ) 
			{
				securitylib::secureClearPassphrase(newPassphrase);
				isPassphraseValidated = true;
			}
  	  	}
	  	else 
	  	{
			validinfo = false;
			if ( newPassphrase.empty() )
				validinfo = true;
			if ( isVerifiedCreatedpassphrase )
				PassphraseMessageBox(cdkscreen, const_cast<char*>(err_msg.c_str()));
			else {
				if ( modifypassphraseItemval )
					MessageBox(cdkscreen, const_cast<char*>(err_msg.c_str()));
				else if ( !isPassphraseNotModified )
					MessageBox(cdkscreen, const_cast<char*>(err_msg.c_str()));
			}
 	  	}
	}
         
          bool cxpsConfUpdate = true;
	  if(scoutAgentInstallCode != SCOUT_FX) 
	  {
                bool isMasterTarget = false;
		if (validinfo && (((strcmp(config.getHttp().ipAddress, newIpAddress.c_str()) != 0) || (atoi(newPort.c_str()) != config.getHttp().port) || (config.IsHttps()!= httpsItemval)) || isPassphraseValidated ) )
		{
                    if (strcmp("MasterTarget", config.getAgentRole().c_str()) == 0)
                    {
                       isMasterTarget = true;
                    }
                    if(isMasterTarget)
                    { 
                
                           std::string installpath = config.getInstallPath();
                           std::string confFilePath = installpath;
                           confFilePath += "transport";
                           confFilePath += ACE_DIRECTORY_SEPARATOR_CHAR;
                           confFilePath += "cxps.conf";
                     
                           std::string cmd = installpath + "transport" + ACE_DIRECTORY_SEPARATOR_CHAR + "cxpscli";
                           cmd += std::string(" ") + "--conf" + std::string(" ") + confFilePath;
                           cmd += std::string(" ") + "--csip" + std::string(" ") + newIpAddress;
                    
                           if(httpsItemval)
                           {
                              cmd += std::string(" ") + "--cssecure" + std::string(" ") + "yes";
                              cmd += std::string(" ") + "--cssslport" + std::string(" ") + newPort;
                           }
                           else
                           {
                              cmd += std::string(" ") + "--cssecure" + std::string(" ") + "no";
                              cmd += std::string(" ") + "--csport" + std::string(" ") + newPort;
                           }
                    
                           std::string err = "";
                           if(!execProc(TMPPATH,cmd,err))    
                           {
                              cxpsConfUpdate = false;
                              MessageBox(cdkscreen, const_cast<char*>(err.c_str()));         
                           }
                    }
                    if(cxpsConfUpdate)
                    { 
	  	        config.setHostName(newIpAddress);
	  	        config.setPort(atoi(newPort.c_str()));
		        config.setHttps((bool)httpsItemval);
	                needsRestart = true;
                    }
               }
	  } 
	  if(scoutAgentInstallCode != SCOUT_VX)
	  {
             if(cxpsConfUpdate)
             {
		if (!GetValuesInConfigFile( configFilePath, KEY_SV_SERVER_PORT, portvalueFx))
		{
			fprintf(stderr,"Can't get the port value from config.ini\n" );
			ExitProgram (EXIT_FAILURE);
		}
		if (!GetValuesInConfigFile( configFilePath, KEY_HTTPS, protocolvalueFx))
		{
			if(!WriteKeyValueInConfigFile(KEY_HTTPS, "0"))
			{
				fprintf(stderr,"can't write the keyvalue pair in config.ini\n");
				ExitProgram (EXIT_FAILURE);
			}
		}	
		if (!GetValuesInConfigFile( configFilePath, KEY_SV_SERVER, ipaddressvalueFx))
		{
			fprintf(stderr,"Can't get the ipaddress value from config.ini\n" );
			ExitProgram (EXIT_FAILURE);
		}
		if (validinfo && (((strcmp(ipaddressvalueFx.c_str(), newIpAddress.c_str()) != 0) || (atoi(newPort.c_str()) != (atoi(portvalueFx.c_str())) || atoi(protocolvalueFx.c_str()) != httpsItemval)) || isPassphraseValidated ) )
		{
			if(!SetValuesInConfigFile(KEY_SV_SERVER, newIpAddress))
			{
				fprintf(stderr,"Can't set the ipaddress to config.ini\n");
				ExitProgram (EXIT_FAILURE);
			}
			if(!SetValuesInConfigFile(KEY_SV_SERVER_PORT, newPort))
			{
				fprintf(stderr,"Can't set the portvalue to config.ini\n");
				ExitProgram (EXIT_FAILURE);
			}
			stringstream protocolValue;
			protocolValue << httpsItemval;
			string s = protocolValue.str();
			if(!SetValuesInConfigFile(KEY_HTTPS, s))
			{
				fprintf(stderr,"Can't set the protocolvalue to config.ini\n");
				ExitProgram (EXIT_FAILURE);
			}
			needsRestart = true;
		}
             }
	  }
	
	  destroyCDKLabel (demo);
	  destroyCDKEntry (ipAddressEntry);
	  destroyCDKEntry (portEntry);
	  destroyCDKItemlist (httpsItem);
	  destroyCDKEntry (passphraseEntry);
	

	  if (needsRestart)
	  {
		if (scoutAgentInstallCode != SCOUT_FX)
		{
			std::string VxInstallPath = FilePath;
			VxInstallPath += "bin";
			RestartServices(VxInstallPath);
		}
		if (scoutAgentInstallCode != SCOUT_VX)
		{
			std::string FxInstallpath;
			if (!GetValuesInConfigFile( configFilePath, KEY_CACHE_DIR_PATH, FxInstallpath))
    		    	{
        			fprintf(stderr,"Can't get the Fx installation path from config.ini\n" );
        			ExitProgram (EXIT_FAILURE);
    		    	}
		    	RestartServices(FxInstallpath);
		}
		isPassphraseValidated = false;
	  }
	  refreshCDKScreen (cdkscreen);
	}
      else if (selection == 1)
	{
		
	  CDKLABEL     *demo;
	  CDKENTRY *cacheDirEntry  = 0;
	  CDKLABEL *note;
	  char *message[10];
	  char *mesg[1];
	  char *info[2];
	  std::string newCacheDir;
	  mesg[0] = "</B></U>VX Agent<!U><!B>";
	  demo = newCDKLabel (cdkscreen,
			      getbegx (question->win), getbegy (question->win) + question->boxHeight,	
			      mesg, 
			      1,
			      FALSE, 
			      FALSE);
	  if (demo == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create label-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	
	  cacheDirEntry = newCDKEntry (cdkscreen, getbegx (demo->win), getbegy (demo->win) + demo->boxHeight,
				       "<C>Application Cache Directory: ", NULL, A_NORMAL, '_', vMIXED,
				       50, 0, 256, TRUE, FALSE);

	  if (cacheDirEntry == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create cacheDirEntry-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	
  	  bindCDKObject (vENTRY, cacheDirEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(cacheDirEntry, CDKEntryBackSpaceCB);
	  setCDKEntryValue(cacheDirEntry, (char *)config.getCacheDirectory().c_str());

	  info[0]      = "<C></5>No";
	  info[1]      = "<C></5>Yes";
//	  CDKITEMLIST *registerSystemItem;
//	  registerSystemItem = newCDKItemlist (cdkscreen,
//					       getbegx (cacheDirEntry->win), getbegy (cacheDirEntry->win) + cacheDirEntry->boxHeight,
//					       "System and Cache Volumes Registration: ", // NULL title,
//					       NULL, //"System and Cache Volumes Registration: ",
//					       info,
//					       2, 
//					       config.getRegisterSystemDrive()?1:0,  
//					       TRUE, 
//					       FALSE);
//
//	  if (registerSystemItem == 0)
//	    {
//	      destroyCDKScreen (cdkscreen);
//	      endCDK();
//	      
//	      fprintf(stderr, "Cannot create registerSystemItem-widget\n");
//	      ExitProgram (EXIT_FAILURE);
//	    }

      message[0] = "</B></32>Note: Changing cache directory requires the following steps.<!32><!B>";
      message[1] = "<C></B></32>Not following these steps can result into data loss.<!32><!B>";
      message[2] = "";
      message[3] = "</B></32>1. Stop svagents<!32><!B>";
      message[4] = "</B></32>2. Wait for svagent and child processes to stop completely<!32><!B>";
      message[5] = "</B></32>3. Create the new cache directory<!32><!B>";
      message[6] = "</B></32>4. Move contents from old cache directory to new cache directory<!32><!B>";
      message[7] = "</B></32>5. change cache directory using hostconfigcli/hostconfigui<!32><!B>";
      message[8] = "</B></32>6. start svagents<!32><!B>";
	  note = newCDKLabel (cdkscreen,
                  getbegx (cacheDirEntry->win), getbegy (cacheDirEntry->win) + cacheDirEntry->boxHeight,
			      message, 
			      9,
			      TRUE, 
			      FALSE);
	  if (note == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create note-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	
	  drawCDKLabel(demo, ObjOf(demo)->box);
	  drawCDKEntry (cacheDirEntry,    ObjOf(cacheDirEntry)->box);
//	  drawCDKItemlist(registerSystemItem, ObjOf(registerSystemItem)->box);
	  drawCDKLabel(note, ObjOf(note)->box);

	  newCacheDir = activateCDKEntry (cacheDirEntry, 0);
	  if (newCacheDir.empty())
	  {
		MessageBox(cdkscreen, "Cache directory path is empty");
	  	destroyCDKLabel (demo);
	  	destroyCDKEntry (cacheDirEntry);
//	  	destroyCDKItemlist (registerSystemItem);
		destroyCDKLabel (note);
		continue;
	  }
	  config.setCacheDirectory (newCacheDir);
      config.setDiffTargetCacheDirectoryPrefix(newCacheDir);
//	  if (activateCDKItemlist (registerSystemItem, 0) == 0)
//	    config.setRegisterSystemDrive(false);
//	  else
//	    config.setRegisterSystemDrive(true);

	  destroyCDKLabel (demo);
	  destroyCDKEntry (cacheDirEntry);
//	  destroyCDKItemlist (registerSystemItem);
	  destroyCDKLabel (note);
	}
      else if (selection == 2)
	{
	  CDKLABEL     *demo;
	  char *info[2];
	  char         *mesg[1];
	  mesg[0] = "</B></U>NAT Hostname /IP address configuration<!U><!B>";
	  demo = newCDKLabel (cdkscreen,
			      getbegx (question->win), getbegy (question->win) + question->boxHeight,	
			      mesg, 1,
			      FALSE, 
			      FALSE);
	  info[0]      = "<C></5>No";
	  info[1]      = "<C></5>Yes";
	  CDKITEMLIST *naTHostItem;
	  naTHostItem = newCDKItemlist (cdkscreen,
					    getbegx (demo->win), getbegy (demo->win) + demo->boxHeight,
					    "Enable Fixed NAT Hostname: ",
					    NULL,
					    info,
					    2, 
					    config.getUseConfiguredHostname(),
					    TRUE, 
					    FALSE);
	  if (naTHostItem == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create naTHostItem-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	  CDKITEMLIST *naTIPItem;
	  naTIPItem = newCDKItemlist (cdkscreen,
					  getbegx (naTHostItem->win), getbegy (naTHostItem->win) + naTHostItem->boxHeight,
					  "Enable Fixed NAT IP Address: ",
					  NULL,
					  info,
					  2, 
					  config.getUseConfiguredIpAddress(),
					  TRUE, 
					  FALSE);
	  if (naTIPItem == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create naTIPItem-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	  CDKENTRY *naTHostEntry  = 0;
	  CDKENTRY *naTIPEntry = 0;
	  naTHostEntry  = newCDKEntry (cdkscreen, getbegx (naTHostItem->win) + naTHostItem->boxWidth + 2, getbegy (naTHostItem->win),
					 "<C>Enter NAT Hostname", "", A_NORMAL, '_', vMIXED,
					 20, 0, 15, TRUE, FALSE);
	  naTIPEntry = newCDKEntry (cdkscreen, getbegx (naTHostEntry->win), getbegy (naTHostEntry->win) + naTHostEntry->boxHeight  ,
				   "<C>Enter NAT IP Address", "", A_NORMAL, '_', vMIXED,
				   20, 0, 15, TRUE, FALSE);
  	  bindCDKObject (vENTRY, naTHostEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(naTHostEntry, CDKEntryBackSpaceCB);

  	  bindCDKObject (vENTRY, naTIPEntry, CODE_BACKSPACE, onBackSpaceCB, 0);
	  setCDKEntryCB(naTIPEntry, CDKEntryBackSpaceCB);

	  setCDKEntryValue(naTHostEntry, (char *)config.getConfiguredHostname().c_str());
	  setCDKEntryValue(naTIPEntry, (char *)config.getConfiguredIpAddress().c_str());
	  drawCDKLabel(demo, ObjOf(demo)->box);
	  drawCDKItemlist(naTHostItem, ObjOf(naTHostItem)->box);
	  drawCDKItemlist(naTIPItem, ObjOf(naTIPItem)->box);
	  drawCDKEntry (naTHostEntry,    ObjOf(naTHostEntry)->box);
	  drawCDKEntry (naTIPEntry,    ObjOf(naTIPEntry)->box);

	  activateCDKLabel(demo, 0);
	  int naTHostItemval = activateCDKItemlist (naTHostItem, 0);
	  int naTIPItemval = activateCDKItemlist (naTIPItem, 0);
	  std::string newnatHost = activateCDKEntry (naTHostEntry, 0);
	  if (newnatHost.empty())
	  {
		MessageBox(cdkscreen, "Nat host name is empty");
	  	destroyCDKLabel (demo);
	  	destroyCDKItemlist (naTHostItem);
	  	destroyCDKItemlist (naTIPItem); 
	  	destroyCDKEntry (naTHostEntry);
	  	destroyCDKEntry (naTIPEntry);
		continue;
	  }  
	  std::string newnatIp = activateCDKEntry (naTIPEntry, 0);
	  if (newnatIp.empty())
	  {
		MessageBox(cdkscreen, "Nat IP address is empty");
	  	destroyCDKLabel (demo);
	  	destroyCDKItemlist (naTHostItem);
	  	destroyCDKItemlist (naTIPItem); 
	  	destroyCDKEntry (naTHostEntry);
	  	destroyCDKEntry (naTIPEntry);
		continue;
	  }  
	  
	  config.setUseConfiguredHostname((bool)naTHostItemval);
	  config.setUseConfiguredIpAddress((bool)naTIPItemval);
	  config.setConfiguredHostname(newnatHost);
	  config.setConfiguredIpAddress(newnatIp);
	  destroyCDKLabel (demo);
	  destroyCDKItemlist (naTHostItem);
	  destroyCDKItemlist (naTIPItem); 
	  destroyCDKEntry (naTHostEntry);
	  destroyCDKEntry (naTIPEntry);
	}
      else if (selection == 3)
	{
	  CDKLABEL     *demo;
	  char *info[2];
	  char         *mesg[1];
	  mesg[0] = "</B></U>Logging Options<!U><!B>";
	  demo = newCDKLabel (cdkscreen,
			      getbegx (question->win), getbegy (question->win) + question->boxHeight,	
			      mesg, 
			      1,
			      FALSE, 
			      FALSE);

	  CDKSCALE *remoteLogLevelScale = 0;
	  remoteLogLevelScale = newCDKScale (cdkscreen,
					      getbegx (demo->win), getbegy (demo->win) + demo->boxHeight,
				       NULL, // NULL title 
				       "Remote Log Level: ",
				       A_NORMAL,
				       5,
				       (int) config.getRemoteLogLevel(), // Current value of log level
				       0, //low value
				       3, //high value
				       1, //increment 
				       1*2, // fast increment
				       TRUE,
				       FALSE);
	  if (remoteLogLevelScale == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create remoteLogLevelScale-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }

	  CDKSCALE *logLevelScale = 0;
	  logLevelScale = newCDKScale (cdkscreen,
				       getbegx (remoteLogLevelScale->win) + remoteLogLevelScale->boxWidth + 1, getbegy (remoteLogLevelScale->win),
				       NULL, // NULL title 
				       "Log Level: ",
				       A_NORMAL,
				       5,
				       (int) config.getLogLevel(), // Current value of log level
				       0, //low value
				       7, //high value
				       1, //increment 
				       1*2, // fast increment
				       TRUE,
				       FALSE);
	  if (logLevelScale == 0)
	    {
	      destroyCDKScreen (cdkscreen);
	      endCDK();
	      
	      fprintf(stderr, "Cannot create logLevelScale-widget\n");
	      ExitProgram (EXIT_FAILURE);
	    }
	  
	  drawCDKLabel(demo, ObjOf(demo)->box);
	  drawCDKItemlist(remoteLogLevelScale, ObjOf(remoteLogLevelScale)->box);
	  drawCDKScale(logLevelScale, ObjOf(logLevelScale)->box);

	  activateCDKLabel(demo, 0);
	  int remoteLogLevel = activateCDKScale (remoteLogLevelScale, 0);
	  config.setRemoteLogLevel ((SV_LOG_LEVEL) remoteLogLevel);
	  int loglevel = activateCDKScale(logLevelScale, 0); 
	  if ( scoutAgentInstallCode != SCOUT_VX )
	  {
		std::string loglevelFx;
		
		if (!GetValuesInConfigFile( configFilePath, KEY_LOG_LEVEL, loglevelFx))
		{
			fprintf(stderr,"Can't get the log level value from config.ini\n" );
			ExitProgram (EXIT_FAILURE);
		}
		stringstream localLogvalueFx;
		localLogvalueFx << loglevel;
		string s = localLogvalueFx.str(); 
		if (!SetValuesInConfigFile(KEY_LOG_LEVEL, s))
		{
			fprintf(stderr,"Can't set the log level value to config.ini\n" );
			ExitProgram (EXIT_FAILURE);
		}
	  }
	
	  if ( scoutAgentInstallCode != SCOUT_FX )
	  	config.setLogLevel ((SV_LOG_LEVEL) loglevel);

	  destroyCDKLabel (demo);
	  destroyCDKScale (remoteLogLevelScale);
	  destroyCDKScale (logLevelScale);
	}
    } //end while

  /* Clean up. */
  destroyCDKDialog (question);
  destroyCDKScreen (cdkscreen);
  endCDK ();
  ExitProgram (EXIT_SUCCESS);
} //end try
catch ( ContextualException& ce )
{
	MessageBox(cdkscreen, (char*)ce.what());
}
catch( std::exception const& e )
{
	MessageBox(cdkscreen, (char*)e.what());
}
catch ( ... )
{
	MessageBox(cdkscreen, "Encountered unknown exception");
}
return 0;
}

#ifdef SV_UNIX
void RestartServices(std::string installPath)
{	
  stringstream msg;
  ACE_Process_Manager* pm = ACE_Process_Manager::instance();
  ACE_Process_Options optionsStop;
  ACE_Process_Options optionsStart;
  ACE_Time_Value waitTime(ACE_OS::gettimeofday());
  ACE_exitcode status = 0;
  pid_t pid, rc;
  std::string statusPath = installPath + "/status";
  std::string stopPath = installPath  + "/stop" + " > /dev/null 2>&1 ";
  std::string startPath = installPath + "/start";
  std::string svagentPid = "ps -ef | grep \"/svagents\" | grep -v grep | awk \'{print $2}\'";
  string rsInfo;
  DebugPrintf(SV_LOG_DEBUG,"Status utility Path  %s\n",statusPath.c_str());
  DebugPrintf(SV_LOG_DEBUG,"Stop utility Path  %s\n",stopPath.c_str());
  DebugPrintf(SV_LOG_DEBUG,"Start utility Path  %s\n",startPath.c_str());
  
  GetProcessOutput(statusPath, rsInfo);
  if (rsInfo.find("not") != std::string::npos) {
    // No need to restart when service is down
    return;
  }
  // To Stop the service
  DebugPrintf (SV_LOG_DEBUG, "stopPath = %s\n", stopPath.c_str());
  optionsStop.command_line(ACE_TEXT("%s"), stopPath.c_str());
  pid = pm->spawn(optionsStop);
  if (ACE_INVALID_PID == pid) {
    msg << "post script failed with error no:" << ACE_OS::last_error() << '\n';
    DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
    return;
  }
  //waitTime(ACE_OS::gettimeofday());
  waitTime.sec(waitTime.sec() + 5);

  rc = ACE_Process_Manager::instance ()->wait(pid, waitTime, &status);
  if (ACE_INVALID_PID == rc) {
    msg << "script failed in ACE_Process_Manager::wait with error no: " 
	<< ACE_OS::last_error() << '\n';
    DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
  } 
  else if (rc == pid) {
    if (0 != status) {
      msg << "script failed with exit status " << status << '\n';
      DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
    }	
  }

  // To Restart the service
  DebugPrintf (SV_LOG_DEBUG, "startPath = %s\n", startPath.c_str());
	
  optionsStart.command_line(ACE_TEXT("%s"), startPath.c_str());
  pid = pm->spawn(optionsStart);
  if (ACE_INVALID_PID == pid) {
    msg << "post script failed with error no:" << ACE_OS::last_error() << '\n';
    DebugPrintf (SV_LOG_DEBUG, "%s", msg.str().c_str());
    return;
  }
  waitTime = ACE_Time_Value(ACE_OS::gettimeofday());
  waitTime.sec(waitTime.sec() + 5);
  status = 0;
  rc = ACE_Process_Manager::instance ()->wait(pid, waitTime, &status);
  if (ACE_INVALID_PID == rc) {
    msg << "script failed in ACE_Process_Manager::wait with error no: " 
	<< ACE_OS::last_error() << '\n';
    DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
  } 
  else if (rc == pid) {
    if (0 != status) 
      {
	msg << "script failed with exit status " << status << '\n';
	DebugPrintf (SV_LOG_DEBUG, "%s", msg.str().c_str());
      }

  }
}
#else

void RestartServices(std::string installPath)
{
}
#endif
int main(int argc, char **argv)
{
  return interface(argc, argv);

}

