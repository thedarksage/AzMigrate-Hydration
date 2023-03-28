#include <stdio.h>
#include <sys/types.h>
#include <string>
using namespace std;
#include <errno.h>
#include "svtypes.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "svutils.h"

void trim(string& s, string stuff_to_trim = " \n\b\t\a\r\xc") 
{

  int lInmend = s.size() - 1, lInmstart = 0;

  lInmend = s.find_last_not_of(stuff_to_trim);
  if (lInmend == string::npos) lInmend = s.size() - 1;

  lInmstart = s.find_first_not_of(stuff_to_trim);
  if (lInmstart == string::npos) lInmstart = 0;

  s = s.substr(lInmstart, lInmend - lInmstart + 1);
}


SVERROR GetProcessOutput(const string& cmd, string& output)
{
    FILE *in;
    char lInmbuff[512];

    /* popen creates a pipe so we can read the output
     of the program we are invoking */
    if (!(in = popen(cmd.c_str(), "r")))
    {
        return SVS_FALSE;
    }

    /* read the output of command , one line at a time */
    while (fgets(lInmbuff, sizeof(lInmbuff), in) != NULL )
    {
        output += lInmbuff;
    }
    trim(output);
    /* close the pipe */
    pclose(in);
    return SVS_OK;
}

