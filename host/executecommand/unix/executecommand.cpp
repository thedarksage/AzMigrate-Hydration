//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : executecommand.cpp
//

#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

#include "executecommand.h"
#include "logger.h"
#include "portable.h"

#include <boost/thread.hpp>
boost::mutex g_MutexForkProcess;
#define READ 0
#define WRITE 1
#define READ_SUCCESS 0
#define READ_EAGAIN -1
#define READ_ERROR -2

static const char* gBinPath = "/bin/sh";

/**
 * Set the `FD_CLOEXEC' flag of DESC if VALUE is nonzero,
 * or clear the flag if VALUE is 0.
 *
 * @returns 0 on success, or -1 on error.
 **/
int setCloexecFlag (const int desc, const bool value)
{
  int oldflags = fcntl (desc, F_GETFD, 0);
  /* If reading the flags failed, return error indication now. */
  if (oldflags == -1)
  {
    return oldflags;
  }
  /* Set just the flag we want to set. */
  if (value)
  {
    oldflags |= FD_CLOEXEC;
  }
  else
  {
    oldflags &= ~FD_CLOEXEC;
  }

  /* Store modified flag word in the descriptor. */
  return fcntl (desc, F_SETFD, oldflags);
}

/* reads from read fd into results 
 * returns READ_EAGAIN if errno is EAGAIN
 * returns READ_ERROR if read fails for any other error
 * returns READ_SUCCESS upon successful read till EOF
 */
int readFromPipe(const int pipefdRead, std::stringstream & results)
{
  int bytesRead = 0;
  char buffer[512];
  do
  {
    bytesRead = read(pipefdRead, buffer, sizeof(buffer));
    if (bytesRead == -1)
    {
      if (EAGAIN == errno || EINTR == errno)
      {
        break; /// go to select
      }
      else
      {
        DebugPrintf(SV_LOG_ERROR, "executePipe read failed, error no %d\n", errno);
        bytesRead = READ_ERROR;
        break; /// Return if can't read
      }
    }
    if (bytesRead)
    {
      results.write(buffer, bytesRead);
    }
  } while (bytesRead);
  return bytesRead;
}


/// Reads from the given fd using select. Select timeout is used to log delays to cx. 
void SelectNRead(const int pipefdRead, std::stringstream & results, std::string const & command)
{
  fd_set rfds, exfds;
  struct timeval tv;
  int retval = -1;
  int bytesRead = 0;
  const int WAITTIME = 300;
  bool b_ReadyToReadFromPipe = false;

  do
  {
    FD_ZERO(&rfds);
    FD_ZERO(&exfds);
    FD_SET(pipefdRead, &rfds);
    FD_SET(pipefdRead, &exfds);
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = WAITTIME;
    tv.tv_usec = 0;
    retval = select(pipefdRead + 1, &rfds, NULL, &exfds, &tv);
    if (retval == -1)
    {
      if (EINTR == errno)
      {
        DebugPrintf(SV_LOG_DEBUG,"executePipe select interrupted\n");
        continue;
      }
      else
      {
        DebugPrintf(SV_LOG_ERROR,"select failed, errno %d, command %s\n", errno,
          command.c_str());
        return;
      }
    }
    else if (retval == 0)
    {
      DebugPrintf(SV_LOG_ERROR, "executePipe %s not returned for %d minutes, fd %d.\n", 
        command.c_str(), WAITTIME/60, pipefdRead);
    }
    else if (FD_ISSET(pipefdRead, &rfds))
    {
      b_ReadyToReadFromPipe = true;
    }
    else if (FD_ISSET(pipefdRead, &exfds))
    {
      DebugPrintf(SV_LOG_ERROR, "executePipe exception set for fd %d\n", pipefdRead);
      break;
    }
    else
    {
      /* impossible case: record error */
      DebugPrintf(SV_LOG_ERROR, "select returned %d but fd is not ready: command %s\n",
        retval, command.c_str());
        break;
    }

    if (b_ReadyToReadFromPipe)
    {
      b_ReadyToReadFromPipe = false;
      bytesRead = readFromPipe(pipefdRead, results);
      if (bytesRead == READ_SUCCESS)
      {
        break;/// Read completely, so break out of select and return
      }
      else if (bytesRead == READ_ERROR)
      {
        break; /// read error, return
      }
      /* Default is to continue select() call as there is data to read */
    }
  } while(true);
}

bool executePipe(std::string const & command, std::stringstream & results)
{
  DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s \n",FUNCTION_NAME, command.c_str());
  // note: this is unix only
  int pipefd[2];
  pid_t chpid;
  do {/// do while controls the scoped_lock duration
    boost::mutex::scoped_lock guard(g_MutexForkProcess);
    if (pipe(pipefd) != 0)
    {
      results << "pipe failed, errno " << errno << '\n';
      return false;
    }
  
    chpid = fork();
  
    if (-1 == chpid)
    {
      /// fork error
      close(pipefd[READ]);
      close(pipefd[WRITE]);
      DebugPrintf(SV_LOG_ERROR, "fork failed, errno %d \n", errno);
      return false;
    }
    else if (0 == chpid)
    {
      /// child process
      close(pipefd[READ]);
      dup2(pipefd[WRITE], STDOUT_FILENO);
      close(pipefd[WRITE]);
      close(STDERR_FILENO);
      close(STDIN_FILENO);
      execl(gBinPath, "sh", "-c", command.c_str(), NULL);
      DebugPrintf(SV_LOG_ERROR, "exec failed, errno %d \n", errno);
      _exit(1);
    }
    else
    {
      /// parent process
      /// Close write end of pipe as parent is interested ONLY in read end
      close(pipefd[WRITE]);
    
      /// pipefd shouldn't be inherited by other children of svagents
      setCloexecFlag(pipefd[READ], 1);
    }
  } while (0);/// Unlock the scoped_lock

  SelectNRead(pipefd[READ], results, command);
  close(pipefd[READ]);

  /* Wait for the child*/
  int chstatus;
  pid_t wpid = waitpid(chpid, &chstatus, 0);

  // reset for user to read from begining
  results.clear();
  results.seekg(0, std::ios::beg);
  DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, command.c_str());
  return true;
}

