// inmtouch.cpp : Defines the entry point for the console application.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

#define TOUCH_SUCCESS 0
#define TOUCH_FAIL 9
#define DEFAULT_TIMEOUT 10

int main (int argc, char *argv[])
{
	//calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	
	char *file;
    int timeout = 0, blocks = 0, read_write = 0, touch_ret = TOUCH_FAIL;
#ifdef WIN32
	int isChild = 0 ;
#endif
	
    if (argc == 2)
    {
        file = argv[1];
        timeout = DEFAULT_TIMEOUT;
    }
    else if (argc == 3) 
    {
        file = argv[1];
		#ifdef WIN32
		if( strcmp( argv[2], "-child" ) == 0 )
		{
			isChild = 1 ;
			timeout = DEFAULT_TIMEOUT;
		}
		else
		#endif
			timeout = atoi (argv[2]);
    }
	#ifdef WIN32
	else if ( ( argc == 4 ) && ( strcmp( argv[3], "-child" ) == 0 ) )  
    {
		file = argv[1];
        timeout = atoi (argv[2]);		
		isChild = 1 ;
		
	}
	#endif
    else 
    {
        printf ("Usage: inmtouch filename [timeout (seconds)]\n");
        exit (1);
    }

	#ifdef WIN32
	DWORD open_ret = 0 ;
	if( isChild == 1 )
	{
		char data[8193];
		int i = 0;

		for (i=0; i<8193; i++)
		{
			data[i] = 'A';
		}
		
		HANDLE tempFile = CreateFile( file, 
													GENERIC_READ | GENERIC_WRITE, 
													0, 0, 
													CREATE_ALWAYS,
													FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
													0 ) ;
		if( tempFile != INVALID_HANDLE_VALUE )
		{
			DWORD dwBytesWritten = 0 ;
			if( ::WriteFile( tempFile, data, 8193, &dwBytesWritten, 0 ) == 0 ) 
				open_ret = GetLastError() ;
			
			CloseHandle( tempFile ) ;
		}
		else
			open_ret = GetLastError() ;		
	}
	else
	{
		size_t nLength = 0 ;
		for( int index = 0; 
			index < argc; 
			index++ )
		{
			INM_SAFE_ARITHMETIC(nLength += InmSafeInt<size_t>::Type(strlen( argv[ index ] ))  + 1, INMAGE_EX(nLength)(strlen( argv[ index ] )))
		}
		
		INM_SAFE_ARITHMETIC(nLength += InmSafeInt<size_t>::Type(strlen( "-child" )) + 1, INMAGE_EX(nLength)(strlen( "-child" )))
		char *cmdLine = new char[ nLength ] ;
		ZeroMemory( cmdLine, nLength ) ;
		
		for( int index = 0; 
			index < argc; 
			index++ )
		{
			inm_strncat_s( cmdLine, nLength,argv[index], strlen( argv[index] ) ) ;
			inm_strcat_s( cmdLine,nLength, " " ) ;		
		}

		inm_strncat_s( cmdLine,nLength, "-child", strlen( "-child" ) ) ;

		STARTUPINFO si;
		PROCESS_INFORMATION pi;
	
		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );

		// Start the child process. 
		if( !CreateProcess( NULL,   // No module name (use command line)
								cmdLine,      // Command line
								NULL,           // Process handle not inheritable
								NULL,           // Thread handle not inheritable
								FALSE,          // Set handle inheritance to FALSE
								0,              // No creation flags
								NULL,           // Use parent's environment block
								NULL,           // Use parent's starting directory 
								&si,            // Pointer to STARTUPINFO structure
								&pi )           // Pointer to PROCESS_INFORMATION structure
								) 
		{
			open_ret = GetLastError() ;        
		}
		else
		{
			WaitForSingleObject( pi.hProcess, timeout );
			::GetExitCodeProcess( pi.hProcess, &open_ret ) ;

			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );			
		}
	}
	return open_ret ;
	#else
	int fork_return = fork() ;
	switch( fork_return )
	{
		case 0 :
			{
				int open_ret = -1 ;
				char data[8193];
				int i = 0;

				for (i=0; i<8193; i++)
				{
					data[i] = 'A';
				}
				
					open_ret = open (file, O_WRONLY|O_NONBLOCK|O_CREAT|O_NOCTTY, 0666);
					
					utime (file, NULL);
					unlink( file ) ;
					if (open_ret > 0 )
					{
						int write_fd = open_ret;
						open_ret = write (write_fd, data, sizeof(data));
						if (open_ret > 0 )
						{
							open_ret = 0;
						}
						close (write_fd);
					}
				exit (open_ret) ;
			}
			break ;
		
		case -1:			
			exit(-1) ;			
		default:
			//parent here waits for the child to close or timeout...
			{
				int child_pid = 0 ;
				int count = 0 ;
				int status = 0 ;

				while( child_pid == 0 && count < timeout )
				{
					sleep( 1 ) ;
					count++ ;
					child_pid = waitpid( fork_return, 
											   &status,
											   WNOHANG ) ;					
				}
				if (child_pid > 0)
				{
					if (WIFEXITED (status))
					{
						touch_ret = WEXITSTATUS (status);
					}
				}
				else            
				{
					kill (fork_return, SIGINT);
				}

				exit (touch_ret);
			}
			break ;
	}
	#endif	
}
/*int main (int argc, char *argv[])
{
    char *file;
    int timeout = 0, blocks = 0, read_write = 0, touch_ret = TOUCH_FAIL;

    if (argc == 2)
    {
        file = argv[1];
        timeout = DEFAULT_TIMEOUT;
    }
    else if (argc == 3) 
    {
        file = argv[1];
        timeout = atoi (argv[2]);
    }
    else
    {
        printf ("Usage: touch filename [timeout (seconds)]\n");
        exit (1);
    }
   
    if (timeout)
    {
        int fork_ret;

        fork_ret = fork ();
        if (fork_ret == -1)
        {
            exit (1);
        }

        if (fork_ret > 0)
        {
            pid_t pid = 0;
            int count = 0;
            int status = 0;

            while (pid == 0 && count < timeout)
            {
                sleep (1);
                count++;
                pid = waitpid (fork_ret, &status, WNOHANG);
            }

            if (pid > 0)
            {
                if (WIFEXITED (status))
                {
                    touch_ret = WEXITSTATUS (status);
                }
            }
            else            
            {
                kill (fork_ret, SIGINT);
            }

            exit (touch_ret);
        }

        else
        {
            int open_ret = open (file, O_WRONLY|O_NONBLOCK|O_CREAT|O_NOCTTY, 0666);
            char data[8193];
            int i = 0;

            for (i=0; i<8193; i++)
            {
                data[i] = 'A';
            }
            //utime (file, NULL);
			unlink( file ) ;
            if (open_ret > 0 )
            {
                int write_fd = open_ret;
                open_ret = write (write_fd, data, sizeof(data));
                if (open_ret > 0 )
                {
                    open_ret = 0;
                }
                close (write_fd);
            }
            exit (open_ret);
        }
    }
}
int child_pid = 0 ;
				int count = 0 ;
				ACE_exitcode *status = NULL ;

				while( child_pid == 0 && count < timeout )
				{
					child_pid = ACE_OS::waitpid( fork_return, 
															status,
															WNOHANG,
															0 ) ;
					ACE_OS::sleep( 1 ) ;
					count++ ;
				}
				if (child_pid > 0)
				{
					if (WIFEXITED (status))
					{
						touch_ret = WEXITSTATUS (*status);
					}
				}
				else            
				{
					ACE_OS::kill (fork_return, SIGINT);
				}

				exit (touch_ret);*/
