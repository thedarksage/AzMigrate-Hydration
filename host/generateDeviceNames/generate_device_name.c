#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "inmsafecapis.h"

char disk_name[256];
unsigned long int index_var=0;

int main(int argc, char *argv[])
{
    if (argc < 2 ) {
    	return 1;
    }
    //printf("\n arv0:%s arg1:%s \n",argv[0], argv[1]);
    index_var = atoi( argv[1] );
    //printf("\nindex_var:%lu\n", index_var);
    memset(&disk_name,0,256);

    if (index_var < 26) {
		inm_sprintf_s(disk_name, ARRAYSIZE(disk_name), "sd%c", 'a' + (int)index_var % 26);
    } else if (index_var < (26 + 1) * 26) {
		inm_sprintf_s(disk_name, ARRAYSIZE(disk_name), "sd%c%c",
            'a' + index_var / 26 - 1,'a' + (int)index_var % 26);
    } else {
        const unsigned int m1 = ((int)index_var / 26 - 1) / 26 - 1;
        const unsigned int m2 = ((int)index_var / 26 - 1) % 26;
        const unsigned int m3 =  (int)index_var % 26;
		inm_sprintf_s(disk_name, ARRAYSIZE(disk_name), "sd%c%c%c",
            'a' + m1, 'a' + m2, 'a' + m3);
    }
    printf("%s", disk_name);
    return 0;

}
