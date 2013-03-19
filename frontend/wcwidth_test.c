#include <stdio.h>
#include <wchar.h>
 
int   main(void)
{
	
    char*   wc   =   "哦我";

 
    printf("   has   a   width   of   %d\n",     wcswidth(wc));
    return   0;
 
   /**************************************************************************
    The   output   should   be   similar   to   :
    A   has   a   width   of   1
    **************************************************************************/
 
}
