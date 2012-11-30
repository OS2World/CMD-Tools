#include        <stdio.h> 

#include        <windows.h>

int	usage(void)
{
    printf("SYS - Windows error code retriever for WIN32, V1.00\n\n");
    printf("      (C) Roman Stangl 09, 2001 (Roman_Stangl@at.ibm.com)\n");
    printf("      http://www.geocities.com/SiliconValley/Pines/7885/\n\n");
    printf("Syntax: SYS x\n");
    printf("Where:      x ... Numeric Windows system error code\n");
    printf("\n");
    return(NO_ERROR);
}

int     main(int argc, char *argv[])
{
    LPVOID	lpMessageBuffer;
    int		iIndex;
    int		iErrorCode=NO_ERROR;

    if(argc==1)
        {
	usage();
	return(1);
	}
    for(iIndex=0; ; iIndex++)
	{
	char	cChar;

	cChar=argv[1][iIndex];
	if(cChar=='\0')
	    break;
	if((cChar<'0') || (cChar>'9'))
	    {
            usage();
	    return(2);
	    }
	}
    iErrorCode=atoi(argv[1]);
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        iErrorCode, 
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMessageBuffer,
        0,
        NULL);
    if(lpMessageBuffer!=NULL)
        printf("SYS%d: %s", iErrorCode, lpMessageBuffer);
    else	
        printf("SYS%d: < No help test available>\n", iErrorCode);
    LocalFree(lpMessageBuffer);
    return(NO_ERROR); 
}

