#ifndef __MDDRIVER_H__
#define __MDDRIVER_H__
/*
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/fcntl.h>
*/
#include "libinclude.h"
#include "global.h"

/*
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <assert.h>
#include <asm/fcntl.h>
#include "global.h"
*/
#define TEST_BLOCK_LEN 1000
#define TEST_BLOCK_COUNT 1000

void MDString(unsigned char *, int);
void MDTimeTrial(void);
void MDTestSuite(void);
void MDFile(char *);
void MDFilter(void);
void MDPrint(unsigned char [16]);

#endif
