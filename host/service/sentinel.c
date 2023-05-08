#include <stdio.h>
main()
{
int i = 0;
for(;;)
{
 printf(" sentinel i = %d , sleeping 10 sec.\n", i++);
 sleep(10);
}
}
